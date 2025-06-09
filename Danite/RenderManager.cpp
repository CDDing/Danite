#include "pch.h"
#include "RenderManager.h"
#include "Pipeline.h"
#include "Context.h"
void RenderManager::Init()
{
	createRenderPass();
	createDescriptor();
	InitDescriptorsForMeshlets();
	createPipeline();
	InitGUISampler();
	initFrameDatas();
	createDepthImage();
	createFramebuffers();

}
void RenderManager::DrawFrame()
{

	FrameData& frameData = frameDatas[currentFrame];

	auto resultForFence = app->context.logical.waitForFences({ *frameData.waitFrame }, vk::True, UINT64_MAX);

	auto acquireImage = app->context.swapchain.acquireNextImage(UINT64_MAX, *frameData.imageAvaiable, VK_NULL_HANDLE);
	uint32_t imageIndex = acquireImage.second;
	vk::Result result = acquireImage.first;
	//TODO handle recreateSwapchain
	app->context.logical.resetFences({ *frameData.waitFrame });

	frameData.commandBuffer.reset({});
	vk::CommandBufferBeginInfo beginInfo{};
	frameData.commandBuffer.begin(beginInfo);

	updateUniform(frameData.commandBuffer);
	Draw(frameData.commandBuffer, imageIndex);



	app->input.DrawImGui(frameData.commandBuffer, imageIndex);

	frameData.commandBuffer.end();
	submitCommandBuffer(*frameData.commandBuffer);
	presentCommandBuffer(*frameData.commandBuffer, imageIndex);


	currentFrame = (currentFrame + 1) % FRAME_CNT;
}
void RenderManager::Draw(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
{
	auto& frameData = frameDatas[currentFrame];
	vk::RenderPassBeginInfo renderPassInfo{};
	renderPassInfo.setRenderPass(renderPass);
	renderPassInfo.framebuffer = framebuffers[imageIndex];
	renderPassInfo.renderArea.offset = vk::Offset2D{ 0,0 };
	renderPassInfo.renderArea.extent = app->context.swapchainExtent;

	std::array<vk::ClearValue, 2> clearValues{};
	clearValues[0].color = vk::ClearColorValue{ 1.0f, 1.0f, 1.0f, 1.0f };
	clearValues[1].depthStencil = vk::ClearDepthStencilValue{ 1.0f, 0 };
	renderPassInfo.setClearValues(clearValues);

	//SkyboxDraw
	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);

	commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);

	std::vector<vk::DescriptorSet> descriptorSetListForSkybox = {
		*frameData.descriptorSet,
		*meshlet_descriptorSet
	};
	commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics,
		pipelineLayout, 0, descriptorSetListForSkybox, {});

	app->model.Draw(frameData.commandBuffer);
	commandBuffer.endRenderPass();
}
void RenderManager::DrawUI()
{
}

void RenderManager::InitGUISampler()
{
	vk::SamplerCreateInfo samplerInfo{};
	samplerInfo.setMagFilter(vk::Filter::eLinear);
	samplerInfo.setMinFilter(vk::Filter::eLinear);
	samplerInfo.setMipmapMode(vk::SamplerMipmapMode::eLinear);
	samplerInfo.setAddressModeU(vk::SamplerAddressMode::eRepeat);
	samplerInfo.setAddressModeV(vk::SamplerAddressMode::eRepeat);
	samplerInfo.setAddressModeW(vk::SamplerAddressMode::eRepeat);
	samplerInfo.setMinLod(-1000);
	samplerInfo.setMaxLod(1000);
	samplerInfo.setMaxAnisotropy(1.0f);
	DefaultSampler = vk::raii::Sampler(app->context.logical, samplerInfo);
}

void RenderManager::initFrameDatas()
{
	for (int i = 0; i < FRAME_CNT; i++) {
		FrameData frameData{};
		{
			vk::SemaphoreCreateInfo semaphoreInfo{};
			frameData.imageAvaiable = app->context.logical.createSemaphore(semaphoreInfo);
			frameData.renderFinish = app->context.logical.createSemaphore(semaphoreInfo);
		}
		{
			vk::FenceCreateInfo fenceInfo{};
			fenceInfo.flags = vk::FenceCreateFlagBits::eSignaled;
			frameData.waitFrame = app->context.logical.createFence(fenceInfo);
		}
		{
			vk::CommandPoolCreateInfo commandPoolInfo{};
			QueueFamilyIndices indices = app->context.findQueueFamilies(*app->context.physical, *app->context.surface);
			commandPoolInfo.setQueueFamilyIndex(indices.graphicsFamily.value());
			commandPoolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
			frameData.commandPool = app->context.logical.createCommandPool(commandPoolInfo);
		}
		{
			vk::CommandBufferAllocateInfo allocInfo{};
			allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
			allocInfo.setCommandBufferCount(1);
			allocInfo.setCommandPool(*frameData.commandPool);

			frameData.commandBuffer = std::move(app->context.logical.allocateCommandBuffers(allocInfo).front());
		}
		{

			std::vector<vk::DescriptorPoolSize> poolSizes = {
				vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,1),
				vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer,4)
			};

			vk::DescriptorPoolCreateInfo poolInfo{};
			poolInfo.setMaxSets(FRAME_CNT);
			poolInfo.setPoolSizes(poolSizes);
			poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

			frameData.descriptorPool = app->context.logical.createDescriptorPool(poolInfo);
		}
		{
			vk::DescriptorSetAllocateInfo allocInfo{};
			allocInfo.setDescriptorPool(*frameData.descriptorPool);
			allocInfo.setDescriptorSetCount(1);
			allocInfo.setSetLayouts(*descriptorSetLayout);

			frameData.descriptorSet = std::move(app->context.logical.allocateDescriptorSets(allocInfo).front());
		}
		{
			vk::BufferCreateInfo stagingInfo{ };
			stagingInfo.setSize(sizeof(GlobalBuffer));
			stagingInfo.setUsage(vk::BufferUsageFlagBits::eTransferSrc);

			VmaAllocationCreateInfo allocInfo{};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;

			frameData.stagingBuffer = DDing::Buffer(stagingInfo, allocInfo);
		}
		{
			vk::BufferCreateInfo bufferInfo{};
			bufferInfo.setSize(sizeof(GlobalBuffer));
			bufferInfo.setUsage(vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst);

			VmaAllocationCreateInfo allocInfo{};
			allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
			allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
			allocInfo.priority = 1.0f;

			frameData.uniformBuffer = DDing::Buffer(bufferInfo, allocInfo);
		}
		{
			vk::DescriptorBufferInfo bufferInfo{};
			bufferInfo.setBuffer(frameData.uniformBuffer.buffer);
			bufferInfo.setRange(sizeof(GlobalBuffer));
			bufferInfo.setOffset(0);


			vk::WriteDescriptorSet descriptorWrite{};
			descriptorWrite.dstSet = frameData.descriptorSet;
			descriptorWrite.dstBinding = 0;
			descriptorWrite.dstArrayElement = 0;
			descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
			descriptorWrite.descriptorCount = 1;
			descriptorWrite.pBufferInfo = &bufferInfo;

			app->context.logical.updateDescriptorSets(descriptorWrite, nullptr);
		}
		frameDatas.push_back(std::move(frameData));
	}
}

void RenderManager::createRenderPass()
{
	vk::AttachmentDescription colorAttachment{};
	colorAttachment.format = vk::Format::eB8G8R8A8Unorm;
	colorAttachment.samples = vk::SampleCountFlagBits::e1;
	colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
	colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
	colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

	vk::AttachmentDescription depthAttachment{};
	depthAttachment.format = vk::Format::eD32Sfloat;
	depthAttachment.samples = vk::SampleCountFlagBits::e1;
	depthAttachment.loadOp = vk::AttachmentLoadOp::eClear;
	depthAttachment.storeOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	depthAttachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	depthAttachment.initialLayout = vk::ImageLayout::eUndefined;
	depthAttachment.finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	vk::AttachmentReference colorAttachmentRef{};
	colorAttachmentRef.attachment = 0;
	colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

	vk::AttachmentReference depthAttachmentRef{};
	depthAttachmentRef.attachment = 1;
	depthAttachmentRef.layout = vk::ImageLayout::eDepthStencilAttachmentOptimal;

	std::vector<vk::AttachmentReference> colorAttachments{ colorAttachmentRef };

	vk::SubpassDescription subpass{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpass.setColorAttachments(colorAttachments);
	subpass.pDepthStencilAttachment = &depthAttachmentRef;

	vk::SubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
	dependency.srcAccessMask = {};
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite;

	std::vector<vk::AttachmentDescription> attachments = { colorAttachment, depthAttachment };
	vk::RenderPassCreateInfo renderPassInfo{};
	renderPassInfo.setAttachments(attachments);
	renderPassInfo.setSubpasses(subpass);
	renderPassInfo.setDependencies(dependency);

	renderPass = app->context.logical.createRenderPass(renderPassInfo);
}

void RenderManager::createPipeline()
{
	auto taskShaderCode = loadShader("Shaders/shader.task.spv");
	vk::ShaderModuleCreateInfo taskShaderCreateInfo{};
	taskShaderCreateInfo.setCode(taskShaderCode);
	vk::raii::ShaderModule taskShaderModule = app->context.logical.createShaderModule(taskShaderCreateInfo);
	vk::PipelineShaderStageCreateInfo taskStage{};
	taskStage.setModule(*taskShaderModule);
	taskStage.setPName("main");
	taskStage.setStage(vk::ShaderStageFlagBits::eTaskEXT);

	auto meshShaderCode = loadShader("Shaders/shader.mesh.spv");
	vk::ShaderModuleCreateInfo meshShaderCreateInfo{};
	meshShaderCreateInfo.setCode(meshShaderCode);
	vk::raii::ShaderModule meshShaderModule = app->context.logical.createShaderModule(meshShaderCreateInfo);
	vk::PipelineShaderStageCreateInfo meshStage{};
	meshStage.setModule(*meshShaderModule);
	meshStage.setPName("main");
	meshStage.setStage(vk::ShaderStageFlagBits::eMeshEXT);

	auto fragShaderCode = loadShader("Shaders/shader.frag.spv");
	vk::ShaderModuleCreateInfo fragCreateInfo{};
	fragCreateInfo.setCode(fragShaderCode);
	vk::raii::ShaderModule fragShaderModule = app->context.logical.createShaderModule(fragCreateInfo);
	vk::PipelineShaderStageCreateInfo fragStage{};
	fragStage.setModule(*fragShaderModule);
	fragStage.setPName("main");
	fragStage.setStage(vk::ShaderStageFlagBits::eFragment);


	std::vector<vk::PipelineShaderStageCreateInfo> shaderStages = { taskStage,meshStage,fragStage };


	std::vector<vk::DynamicState> dynamicStates;
	vk::PipelineDynamicStateCreateInfo dynamicState{};
	dynamicState.setDynamicStates(dynamicStates);

	//TODO
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};

	vk::VertexInputBindingDescription bindingDescription{};
	bindingDescription.setBinding(0);
	bindingDescription.setInputRate(vk::VertexInputRate::eVertex);
	bindingDescription.setStride(sizeof(Vertex));

	std::array<vk::VertexInputAttributeDescription, 3> attributeDescriptions{};

	attributeDescriptions[0].binding = 0;
	attributeDescriptions[0].location = 0;
	attributeDescriptions[0].format = vk::Format::eR32G32B32Sfloat;  // glm::vec3
	attributeDescriptions[0].offset = offsetof(Vertex, position);

	attributeDescriptions[1].binding = 0;
	attributeDescriptions[1].location = 1;
	attributeDescriptions[1].format = vk::Format::eR32G32B32Sfloat;  // glm::vec3
	attributeDescriptions[1].offset = offsetof(Vertex, normal);

	attributeDescriptions[2].binding = 0;
	attributeDescriptions[2].location = 2;
	attributeDescriptions[2].format = vk::Format::eR32G32Sfloat;     // glm::vec2
	//attributeDescriptions[2].offset = offsetof(Vertex, texCoord);
	//vertexInputInfo.setVertexAttributeDescriptions(attributeDescriptions);
	//vertexInputInfo.setVertexBindingDescriptions(bindingDescription);
	vertexInputInfo.setVertexAttributeDescriptions(nullptr);
	vertexInputInfo.setVertexBindingDescriptions(nullptr);

	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.setPrimitiveRestartEnable(vk::False);
	inputAssembly.setTopology(vk::PrimitiveTopology::eTriangleList);

	vk::Viewport viewport{};
	viewport.setWidth(app->context.swapchainExtent.width);
	viewport.setHeight(app->context.swapchainExtent.height);
	viewport.setX(0.0f);
	viewport.setY(0.0f);
	viewport.setMinDepth(0.0f);
	viewport.setMaxDepth(1.0f);

	vk::Rect2D scissor{};
	scissor.setExtent(app->context.swapchainExtent);

	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.setScissors(scissor);
	viewportState.setViewports(viewport);

	vk::PipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.setCullMode(vk::CullModeFlagBits::eFront);
	rasterizer.setFrontFace(vk::FrontFace::eCounterClockwise);
	rasterizer.setPolygonMode(vk::PolygonMode::eFill);
	rasterizer.setDepthClampEnable(vk::False);
	rasterizer.setRasterizerDiscardEnable(vk::False);
	rasterizer.setLineWidth(1.0f);
	rasterizer.setDepthBiasEnable(vk::False);
	rasterizer.setDepthBiasConstantFactor(0.0f);
	rasterizer.setDepthBiasClamp(0.0f);
	rasterizer.setDepthBiasSlopeFactor(0.0f);

	vk::PipelineMultisampleStateCreateInfo multiSampling{};
	multiSampling.setRasterizationSamples(vk::SampleCountFlagBits::e1);
	multiSampling.setSampleShadingEnable(vk::False);
	multiSampling.setMinSampleShading(1.0f);
	multiSampling.setAlphaToCoverageEnable(vk::False);
	multiSampling.setAlphaToOneEnable(vk::False);

	vk::PipelineColorBlendAttachmentState colorBlendAttachment{};
	colorBlendAttachment.setColorWriteMask(vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA);
	colorBlendAttachment.setBlendEnable(vk::False);
	colorBlendAttachment.setSrcColorBlendFactor(vk::BlendFactor::eOne);
	colorBlendAttachment.setDstColorBlendFactor(vk::BlendFactor::eZero);
	colorBlendAttachment.setColorBlendOp(vk::BlendOp::eAdd);
	colorBlendAttachment.setSrcAlphaBlendFactor(vk::BlendFactor::eOne);
	colorBlendAttachment.setDstAlphaBlendFactor(vk::BlendFactor::eZero);
	colorBlendAttachment.setAlphaBlendOp(vk::BlendOp::eAdd);

	std::vector<vk::PipelineColorBlendAttachmentState> attachments{ colorBlendAttachment };

	vk::PipelineColorBlendStateCreateInfo colorBlending{};
	colorBlending.setLogicOpEnable(vk::False);
	colorBlending.setLogicOp(vk::LogicOp::eCopy);
	colorBlending.setAttachments(attachments);
	colorBlending.setBlendConstants({ 0 });

	vk::PipelineDepthStencilStateCreateInfo depthStencil{};
	depthStencil.setDepthTestEnable(vk::True);
	depthStencil.setDepthWriteEnable(vk::True);
	depthStencil.setDepthCompareOp(vk::CompareOp::eLess);
	depthStencil.setDepthBoundsTestEnable(vk::False);
	depthStencil.setMinDepthBounds(0.0f);
	depthStencil.setMaxDepthBounds(1.0f);
	depthStencil.setStencilTestEnable(vk::False);
	depthStencil.setFront(vk::StencilOpState{});
	depthStencil.setBack(vk::StencilOpState{});

	std::vector<vk::DescriptorSetLayout> setLayouts = { *descriptorSetLayout, *meshlet_descriptorSetLayout };
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo{};
	pipelineLayoutInfo.setSetLayouts(setLayouts);
	//TODO
	vk::PushConstantRange pushConstantRange{};
	pushConstantRange.setOffset(0);
	pushConstantRange.setSize(sizeof(DrawPushConstant));
	pushConstantRange.setStageFlags(vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment);
	pipelineLayoutInfo.setPushConstantRanges({ /*pushConstantRange */ });
	pipelineLayout = app->context.logical.createPipelineLayout(pipelineLayoutInfo);

	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.setStages(shaderStages);
	pipelineInfo.setPVertexInputState(&vertexInputInfo);
	pipelineInfo.setPInputAssemblyState(&inputAssembly);
	pipelineInfo.setPViewportState(&viewportState);
	pipelineInfo.setPRasterizationState(&rasterizer);
	pipelineInfo.setPDepthStencilState(&depthStencil);
	pipelineInfo.setPMultisampleState(&multiSampling);
	pipelineInfo.setPColorBlendState(&colorBlending);
	pipelineInfo.setPDynamicState(&dynamicState);
	pipelineInfo.setLayout(pipelineLayout);
	pipelineInfo.setRenderPass(renderPass);
	pipelineInfo.setSubpass(0);
	pipelineInfo.setBasePipelineHandle(VK_NULL_HANDLE);
	pipelineInfo.setBasePipelineIndex(-1);

	pipeline = app->context.logical.createGraphicsPipeline(nullptr, pipelineInfo);
}

void RenderManager::createDescriptor()
{
	std::vector<vk::DescriptorSetLayoutBinding> bindings;

	vk::DescriptorSetLayoutBinding layoutBinding{};
	layoutBinding.binding = 0;
	layoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = vk::ShaderStageFlagBits::eMeshEXT | vk::ShaderStageFlagBits::eFragment;
	bindings.push_back(layoutBinding);


	vk::DescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.setBindings(bindings);

	descriptorSetLayout = app->context.logical.createDescriptorSetLayout(layoutInfo);

}

void RenderManager::createDepthImage()
{
	vk::ImageCreateInfo imageInfo{};
	imageInfo.setArrayLayers(1);
	imageInfo.setExtent({ app->context.swapchainExtent.width, app->context.swapchainExtent.height, 1 });
	imageInfo.setFormat(vk::Format::eD32Sfloat);
	imageInfo.setImageType(vk::ImageType::e2D);
	imageInfo.setMipLevels(1);
	imageInfo.setSamples(vk::SampleCountFlagBits::e1);
	imageInfo.setTiling(vk::ImageTiling::eOptimal);
	imageInfo.setUsage(vk::ImageUsageFlagBits::eDepthStencilAttachment);
	imageInfo.setInitialLayout(vk::ImageLayout::eUndefined);
	imageInfo.setSharingMode(vk::SharingMode::eExclusive);

	vk::ImageViewCreateInfo imageViewInfo{};
	imageViewInfo.setFormat(imageInfo.format);
	imageViewInfo.setViewType(vk::ImageViewType::e2D);
	imageViewInfo.setSubresourceRange({ vk::ImageAspectFlagBits::eDepth, 0, imageInfo.mipLevels, 0, 1 });

	VmaAllocationCreateInfo allocCreateInfo = {};
	allocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
	allocCreateInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;
	allocCreateInfo.priority = 1.0f;

	depthImage = DDing::Image(imageInfo, allocCreateInfo, imageViewInfo);
}

void RenderManager::InitDescriptorsForMeshlets()
{
	//Layout
	{

		std::vector<vk::DescriptorSetLayoutBinding> bindings;

		vk::DescriptorSetLayoutBinding meshletBinding{};
		meshletBinding.binding = 0;
		meshletBinding.descriptorCount = 1;
		meshletBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		meshletBinding.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
		bindings.push_back(meshletBinding);

		vk::DescriptorSetLayoutBinding meshletVerticesBinding{};
		meshletVerticesBinding.binding = 1;
		meshletVerticesBinding.descriptorCount = 1;
		meshletVerticesBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		meshletVerticesBinding.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
		bindings.push_back(meshletVerticesBinding);

		vk::DescriptorSetLayoutBinding meshletTrianglesBinding{};
		meshletTrianglesBinding.binding = 2;
		meshletTrianglesBinding.descriptorCount = 1;
		meshletTrianglesBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		meshletTrianglesBinding.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
		bindings.push_back(meshletTrianglesBinding);

		vk::DescriptorSetLayoutBinding verticesBinding{};
		verticesBinding.binding = 3;
		verticesBinding.descriptorCount = 1;
		verticesBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		verticesBinding.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
		bindings.push_back(verticesBinding);


		vk::DescriptorSetLayoutBinding LODOffsetsBinding{};
		LODOffsetsBinding.binding = 4;
		LODOffsetsBinding.descriptorCount = 1;
		LODOffsetsBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		LODOffsetsBinding.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
		bindings.push_back(LODOffsetsBinding);


		vk::DescriptorSetLayoutBinding childIndicesBinding{};
		childIndicesBinding.binding = 5;
		childIndicesBinding.descriptorCount = 1;
		childIndicesBinding.descriptorType = vk::DescriptorType::eStorageBuffer;
		childIndicesBinding.stageFlags = vk::ShaderStageFlagBits::eMeshEXT;
		bindings.push_back(childIndicesBinding);

		vk::DescriptorSetLayoutCreateInfo layoutInfo{};
		layoutInfo.setBindings(bindings);

		meshlet_descriptorSetLayout = app->context.logical.createDescriptorSetLayout(layoutInfo);

	}
	//Pool
	{

		std::vector<vk::DescriptorPoolSize> poolSizes = {
			vk::DescriptorPoolSize(vk::DescriptorType::eUniformBuffer,1),
			vk::DescriptorPoolSize(vk::DescriptorType::eStorageBuffer,16)
		};

		vk::DescriptorPoolCreateInfo poolInfo{};
		poolInfo.setMaxSets(FRAME_CNT);
		poolInfo.setPoolSizes(poolSizes);
		poolInfo.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet | vk::DescriptorPoolCreateFlagBits::eUpdateAfterBind);

		meshlet_descriptorPool = app->context.logical.createDescriptorPool(poolInfo);
	}
	//Set
	{
		vk::DescriptorSetAllocateInfo allocInfo{};
		allocInfo.setDescriptorPool(*meshlet_descriptorPool);
		allocInfo.setDescriptorSetCount(1);
		allocInfo.setSetLayouts(*meshlet_descriptorSetLayout);

		meshlet_descriptorSet = std::move(app->context.logical.allocateDescriptorSets(allocInfo).front());
	}
	//Update
	{

		std::vector<vk::WriteDescriptorSet> descriptorWrites;

		std::vector<vk::DescriptorBufferInfo> cluster_Infos;
		{
			vk::DescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = app->model.cluster_Buffer.buffer;
			bufferInfo.range = sizeof(Cluster) * app->model.clusters.size();
			bufferInfo.offset = 0;
			cluster_Infos.push_back(bufferInfo);


			vk::WriteDescriptorSet writeDescriptor;
			writeDescriptor.dstSet = *meshlet_descriptorSet;
			writeDescriptor.dstBinding = 0;
			writeDescriptor.dstArrayElement = 0;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = vk::DescriptorType::eStorageBuffer;
			writeDescriptor.pBufferInfo = cluster_Infos.data();
			descriptorWrites.push_back(writeDescriptor);
		}

		std::vector<vk::DescriptorBufferInfo> meshlet_vertices_Infos;
		{
			vk::DescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = app->model.meshlet_vertices_Buffer.buffer;
			bufferInfo.range = sizeof(unsigned int) * app->model.meshlet_vertices.size();
			bufferInfo.offset = 0;
			meshlet_vertices_Infos.push_back(bufferInfo);


			vk::WriteDescriptorSet writeDescriptor;
			writeDescriptor.dstSet = *meshlet_descriptorSet;
			writeDescriptor.dstBinding = 1;
			writeDescriptor.dstArrayElement = 0;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = vk::DescriptorType::eStorageBuffer;
			writeDescriptor.pBufferInfo = meshlet_vertices_Infos.data();
			descriptorWrites.push_back(writeDescriptor);
		}
		std::vector<vk::DescriptorBufferInfo> meshlet_triangles_Infos;
		{
			vk::DescriptorBufferInfo bufferInfo{};
			bufferInfo.buffer = app->model.meshlet_triangles_Buffer.buffer;
			bufferInfo.range = sizeof(unsigned char) * app->model.meshlet_triangles.size();
			bufferInfo.offset = 0;
			meshlet_triangles_Infos.push_back(bufferInfo);


			vk::WriteDescriptorSet writeDescriptor;
			writeDescriptor.dstSet = *meshlet_descriptorSet;
			writeDescriptor.dstBinding = 2;
			writeDescriptor.dstArrayElement = 0;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = vk::DescriptorType::eStorageBuffer;
			writeDescriptor.pBufferInfo = meshlet_triangles_Infos.data();
			descriptorWrites.push_back(writeDescriptor);
		}

		vk::DescriptorBufferInfo vertices_Info;
		{
			vertices_Info.buffer = app->model.vertexBuffer.buffer;
			vertices_Info.offset = 0;
			vertices_Info.range = sizeof(Vertex) * app->model.vertices.size();


			vk::WriteDescriptorSet writeDescriptor;
			writeDescriptor.dstSet = *meshlet_descriptorSet;
			writeDescriptor.dstBinding = 3;
			writeDescriptor.dstArrayElement = 0;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = vk::DescriptorType::eStorageBuffer;
			writeDescriptor.pBufferInfo = &vertices_Info;
			descriptorWrites.push_back(writeDescriptor);
		}
		vk::DescriptorBufferInfo LODOffset_Info;
		{
			LODOffset_Info.buffer = app->model.LODOffset_Buffer.buffer;
			LODOffset_Info.offset = 0;
			LODOffset_Info.range = sizeof(uint32_t) * app->model.LODOffset.size();


			vk::WriteDescriptorSet writeDescriptor;
			writeDescriptor.dstSet = *meshlet_descriptorSet;
			writeDescriptor.dstBinding = 4;
			writeDescriptor.dstArrayElement = 0;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = vk::DescriptorType::eStorageBuffer;
			writeDescriptor.pBufferInfo = &LODOffset_Info;
			descriptorWrites.push_back(writeDescriptor);

		}
		vk::DescriptorBufferInfo childIndices_Info;
		{

			childIndices_Info.buffer = app->model.childIndices_Buffer.buffer;
			childIndices_Info.offset = 0;
			childIndices_Info.range = sizeof(uint32_t) * app->model.childIndices.size();


			vk::WriteDescriptorSet writeDescriptor;
			writeDescriptor.dstSet = *meshlet_descriptorSet;
			writeDescriptor.dstBinding = 5;
			writeDescriptor.dstArrayElement = 0;
			writeDescriptor.descriptorCount = 1;
			writeDescriptor.descriptorType = vk::DescriptorType::eStorageBuffer;
			writeDescriptor.pBufferInfo = &childIndices_Info;
			descriptorWrites.push_back(writeDescriptor);

		}

		app->context.logical.updateDescriptorSets(descriptorWrites, nullptr);
	}
}

void RenderManager::createFramebuffers()
{
	for (int i = 0; i < app->context.swapchainImages.size(); i++) {
		std::array<vk::ImageView, 2> attachments{
			app->context.swapchainImageViews[i],
			depthImage.imageView
		};
		vk::FramebufferCreateInfo framebufferInfo{};
		framebufferInfo.setRenderPass(renderPass);
		framebufferInfo.setAttachments(attachments);
		framebufferInfo.setWidth(app->context.swapchainExtent.width);
		framebufferInfo.setHeight(app->context.swapchainExtent.height);
		framebufferInfo.setLayers(1);

		framebuffers.emplace_back(app->context.logical, framebufferInfo);
	}
}

void RenderManager::updateUniform(vk::CommandBuffer commandBuffer)
{
	auto& frameData = frameDatas[currentFrame];

	GlobalBuffer uniformBuffer{};
	uniformBuffer.cameraPosition = camera.worldPosition;
	uniformBuffer.view = DDing::Camera::View;
	uniformBuffer.projection = DDing::Camera::Projection;
	uniformBuffer.transform = glm::rotate(glm::radians(90.0f), glm::vec3(1, 0, 0));
	uniformBuffer.totalClusters = app->model.clusters.size();
	uniformBuffer.currentLOD = app->model.LOD;
	memcpy(frameData.stagingBuffer.GetMappedPtr(), &uniformBuffer, sizeof(GlobalBuffer));

	vk::BufferCopy copyRegion{};
	copyRegion.setSize(sizeof(GlobalBuffer));
	copyRegion.setDstOffset(0);
	copyRegion.setSrcOffset(0);
	commandBuffer.copyBuffer(frameData.stagingBuffer.buffer,
		frameData.uniformBuffer.buffer, copyRegion);
}

void RenderManager::submitCommandBuffer(vk::CommandBuffer commandBuffer)
{
	vk::SubmitInfo submitInfo{};
	FrameData& frameData = frameDatas[currentFrame];

	vk::PipelineStageFlags waitStages[1] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	submitInfo.setCommandBuffers(commandBuffer);
	submitInfo.setSignalSemaphores(*frameData.renderFinish);
	submitInfo.setWaitSemaphores(*frameData.imageAvaiable);
	submitInfo.setWaitDstStageMask(waitStages);

	app->context.GetQueue(DDing::Context::QueueType::eGraphics).submit(submitInfo, *frameData.waitFrame);
};

void RenderManager::presentCommandBuffer(vk::CommandBuffer commandBuffer, uint32_t imageIndex)
{
	vk::PresentInfoKHR presentInfo{};
	FrameData& frameData = frameDatas[currentFrame];

	presentInfo.setSwapchains(*(app->context.swapchain));
	presentInfo.setWaitSemaphores(*frameData.renderFinish);
	presentInfo.setImageIndices(imageIndex);
	presentInfo.setPResults(nullptr);

	auto result = app->context.GetQueue(DDing::Context::QueueType::ePresent).presentKHR(presentInfo);
}
