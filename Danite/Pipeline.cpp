#include "pch.h"
#include "Pipeline.h"


DDing::GraphicsPipeline::GraphicsPipeline(DDing::Context& context, PipelineDesc& desc)
{
	pipelineLayout = context.logical.createPipelineLayout(desc.layout);
	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.setStages(desc.shaderStages);
	pipelineInfo.setPVertexInputState(&desc.vertexInput);
	pipelineInfo.setPInputAssemblyState(&desc.inputAssembly);
	pipelineInfo.setPViewportState(&desc.viewportState);
	pipelineInfo.setPRasterizationState(&desc.rasterizer);
	pipelineInfo.setPDepthStencilState(&desc.depthStencil);
	pipelineInfo.setPMultisampleState(&desc.multiSample);
	pipelineInfo.setPColorBlendState(&desc.colorBlend);
	pipelineInfo.setPDynamicState(&desc.dynamicState);
	pipelineInfo.setLayout(pipelineLayout);
	pipelineInfo.setRenderPass(desc.renderPass);
	pipelineInfo.setSubpass(0);
	pipelineInfo.setBasePipelineHandle(VK_NULL_HANDLE);
	pipelineInfo.setBasePipelineIndex(-1);
	pipeline = context.logical.createGraphicsPipeline(nullptr,pipelineInfo);
}

std::vector<uint32_t> loadShader(const char* filePath)
{
	std::ifstream file(filePath, std::ios::ate | std::ios::binary);
	if (!file.is_open())
		std::runtime_error("failed to load shader!");

	size_t fileSize = (size_t)file.tellg();
	std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));
	file.seekg(0);
	file.read((char*)buffer.data(), fileSize);
	file.close();

	return buffer;
}
