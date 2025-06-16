#include "pch.h"
#include "Model.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
void Model::Draw(vk::raii::CommandBuffer& commandBuffer)
{

	vk::Buffer vertexBuffers[] = { vertexBuffer.buffer };
	vk::DeviceSize offsets[] = { 0 };



	//commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
	//commandBuffer.bindIndexBuffer(indexBuffers[LOD].buffer, 0, vk::IndexType::eUint32);

	//commandBuffer.drawIndexed(static_cast<uint32_t>(indices[LOD].size()), 1, 0, 0, 0);

	static const int local_size_x = 1;
	static const int meshletCount = clusters.size() - LODOffset[MAX_LOD - 1];
	static const int instancingCount = 100;
	
	commandBuffer.drawMeshTasksEXT(meshletCount * instancingCount / local_size_x,1,1);
}
void Model::loadFile(std::string path)
{

	tinygltf::Model model;
	tinygltf::TinyGLTF loader;
	std::string err;
	std::string warn;
	std::string lower = path;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

	bool ret = false;
	if (lower.size() >= 5 && lower.substr(lower.size() - 5) == ".gltf") {
		ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
	}
	else if (lower.size() >= 4 && lower.substr(lower.size() - 4) == ".glb") {
		ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
	}


	if (!warn.empty())
		std::cout << "Tiny Warning ! : " << warn << std::endl;

	if (!err.empty())
		std::cout << "Tiny Error ! :" << err << std::endl;

	if (!ret)
		throw std::runtime_error("Failed to Load GLTF!");
	for (const auto& mesh : model.meshes) {

		for (const auto& primitive : mesh.primitives) {
			std::vector<uint32_t> index;
			//Index
			{
				if (primitive.indices >= 0) {
					const auto& accessor = model.accessors[primitive.indices];
					const auto& bufferView = model.bufferViews[accessor.bufferView];
					const auto& buffer = model.buffers[bufferView.buffer];

					const uint8_t* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;
					size_t count = accessor.count;

					if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT) {
						const uint16_t* buf = reinterpret_cast<const uint16_t*>(dataPtr);
						for (size_t i = 0; i < count; ++i)
							index.push_back(static_cast<uint32_t>(buf[i]));
					}
					else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
						const uint32_t* buf = reinterpret_cast<const uint32_t*>(dataPtr);
						for (size_t i = 0; i < count; ++i)
							index.push_back(buf[i]);
					}
				}
			}

			//Vertex
			std::vector<glm::vec3> positions, normals;
			std::vector<glm::vec2> texcoords;
			{
				for (const auto& attr : primitive.attributes) {
					const std::string& attrName = attr.first;
					const auto& accessor = model.accessors[attr.second];
					const auto& bufferView = model.bufferViews[accessor.bufferView];
					const auto& buffer = model.buffers[bufferView.buffer];

					const uint8_t* dataPtr = buffer.data.data() + bufferView.byteOffset + accessor.byteOffset;

					if (attrName == "POSITION") {
						for (size_t i = 0; i < accessor.count; ++i) {
							const float* elem = reinterpret_cast<const float*>(dataPtr + i * accessor.ByteStride(bufferView));
							positions.emplace_back(elem[0], elem[1], elem[2]);
						}
					}
					else if (attrName == "NORMAL") {
						for (size_t i = 0; i < accessor.count; ++i) {
							const float* elem = reinterpret_cast<const float*>(dataPtr + i * accessor.ByteStride(bufferView));
							normals.emplace_back(elem[0], elem[1], elem[2]);
						}
					}
					else if (attrName == "TEXCOORD_0") {
						for (size_t i = 0; i < accessor.count; ++i) {
							const float* elem = reinterpret_cast<const float*>(dataPtr + i * accessor.ByteStride(bufferView));
							texcoords.emplace_back(elem[0], elem[1]);
						}
					}
				}

				// 3. Combine attributes into Vertex struct
				for (size_t i = 0; i < positions.size(); ++i) {
					Vertex v{};
					v.position = positions[i];
					v.normal = i < normals.size() ? normals[i] : glm::vec3(0.0f);
					glm::vec2 texCoord = i < texcoords.size() ? texcoords[i] : glm::vec2(0.0f);
					v.texCoordX = texCoord.r;
					v.texCoordY = texCoord.g;

					vertices.push_back(v);
				}
			}

			indices = (index);
		}

	}

}

void Model::initBuffer()
{

	//init Buffer
	vertexBuffer = generateGPUBuffer(vertices,app->context);

	//Index
	indexBuffer = generateGPUBuffer(indices, app->context);

	//meshlet_Vertices
	meshlet_vertices_Buffer = generateGPUBuffer(meshlet_vertices, app->context);
	
	//meshlet_Triangles
	meshlet_triangles_Buffer = generateGPUBuffer(meshlet_triangles, app->context);
	
	//LOD Offset
	LODOffset_Buffer = generateGPUBuffer(LODOffset, app->context);
	childIndices_Buffer = generateGPUBuffer(childIndices, app->context);


	//Meshlet
	cluster_Buffer = generateGPUBuffer(clusters,app->context);
}

void Model::createClusters()
{
	const size_t max_vertices = 64;
	const size_t max_triangles = 124;
	const float cone_weight = 0.0f;

	size_t max_meshlets = meshopt_buildMeshletsBound(indices.size(), max_vertices, max_triangles);
	std::vector<meshopt_Meshlet> lod_meshlets(max_meshlets);
	std::vector<unsigned int> lod_meshlet_vertices(max_meshlets * max_vertices);
	std::vector<unsigned char> lod_meshlet_triangles(max_meshlets * max_triangles * 3);

	size_t meshlet_count = meshopt_buildMeshlets(lod_meshlets.data(),
		lod_meshlet_vertices.data(),
		lod_meshlet_triangles.data(),
		indices.data(),
		indices.size(),
		&vertices[0].position.x,
		vertices.size(),
		sizeof(Vertex),
		max_vertices,
		max_triangles,
		cone_weight);

	const meshopt_Meshlet& last = lod_meshlets[meshlet_count - 1];

	lod_meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
	lod_meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
	lod_meshlets.resize(meshlet_count);

	meshlets = lod_meshlets;
	meshlet_triangles = lod_meshlet_triangles;
	meshlet_vertices = lod_meshlet_vertices;

	for (auto& meshlet : meshlets) {
		Cluster cluster;
		cluster.meshlet = meshlet;
		cluster.verticesOffset = 0;
		cluster.triangleOffset = 0;
		cluster.childCount = 0;
		cluster.childOffset = 0;

		clusters.push_back(cluster);
	}

	LODOffset.push_back(clusters.size());

}

void Model::GroupingMeshlets()
{

	for (int previousLOD = 0; previousLOD < MAX_LOD; previousLOD++) {


		unsigned int clusterCntInCurrentLOD = 0;
		int start, end;

		if (previousLOD == 0) {
			clusterCntInCurrentLOD = LODOffset[previousLOD];
			start = 0;
			end = LODOffset[previousLOD];
		}
		else {
			clusterCntInCurrentLOD = LODOffset[previousLOD] - LODOffset[previousLOD - 1];
			start = LODOffset[previousLOD - 1];
			end = LODOffset[previousLOD];
		}

		std::unordered_map<Edge, std::unordered_set<int>> edgeToClusters;

	
		for (int clusterIndex = start; clusterIndex < end; clusterIndex++) {
			auto& cluster = clusters[clusterIndex];
			auto& meshlet = cluster.meshlet;

			const uint32_t* vertex_offset = &meshlet_vertices[meshlet.vertex_offset + cluster.verticesOffset]; // from meshoptimizer

			for (int tri = 0; tri < meshlet.triangle_count; tri++) {
				const uint32_t triangle_offset = meshlet.triangle_offset + cluster.triangleOffset;
				uint32_t i0 = meshlet_triangles[triangle_offset + tri * 3 + 0];
				uint32_t i1 = meshlet_triangles[triangle_offset + tri * 3 + 1];
				uint32_t i2 = meshlet_triangles[triangle_offset + tri * 3 + 2];

				uint32_t v0 = vertex_offset[i0];
				uint32_t v1 = vertex_offset[i1];
				uint32_t v2 = vertex_offset[i2];

				Edge edges[3] = {
					{std::min(v0,v1), std::max(v0,v1)},
					{std::min(v2,v1), std::max(v2,v1)},
					{std::min(v0,v2), std::max(v0,v2)},
				};


				for (Edge e : edges) {
					edgeToClusters[e].insert(clusterIndex);
				}
			}
		}


		std::vector<std::unordered_set<int>> clusterNeighbors(clusters.size());

		for (const auto& [edge, clustersSharingEdge] : edgeToClusters) {
			if (clustersSharingEdge.size() == 1)
				continue;

			for (auto i = clustersSharingEdge.begin(); i != clustersSharingEdge.end(); ++i) {
				auto iter = i;
				iter++;
				for (auto j = iter; j != clustersSharingEdge.end(); ++j) {
					int a = *i;
					int b = *j;

					clusterNeighbors[a].insert(b);
					clusterNeighbors[b].insert(a);
				}
			}
		}


		std::vector<idx_t> xadj(clusterCntInCurrentLOD + 1);
		std::vector<idx_t> adjncy;

		xadj[0] = 0;
		for (int i = start; i < end; i++) {
			for (int neighbor : clusterNeighbors[i]) {
				adjncy.push_back(neighbor - start);
			}

			xadj[i - start + 1] = adjncy.size();
		}

		idx_t numMeshlets = clusterCntInCurrentLOD;
		idx_t numClusters = clusterCntInCurrentLOD >> 1;

		std::vector<idx_t> partition(numMeshlets);

		idx_t objval, ncon = 1;
		METIS_PartGraphKway(&numMeshlets, &ncon,
			xadj.data(), adjncy.data(),
			NULL, NULL, NULL,
			&numClusters,
			NULL, NULL, NULL,
			&objval,
			partition.data());


		std::unordered_map<int, std::vector<unsigned int>> clusterToParent;
		for (int i = 0; i < partition.size(); i++) {
			clusterToParent[partition[i]].push_back(i + start);
		}

		for (auto& [key, value] : clusterToParent) {
			createAndInsertClusterNode(value);
		}

		LODOffset.push_back(clusters.size());

	}




}

void Model::generateBoundings()
{
	for (auto& cluster : clusters) {

		std::vector<unsigned int> indices;
		auto meshlet = cluster.meshlet;


		const uint32_t* vertex_offset = &meshlet_vertices[meshlet.vertex_offset + cluster.verticesOffset]; // from meshoptimizer

		for (int tri = 0; tri < meshlet.triangle_count; tri++) {
			const uint32_t triangle_offset = meshlet.triangle_offset + cluster.triangleOffset;
			uint32_t i0 = meshlet_triangles[triangle_offset + tri * 3 + 0];
			uint32_t i1 = meshlet_triangles[triangle_offset + tri * 3 + 1];
			uint32_t i2 = meshlet_triangles[triangle_offset + tri * 3 + 2];

			uint32_t v0 = vertex_offset[i0];
			uint32_t v1 = vertex_offset[i1];
			uint32_t v2 = vertex_offset[i2];

			indices.push_back(v0);
			indices.push_back(v1);
			indices.push_back(v2);
		}

		auto bound = meshopt_computeClusterBounds(&indices[0], indices.size(), &vertices[0].position.x, vertices.size(), sizeof(Vertex));
		Bound b;
		b.center = glm::vec3(bound.center[0], bound.center[1], bound.center[2]);
		b.radius = bound.radius;
		cluster.bound = b;
	}
}

//sorting indices per child
void Model::convertingChildIndices()
{
	auto temp = childIndices;
	childIndices.clear();

	//traversing MAX_LOD
	for (int index = LODOffset[MAX_LOD-1]; index < clusters.size(); index++) {
		Cluster& c = clusters[index];

		std::vector<uint32_t> lodCnt(MAX_LOD + 1,0); //store meshletCount per LOD

		uint32_t childOffset = 0;
		uint32_t childCount = 0;

		
		childOffset = childIndices.size();

		std::queue<std::pair<uint32_t, int>> lodQ;
		lodQ.push({ index,MAX_LOD });
		while (!lodQ.empty()) {
			auto idx = lodQ.front().first;
			auto depth = lodQ.front().second;
			lodQ.pop();
			
			Cluster& childCluster = clusters[idx];
			lodCnt[depth]++;

			for (int i = childCluster.childOffset; i < childCluster.childOffset + childCluster.childCount; i++) {
				lodQ.push({ temp[i],depth - 1 });
			}

		}

		for (int i = 0; i <= MAX_LOD; i++) {
			childIndices.push_back(lodCnt[i]);
		}

		std::vector<uint32_t> localChildIndices;
		std::queue<uint32_t> q;
		q.push(index);
		localChildIndices.push_back(index);
		//Traversing with BFS
		while (!q.empty()) {
			auto& childCluster = clusters[q.front()];
			for (int i = childCluster.childOffset; i < childCluster.childOffset + childCluster.childCount; i++) {
				localChildIndices.push_back(temp[i]);
				q.push(temp[i]);
			}
			childCount += childCluster.childCount;
			
			q.pop();
		}


		for (int i = localChildIndices.size() - 1; i >= 0; i--) {
			childIndices.push_back(localChildIndices[i]);
		}

		c.childOffset = childOffset;
		c.childCount = childCount;
	}
}

void Model::createAndInsertClusterNode(std::vector<unsigned int>& childrenClusterIndices)
{
	if (childrenClusterIndices.size() == 1) {
		Cluster childCluster = clusters[childrenClusterIndices[0]];

		Cluster cluster;
		cluster.meshlet = childCluster.meshlet;
		cluster.childCount = 1;
		cluster.bound = childCluster.bound;
		cluster.childOffset = childIndices.size();
		cluster.verticesOffset = childCluster.verticesOffset;
		cluster.triangleOffset = childCluster.triangleOffset;

		clusters.push_back(cluster);

		childIndices.push_back(childrenClusterIndices[0]);

		return;
	}

	std::vector<uint32_t> _childIndices;

	for (auto& clusterIndex : childrenClusterIndices) {
		auto& cluster = clusters[clusterIndex];
		auto& meshlet = cluster.meshlet;


		const uint32_t* vertex_offset = &meshlet_vertices[meshlet.vertex_offset + cluster.verticesOffset]; // from meshoptimizer

		for (int tri = 0; tri < meshlet.triangle_count; tri++) {
			const uint32_t triangle_offset = meshlet.triangle_offset + cluster.triangleOffset;
			uint32_t i0 = meshlet_triangles[triangle_offset + tri * 3 + 0];
			uint32_t i1 = meshlet_triangles[triangle_offset + tri * 3 + 1];
			uint32_t i2 = meshlet_triangles[triangle_offset + tri * 3 + 2];

			uint32_t v0 = vertex_offset[i0];
			uint32_t v1 = vertex_offset[i1];
			uint32_t v2 = vertex_offset[i2];

			_childIndices.push_back(v0);
			_childIndices.push_back(v1);
			_childIndices.push_back(v2);
		}
	}

	//simplify Meshlets
	size_t target_index_count = _childIndices.size() >> 2;
	float target_error = 1e-1f;
	int options = /*meshopt_SimplifyLockBorder |*/
		meshopt_SimplifyErrorAbsolute |
		meshopt_SimplifySparse;

	std::vector<unsigned int> lod(_childIndices.size());
	float lod_error = 0.f;
	lod.resize(meshopt_simplify(&lod[0], _childIndices.data(), _childIndices.size(), 
		&vertices[0].position.x, vertices.size(), sizeof(Vertex),
		target_index_count, target_error, options, &lod_error));


	//build Meshlets
	const size_t max_vertices = 64;
	const size_t max_triangles = 124;
	const float cone_weight = 0.0f;

	size_t max_meshlets = meshopt_buildMeshletsBound(_childIndices.size(), max_vertices, max_triangles);
	std::vector<meshopt_Meshlet> lod_meshlets(max_meshlets);
	std::vector<unsigned int> lod_meshlet_vertices(max_meshlets * max_vertices);
	std::vector<unsigned char> lod_meshlet_triangles(max_meshlets * max_triangles * 3);

	size_t meshlet_count = meshopt_buildMeshlets(lod_meshlets.data(), lod_meshlet_vertices.data(), lod_meshlet_triangles.data(),
		lod.data(),lod.size(),
		&vertices[0].position.x, vertices.size(), sizeof(Vertex), 
		max_vertices, max_triangles, cone_weight);

	const meshopt_Meshlet& last = lod_meshlets[meshlet_count - 1];

	lod_meshlet_vertices.resize(last.vertex_offset + last.vertex_count);
	lod_meshlet_triangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
	lod_meshlets.resize(meshlet_count);


	//generate Cluster and insert Result
	for (int i = 0; i < meshlet_count; i++) {
		Cluster cluster;
		cluster.meshlet = lod_meshlets[i];
		cluster.childCount = childrenClusterIndices.size();
		cluster.childOffset = childIndices.size();

		cluster.triangleOffset = meshlet_triangles.size();
		cluster.verticesOffset = meshlet_vertices.size();

		clusters.push_back(cluster);

	}

	for (auto& childClusterIndex : childrenClusterIndices)
		childIndices.push_back(childClusterIndex);

	for (auto& meshlet_triangle : lod_meshlet_triangles)
		meshlet_triangles.push_back(meshlet_triangle);

	for (auto& meshlet_vertice : lod_meshlet_vertices)
		meshlet_vertices.push_back(meshlet_vertice);
}

void Model::Init(std::string path) {

	loadFile(path);
	createClusters();
	GroupingMeshlets();
	convertingChildIndices();
	generateBoundings();
	initBuffer();
	
}