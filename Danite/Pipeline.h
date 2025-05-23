#pragma once
struct PipelineDesc {
	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages;
	vk::PipelineDynamicStateCreateInfo dynamicState;
	vk::PipelineVertexInputStateCreateInfo vertexInput;
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
	vk::PipelineViewportStateCreateInfo viewportState;
	vk::PipelineRasterizationStateCreateInfo rasterizer;
	vk::PipelineMultisampleStateCreateInfo multiSample;
	vk::PipelineColorBlendStateCreateInfo colorBlend;
	vk::PipelineDepthStencilStateCreateInfo depthStencil;
	vk::PipelineLayoutCreateInfo layout;
	vk::RenderPass renderPass;
};
std::vector<uint32_t> loadShader(const char* filePath);
namespace DDing {
	class Pipeline
	{
	public:
		operator vk::Pipeline() { return pipeline; }
		vk::raii::PipelineLayout& GetLayout() { return pipelineLayout; };
	protected:
		vk::raii::Pipeline pipeline = nullptr;
		vk::raii::PipelineLayout pipelineLayout = nullptr;
	};
	class GraphicsPipeline : public Pipeline
	{
	public:
		GraphicsPipeline(DDing::Context& context, PipelineDesc& desc);
	};
	class ComputePipeline
	{

	};
}

