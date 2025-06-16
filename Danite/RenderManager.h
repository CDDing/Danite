#pragma once
#define FRAME_CNT 2
#include "Pipeline.h"
#include "Camera.h"
#include "Model.h"
namespace DDing {
	enum class PassType {
		eForward,
		eShadow,
	};
	class Camera;
	class Scene;
	class SwapChain;
	class Pipeline;
}
struct FrameData {
	//For Synchronization
	vk::raii::Semaphore renderFinish = nullptr;
	vk::raii::Semaphore imageAvailable = nullptr;
	vk::raii::Fence waitFrame = nullptr;

	//For Command
	vk::raii::CommandPool commandPool = nullptr;
	vk::raii::CommandBuffer commandBuffer = nullptr;

	vk::raii::DescriptorPool descriptorPool = nullptr;
	vk::raii::DescriptorSet descriptorSet = nullptr;

	DDing::Buffer stagingBuffer;
	DDing::Buffer uniformBuffer;

};
class RenderManager
{
public:
	RenderManager() {};
	void Init();
	void DrawFrame();
	void Draw(vk::CommandBuffer commandBuffer,uint32_t imageIndex);
	void DrawUI();

	uint32_t currentFrame = 0;
	vk::raii::Sampler DefaultSampler = nullptr;

	DDing::Camera camera;
	std::vector<FrameData> frameDatas = {};
private:
	static int swapchainImageCnt;
	void InitGUISampler();
	void initFrameDatas();
	void createRenderPass();
	void createPipeline();
	void createDescriptor();
	void createDepthImage();
	void InitDescriptorsForMeshlets();
	void createFramebuffers();
	void createComputePipeline();

	void updateUniform(vk::CommandBuffer commandBuffer);

	void submitCommandBuffer(vk::CommandBuffer commandBuffer);
	void presentCommandBuffer(vk::CommandBuffer commandBuffer,uint32_t imageIndex);

	
	std::array<DDing::Image, FRAME_CNT> depthImages;
	std::array<DDing::Image, FRAME_CNT> depthImagesForHiZ;

	vk::raii::RenderPass renderPass = nullptr;
	vk::raii::Pipeline pipeline = nullptr;
	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;

	vk::raii::DescriptorPool meshlet_descriptorPool = nullptr;
	vk::raii::DescriptorSet meshlet_descriptorSet = nullptr;
	vk::raii::DescriptorSetLayout meshlet_descriptorSetLayout = nullptr;

	std::vector<vk::raii::Framebuffer> framebuffers;

	//Compute
	vk::raii::Pipeline computePipeline = nullptr;
	vk::raii::PipelineLayout computePipelineLayout = nullptr;

	vk::raii::DescriptorPool computeDescriptorPool = nullptr;
	vk::raii::DescriptorSetLayout computeDescriptorSetLayout = nullptr;
	std::array<std::vector<vk::raii::DescriptorSet>,FRAME_CNT> computeDescriptorSets;
	std::array<std::vector<vk::raii::ImageView>,FRAME_CNT> depthImageForHiZViews;
};

