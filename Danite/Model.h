#pragma once
#include "tiny_gltf.h"
#include "meshoptimizer.h"
#include "metis.h"
struct Vertex {
	glm::vec3 position;
	float texCoordX;
	glm::vec3 normal;
	float texCoordY;
};
struct Cluster {

	meshopt_Meshlet meshlet;
	meshopt_Bounds bound;
	unsigned int verticesOffset;
	unsigned int triangleOffset;
	unsigned int childOffset;
	unsigned int childCount;
};
class Model
{
public:
	Model() {};
	void Init(std::string path);

	std::vector<Vertex> vertices;

	std::vector<uint32_t> indices;
	std::vector<meshopt_Meshlet> meshlets;
	std::vector<unsigned int> meshlet_vertices;
	std::vector<unsigned char> meshlet_triangles;
	std::vector<Cluster> clusters;
	std::vector<unsigned int> childIndices;

	//Buffers, per LOD
	DDing::Buffer vertexBuffer;
	DDing::Buffer indexBuffer;
	DDing::Buffer cluster_Buffer;
	DDing::Buffer meshlet_vertices_Buffer;
	DDing::Buffer meshlet_triangles_Buffer;
	DDing::Buffer LODOffset_Buffer;
	DDing::Buffer childIndices_Buffer;
	void Draw(vk::raii::CommandBuffer& commandBuffer);

	uint32_t LOD = 0;

	// ex
	// LODOffset[0] = 0; 
	// LODOffset[1] = 1000; ->LOD 0 owns 1000Clusters. and LOD 1 starts from 1000 index.
	std::vector<unsigned int> LODOffset;
	const uint32_t MAX_LOD = 4;
private:
	void loadFile(std::string path);
	void initBuffer();

	void createClusters();
	void buildBVH();
	void GroupingMeshlets();
	void generateBoundings();

	//insert new cluster with Simplyfying and building new clusters from clusters
	void createAndInsertClusterNode(std::vector<unsigned int>& childrenClusterIndices);
};

struct Edge {
	uint32_t start;
	uint32_t end;

	bool operator==(const Edge& other) const {
		return start == other.start && end == other.end;
	}
};


namespace std {
	template<>
	struct hash<Edge> {
		std::size_t operator()(const Edge& e) const {
			return std::hash<int>()(e.start) ^ (std::hash<int>()(e.end) << 1);
		}
	};
}

template<typename T>
DDing::Buffer generateGPUBuffer(std::vector<T>& contents, DDing::Context& context) {
	//init Buffer
	const size_t bufferSize = contents.size() * sizeof(T);

	//Buffer 

	vk::BufferCreateInfo bufferInfo{};
	bufferInfo.setSize(bufferSize);
	bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer);

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocInfo.priority = 1.0f;

	DDing::Buffer buffer = DDing::Buffer(bufferInfo, allocInfo);




	//Staging & Copy
	{
		vk::BufferCreateInfo bufferInfo{};
		bufferInfo.setSize(bufferSize);
		bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

		VmaAllocationCreateInfo allocInfo{};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
		allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

		DDing::Buffer staging = DDing::Buffer(bufferInfo, allocInfo);

		void* data = staging.GetMappedPtr();

		memcpy(data, contents.data(), bufferSize);

		context.immediate_submit([&](vk::CommandBuffer commandBuffer) {
			vk::BufferCopy bufferCopy{};
			bufferCopy.dstOffset = 0;
			bufferCopy.srcOffset = 0;
			bufferCopy.size = bufferSize;

			commandBuffer.copyBuffer(staging.buffer, buffer.buffer, bufferCopy);

			});
	}

	return buffer;
}