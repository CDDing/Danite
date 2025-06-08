#pragma once
#include <vma/vk_mem_alloc.h>
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
struct DrawPushConstant {
    glm::mat4 transformMatrix;
    vk::DeviceAddress deviceAddress;
    int materialIndex;
};

struct SwapChainSupportDetails {
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> presentModes;
};

struct alignas(16) sLight {
    glm::vec3 color;
    float intensity;
    glm::vec3 position;
    int type;
    glm::vec3 direction;
    float innerCone, outerCone;
};

struct alignas(16) GlobalBuffer {
    glm::mat4 view;
    glm::mat4 projection;
    glm::mat4 transform;
    glm::vec3 cameraPosition;
    uint32_t totalClusters;
    float time;

};

namespace DDing {
    class Context;
    struct Image {
        static void setImageLayout(vk::CommandBuffer commandBuffer, vk::Image image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout);
        Image() = default;
        Image(VkImageCreateInfo imgInfo, VmaAllocationCreateInfo allocInfo, vk::ImageViewCreateInfo imageViewInfo);

        //non-copyable
        Image(const Image& other) = delete;
        Image& operator=(const Image& other) = delete;

        //Only movable
        Image(Image&& other) noexcept {
            *this = std::move(other);
        };
        Image& operator=(Image&& other) noexcept {
            image = other.image;
            imageView = other.imageView;
            allocation = other.allocation;
            layout = other.layout;
            format = other.format;
            mipLevel = other.mipLevel;

            other.image = nullptr;
            other.imageView = nullptr;
            other.allocation = nullptr;
            other.layout = vk::ImageLayout::eUndefined;
            other.format = {};
            other.mipLevel = 0;
            return *this;
        };

        ~Image();
        void setImageLayout(vk::CommandBuffer commandBuffer, vk::ImageLayout newLayout);
        void generateMipmaps(vk::CommandBuffer commandBuffer);
        VkImage image = nullptr;
        vk::ImageView imageView = nullptr;
        VmaAllocation allocation;

        vk::ImageLayout layout = vk::ImageLayout::eUndefined;
        vk::Format format;
        uint32_t mipLevel;
        vk::Extent3D extent;
    };
    struct Buffer {
        Buffer() = default;
        Buffer(VkBufferCreateInfo bufferCreateInfo, VmaAllocationCreateInfo allocInfo);
        ~Buffer();
        //non-copyable
        Buffer(const Buffer& other) = delete;
        Buffer& operator=(const Buffer& other) = delete;

        //Only movable
        Buffer(Buffer&& other) noexcept {
            *this = std::move(other);
        };
        Buffer& operator=(Buffer&& other) noexcept {
            buffer = other.buffer;
            allocation = other.allocation;

            other.buffer = nullptr;
            other.allocation = nullptr;
            return *this;
        };
        void* GetMappedPtr();
        VkBuffer buffer = nullptr;
        VmaAllocation allocation;

    };
    struct Texture {
        DDing::Image* image;
        vk::Sampler sampler;
    };

    struct alignas(16) Material {
        int baseColorIndex = -1;
        int normalMapIndex = -1;
        int metallicRoughnessIndex = -1;
        int emissiveIndex = -1;

        glm::vec4 baseColorFactor = glm::vec4(1.0f);
        float metallicFactor = 1.0f;
        float roughnessFactor = 1.0f;
        glm::vec2 padding;
        glm::vec3 emissiveFactor = glm::vec3(0.0f);

        float padding2;
    };

}
