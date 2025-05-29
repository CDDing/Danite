#include "pch.h"
#include "Model.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "tiny_gltf.h"
void Model::Draw(vk::CommandBuffer commandBuffer)
{

    vk::Buffer vertexBuffers[] = { vertexBuffer.buffer };
    vk::DeviceSize offsets[] = { 0 };


    //commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    //commandBuffer.bindIndexBuffer(indexBuffers[LOD].buffer, 0, vk::IndexType::eUint32);
    
    //commandBuffer.drawIndexed(static_cast<uint32_t>(indices[LOD].size()), 1, 0, 0, 0);

    static const int local_size_x = 1;
    vkCmdDrawMeshTasksEXT(commandBuffer, meshlets[LOD].size() / local_size_x, 1, 1);
    //commandBuffer.drawMeshTasksEXT(meshlets[LOD].size(), 1, 1);
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

            indices.push_back(index);
        }

    }

}

void Model::initBuffer()
{

    //init Buffer
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);

    //Buffer 
    {
        //Vertex
        {
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.setSize(vertexBufferSize);
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer);

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.0f;

            vertexBuffer = DDing::Buffer(bufferInfo, allocInfo);

        }

    }
    //Staging & Copy
    {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.setSize(vertexBufferSize);
        bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        DDing::Buffer staging = DDing::Buffer(bufferInfo, allocInfo);

        void* data = staging.GetMappedPtr();

        memcpy(data, vertices.data(), vertexBufferSize);

        app->context.immediate_submit([&](vk::CommandBuffer commandBuffer) {
            vk::BufferCopy vertexCopy{};
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = vertexBufferSize;

            commandBuffer.copyBuffer(staging.buffer, vertexBuffer.buffer, vertexCopy);

            });
    }

    for (int i = 0; i < MAX_LOD; i++) {

        //Index
        {
            const size_t indexBufferSize = indices[i].size() * sizeof(uint32_t);
            {
                vk::BufferCreateInfo bufferInfo{};
                bufferInfo.setSize(indexBufferSize);
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer);

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                allocInfo.priority = 1.0f;

                indexBuffers.push_back(DDing::Buffer(bufferInfo, allocInfo));
            }
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.setSize(indexBufferSize);
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            DDing::Buffer staging = DDing::Buffer(bufferInfo, allocInfo);

            void* data = staging.GetMappedPtr();

            memcpy(data, indices[i].data(), indexBufferSize);

            app->context.immediate_submit([&](vk::CommandBuffer commandBuffer) {

                vk::BufferCopy indexCopy{};
                indexCopy.dstOffset = 0;
                indexCopy.srcOffset = 0;
                indexCopy.size = indexBufferSize;

                commandBuffer.copyBuffer(staging.buffer, indexBuffers[i].buffer, indexCopy);
                });
        }
        //Meshlet
        {
            const size_t meshletBufferSize = meshlets[i].size() * sizeof(meshopt_Meshlet);
            {
                vk::BufferCreateInfo bufferInfo{};
                bufferInfo.setSize(meshletBufferSize);
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer);

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                allocInfo.priority = 1.0f;

                meshlet_Buffers.push_back(DDing::Buffer(bufferInfo, allocInfo));
            }
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.setSize(meshletBufferSize);
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            DDing::Buffer staging = DDing::Buffer(bufferInfo, allocInfo);

            void* data = staging.GetMappedPtr();

            memcpy(data, meshlets[i].data(), meshletBufferSize);

            app->context.immediate_submit([&](vk::CommandBuffer commandBuffer) {

                vk::BufferCopy indexCopy{};
                indexCopy.dstOffset = 0;
                indexCopy.srcOffset = 0;
                indexCopy.size = meshletBufferSize;

                commandBuffer.copyBuffer(staging.buffer, meshlet_Buffers[i].buffer, indexCopy);
                });
        }
        //meshlet_Vertices
        {
            const size_t meshlet_vertices_Size = meshlet_vertices[i].size() * sizeof(unsigned int);
            {
                vk::BufferCreateInfo bufferInfo{};
                bufferInfo.setSize(meshlet_vertices_Size);
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer);

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                allocInfo.priority = 1.0f;

                meshlet_vertices_Buffers.push_back(DDing::Buffer(bufferInfo, allocInfo));
            }
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.setSize(meshlet_vertices_Size);
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            DDing::Buffer staging = DDing::Buffer(bufferInfo, allocInfo);

            void* data = staging.GetMappedPtr();

            memcpy(data, meshlet_vertices[i].data(), meshlet_vertices_Size);

            app->context.immediate_submit([&](vk::CommandBuffer commandBuffer) {

                vk::BufferCopy indexCopy{};
                indexCopy.dstOffset = 0;
                indexCopy.srcOffset = 0;
                indexCopy.size = meshlet_vertices_Size;

                commandBuffer.copyBuffer(staging.buffer, meshlet_vertices_Buffers[i].buffer, indexCopy);
                });
        }
        //meshlet_Triangles
        {
            const size_t meshlet_Triangles_Size = meshlet_triangles[i].size() * sizeof(unsigned char);
            {
                vk::BufferCreateInfo bufferInfo{};
                bufferInfo.setSize(meshlet_Triangles_Size);
                bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer);

                VmaAllocationCreateInfo allocInfo{};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
                allocInfo.priority = 1.0f;

                meshlet_triangles_Buffers.push_back(DDing::Buffer(bufferInfo, allocInfo));
            }
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.setSize(meshlet_Triangles_Size);
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

            DDing::Buffer staging = DDing::Buffer(bufferInfo, allocInfo);

            void* data = staging.GetMappedPtr();

            memcpy(data, meshlet_triangles[i].data(), meshlet_Triangles_Size);

            app->context.immediate_submit([&](vk::CommandBuffer commandBuffer) {

                vk::BufferCopy indexCopy{};
                indexCopy.dstOffset = 0;
                indexCopy.srcOffset = 0;
                indexCopy.size = meshlet_Triangles_Size;

                commandBuffer.copyBuffer(staging.buffer, meshlet_triangles_Buffers[i].buffer, indexCopy);
                });
        }
    }
}

void Model::createClusters()
{
    for (int i = 0; i < MAX_LOD; i++) {
        const size_t max_vertices = 64;
        const size_t max_triangles = 124;
        const float cone_weight = 0.0f;

        size_t max_meshlets = meshopt_buildMeshletsBound(indices[i].size(), max_vertices, max_triangles);
        std::vector<meshopt_Meshlet> lod_meshlets(max_meshlets);
        std::vector<unsigned int> lod_meshlet_vertices(max_meshlets * max_vertices);
        std::vector<unsigned char> lod_meshlet_triangles(max_meshlets * max_triangles * 3);

        size_t meshlet_count = meshopt_buildMeshlets(lod_meshlets.data(),
            lod_meshlet_vertices.data(), 
            lod_meshlet_triangles.data(), 
            indices[i].data(),
            indices[i].size(), 
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

        meshlets.push_back(lod_meshlets);
        meshlet_vertices.push_back(lod_meshlet_vertices);
        meshlet_triangles.push_back(lod_meshlet_triangles);


    }
}

void Model::createLODs()
{
    for (int i = 1; i < MAX_LOD; i++) {
        float threshold = 0.2f;
        size_t target_index_count = size_t(indices[i-1].size() * threshold);
        float target_error = 1.2e-2f;

        std::vector<uint32_t> lod(indices[i-1].size());
        float lod_error = 0.f;
        lod.resize(meshopt_simplify(&lod[0], indices[i-1].data(), static_cast<size_t>(indices[i-1].size()), &vertices[0].position.x, static_cast<size_t>(vertices.size()), sizeof(Vertex),
            target_index_count, target_error, /* options= */ 0, &lod_error));

        indices.push_back(lod);
    }
}

void Model::Init(std::string path) {
    vkCmdDrawMeshTasksEXT = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(vkGetDeviceProcAddr(*app->context.logical, "vkCmdDrawMeshTasksEXT"));

    loadFile(path);
    createLODs();
    createClusters();
    initBuffer();
}