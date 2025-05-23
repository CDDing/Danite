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


    commandBuffer.bindVertexBuffers(0, vertexBuffers, offsets);
    commandBuffer.bindIndexBuffer(indexBuffer.buffer, 0, vk::IndexType::eUint32);

    commandBuffer.drawIndexed(static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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
                            indices.push_back(static_cast<uint32_t>(buf[i]));
                    }
                    else if (accessor.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT) {
                        const uint32_t* buf = reinterpret_cast<const uint32_t*>(dataPtr);
                        for (size_t i = 0; i < count; ++i)
                            indices.push_back(buf[i]);
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
                    v.texCoord = i < texcoords.size() ? texcoords[i] : glm::vec2(0.0f);
                    vertices.push_back(v);
                }
            }
        }

    }

    //init Buffer
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    //Buffer 
    {
        //Vertex
        {
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.setSize(vertexBufferSize);
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eVertexBuffer);

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.0f;

            vertexBuffer = DDing::Buffer(bufferInfo, allocInfo);
            
        }
        //Index
        {
            vk::BufferCreateInfo bufferInfo{};
            bufferInfo.setSize(indexBufferSize);
            bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eIndexBuffer);

            VmaAllocationCreateInfo allocInfo{};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
            allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
            allocInfo.priority = 1.0f;

            indexBuffer = DDing::Buffer(bufferInfo, allocInfo);
        }

    }
    //Staging & Copy
    {
        vk::BufferCreateInfo bufferInfo{};
        bufferInfo.setSize(vertexBufferSize + indexBufferSize);
        bufferInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

        DDing::Buffer staging = DDing::Buffer(bufferInfo, allocInfo);

        void* data = staging.GetMappedPtr();

        memcpy(data, vertices.data(), vertexBufferSize);
        memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

        app->context.immediate_submit([&](vk::CommandBuffer commandBuffer) {
            vk::BufferCopy vertexCopy{};
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = vertexBufferSize;

            commandBuffer.copyBuffer(staging.buffer, vertexBuffer.buffer, vertexCopy);

            vk::BufferCopy indexCopy{};
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertexBufferSize;
            indexCopy.size = indexBufferSize;

            commandBuffer.copyBuffer(staging.buffer, indexBuffer.buffer, indexCopy);
            });
    }
}

void Model::createClusters()
{
}

void Model::createLODs()
{
}

void Model::Init(std::string path) {

    loadFile(path);
    createClusters();
    createLODs();
}