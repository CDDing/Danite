#include "pch.h"
#include "Context.h"
DDing::Context::Context(GLFWwindow* window)
	:
	window(window)
{
	createInstance();
	createDebugMessenger();
	createSurface();
	createPhysicalDevice();
	createLogicalDevice();
	createSwapchain();
	createVmaAllocator();
	createImmediateResources();
}

void DDing::Context::immediate_submit(std::function<void(vk::CommandBuffer commandBuffer)>&& function)
{
	logical.resetFences(*immediate.fence);
	immediate.commandBuffer.reset({});

	vk::CommandBufferBeginInfo beginInfo{};
	beginInfo.setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);

	immediate.commandBuffer.begin(beginInfo);

	function(*immediate.commandBuffer);

	immediate.commandBuffer.end();

	vk::SubmitInfo submitInfo{};
	submitInfo.setCommandBuffers({ *immediate.commandBuffer });
	GetQueue(QueueType::eGraphics).submit(submitInfo, *immediate.fence);

	auto result = logical.waitForFences(*immediate.fence, true, UINT64_MAX);
}

void DDing::Context::createInstance()
{
	if(enableValidationLayers && !checkValidationLayerSupport())
		throw std::runtime_error("validation layers requested, but not available!");

	vk::ApplicationInfo appInfo{ "DDing",
	VK_MAKE_VERSION(1,0,0),
	"No Engine",
	VK_MAKE_VERSION(1,0,0),
	VK_API_VERSION_1_3 };

	vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo = createDebugMessengerInfo();
	vk::InstanceCreateInfo createInfo{};
	createInfo.setPApplicationInfo(&appInfo);
	
	if (enableValidationLayers) {
		createInfo.setPEnabledLayerNames(validationLayers);
		createInfo.pNext = &debugCreateInfo;
	}

	auto extensions = getRequiredExtensions();
	createInfo.setPEnabledExtensionNames(extensions);


	instance = vk::raii::Instance(context,createInfo);
}

void DDing::Context::createDebugMessenger()
{
	if (!enableValidationLayers) return;
	vk::DebugUtilsMessengerCreateInfoEXT createInfo = createDebugMessengerInfo();
	debug = instance.createDebugUtilsMessengerEXT(createInfo);
}

void DDing::Context::createSurface()
{
	VkSurfaceKHR cSurface;
	VkResult result = glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &cSurface);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}

	surface = vk::raii::SurfaceKHR(instance, cSurface);
}

void DDing::Context::createPhysicalDevice()
{
	vk::raii::PhysicalDevices devices(instance);
	for (const auto& device : devices) {
		if (isDeviceSuitable(device, surface)) {
			physical = device;
			break;
		}
	}

	if (physical == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}

}

void DDing::Context::createLogicalDevice()
{
	QueueFamilyIndices indices = findQueueFamilies(physical, surface);

	std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(),indices.presentFamily.value() };

	float queuePriority = 1.f;
	for (uint32_t queueFamiliy : uniqueQueueFamilies) {
		vk::DeviceQueueCreateInfo queueCreateInfo{ {},queueFamiliy,1, &queuePriority };
		queueCreateInfos.push_back(queueCreateInfo);
	}

	vk::PhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
	meshShaderFeatures.meshShader = vk::True;


	vk::PhysicalDeviceVulkan12Features physicaldevice12features;
	physicaldevice12features.storageBuffer8BitAccess = vk::True;
	physicaldevice12features.runtimeDescriptorArray = VK_TRUE;
	physicaldevice12features.descriptorBindingPartiallyBound = vk::True;
	physicaldevice12features.descriptorBindingVariableDescriptorCount = vk::True;
	physicaldevice12features.shaderSampledImageArrayNonUniformIndexing = vk::True;
	physicaldevice12features.descriptorBindingSampledImageUpdateAfterBind = vk::True;
	physicaldevice12features.shaderUniformBufferArrayNonUniformIndexing = vk::True;
	physicaldevice12features.descriptorBindingUniformBufferUpdateAfterBind = vk::True;
	physicaldevice12features.shaderStorageBufferArrayNonUniformIndexing = vk::True;
	physicaldevice12features.descriptorBindingStorageBufferUpdateAfterBind = vk::True;
	physicaldevice12features.bufferDeviceAddress = vk::True;
	physicaldevice12features.descriptorIndexing = vk::True;
	physicaldevice12features.pNext = &meshShaderFeatures;

	vk::PhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
	rayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;  // 기능 활성화
	rayTracingPipelineFeatures.pNext = &physicaldevice12features;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures = {};
	accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	accelerationStructureFeatures.accelerationStructure = VK_TRUE;
	
	accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;

	VkPhysicalDeviceFeatures2 deviceFeatures{};
	deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
	deviceFeatures.features.samplerAnisotropy = VK_TRUE;
	deviceFeatures.features.shaderInt64 = VK_TRUE;
	deviceFeatures.pNext = &accelerationStructureFeatures;

	std::vector<const char*> validationlayers;
	if (enableValidationLayers) {
		validationlayers = validationLayers;
	}
	vk::DeviceCreateInfo createInfo{ {},queueCreateInfos,validationLayers,deviceExtensions };
	createInfo.pNext = &deviceFeatures;

	logical = physical.createDevice(createInfo);

	for (int i = 0; i < QueueType::END; i++) {
		vk::Queue q;
		queues.push_back(q);
	}

	GetQueue(QueueType::eGraphics) = logical.getQueue(indices.graphicsFamily.value(), 0);
	GetQueue(QueueType::ePresent) = logical.getQueue(indices.presentFamily.value(), 0);

}

void DDing::Context::createVmaAllocator()
{
	VmaAllocatorCreateInfo allocatorInfo{};
	allocatorInfo.physicalDevice = *physical;
	allocatorInfo.device = *logical;
	allocatorInfo.instance = *instance;
	allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

	vmaCreateAllocator(&allocatorInfo, &allocator);
}
void DDing::Context::createSwapchain()
{
	SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physical, surface);

	vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
	vk::PresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
	vk::Extent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

	uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

	if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
		imageCount = swapChainSupport.capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR createInfo{};
	createInfo.surface = *surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eTransferDst;

	QueueFamilyIndices indices = findQueueFamilies(physical, surface);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	if (indices.graphicsFamily != indices.presentFamily) {
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else {
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
		createInfo.queueFamilyIndexCount = 0; // Optional
		createInfo.pQueueFamilyIndices = nullptr; // Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
	createInfo.presentMode = presentMode;
	createInfo.clipped = vk::True;
	createInfo.oldSwapchain = VK_NULL_HANDLE;

	swapchain = logical.createSwapchainKHR(createInfo);
	swapchainImages = swapchain.getImages();
	swapchainFormat = surfaceFormat.format;
	swapchainExtent = extent;

	for (size_t i = 0; i < swapchainImages.size(); i++) {

		/*context->immediate_submit([&](vk::CommandBuffer commandBuffer) {
			DDing::Image::setImageLayout(commandBuffer, images[i], vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);
			});*/

		vk::ImageViewCreateInfo createInfo{};
		createInfo.image = swapchainImages[i];
		createInfo.viewType = vk::ImageViewType::e2D;
		createInfo.format = swapchainFormat;
		createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1;
		swapchainImageViews.push_back(vk::raii::ImageView(logical, createInfo));
	}
}
void DDing::Context::createImmediateResources()
{
	vk::FenceCreateInfo fenceInfo{};
	fenceInfo.setFlags(vk::FenceCreateFlagBits::eSignaled);
	immediate.fence = vk::raii::Fence(logical, fenceInfo);

	QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physical, surface);
	vk::CommandPoolCreateInfo commandPoolInfo{};
	commandPoolInfo.setFlags(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
	commandPoolInfo.setQueueFamilyIndex(queueFamilyIndices.graphicsFamily.value());
	immediate.commandPool = logical.createCommandPool(commandPoolInfo);

	vk::CommandBufferAllocateInfo allocInfo{};
	allocInfo.setCommandBufferCount(1);
	allocInfo.setCommandPool(immediate.commandPool);
	allocInfo.setLevel(vk::CommandBufferLevel::ePrimary);
	immediate.commandBuffer = std::move(logical.allocateCommandBuffers(allocInfo).front());
}
DDing::Context::~Context() {
	vmaDestroyAllocator(allocator);
}
bool DDing::Context::checkValidationLayerSupport()
{
	std::vector<vk::LayerProperties> availableLayers = vk::enumerateInstanceLayerProperties();

	for (const char* layerName : validationLayers) {
		bool layerFound = false;
		for (const auto& layerProperties : availableLayers) {
			if (strcmp(layerName, layerProperties.layerName) == 0) {
				layerFound = true;
				break;
			}
		}
		if (!layerFound) {
			return false;
		}
	}
	return true;
}

std::vector<const char*> DDing::Context::getRequiredExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** glfwExtensions;
	glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

	if (enableValidationLayers) {
		extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	return extensions;
}

vk::DebugUtilsMessengerCreateInfoEXT DDing::Context::createDebugMessengerInfo()
{
	vk::DebugUtilsMessengerCreateInfoEXT createInfo;
	createInfo.messageSeverity =
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
	createInfo.messageType =
		vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
	createInfo.pfnUserCallback = debugCallback;
	return createInfo;
}

bool DDing::Context::isDeviceSuitable(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{

    QueueFamilyIndices indices = findQueueFamilies(device, surface);

    bool extensionsSupported = true;//checkDeviceExtensionSupport(device);

    bool swapChainAdequate = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    vk::PhysicalDeviceFeatures supportedFeatures = device.getFeatures();
    return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
}

QueueFamilyIndices DDing::Context::findQueueFamilies(vk::PhysicalDevice device, vk::SurfaceKHR surface)
{
	QueueFamilyIndices indices;

	std::vector<vk::QueueFamilyProperties> queueFamilies =
		device.getQueueFamilyProperties();;

	int i = 0;
	for (const auto& queueFamily : queueFamilies) {
		if (queueFamily.queueFlags && VK_QUEUE_GRAPHICS_BIT) {
			indices.graphicsFamily = i;
		}
		VkBool32 presentSupport = device.getSurfaceSupportKHR(i, surface);
		if (presentSupport) {
			indices.presentFamily = i;
		}
		if (indices.isComplete()) {
			break;
		}
		i++;
	}
	return indices;

}

VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT messageSeverity, vk::DebugUtilsMessageTypeFlagsEXT messageTypes, vk::DebugUtilsMessengerCallbackDataEXT const* pCallbackData, void*)
{
	std::ostringstream message;

	message << vk::to_string(messageSeverity) << ": " << vk::to_string(messageTypes) << ":\n";
	message << std::string("\t") << "messageIDName   = <" << pCallbackData->pMessageIdName << ">\n";
	message << std::string("\t") << "messageIdNumber = " << pCallbackData->messageIdNumber << "\n";
	message << std::string("\t") << "message         = <" << pCallbackData->pMessage << ">\n";
	if (0 < pCallbackData->queueLabelCount)
	{
		message << std::string("\t") << "Queue Labels:\n";
		for (uint32_t i = 0; i < pCallbackData->queueLabelCount; i++)
		{
			message << std::string("\t\t") << "labelName = <" << pCallbackData->pQueueLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->cmdBufLabelCount)
	{
		message << std::string("\t") << "CommandBuffer Labels:\n";
		for (uint32_t i = 0; i < pCallbackData->cmdBufLabelCount; i++)
		{
			message << std::string("\t\t") << "labelName = <" << pCallbackData->pCmdBufLabels[i].pLabelName << ">\n";
		}
	}
	if (0 < pCallbackData->objectCount)
	{
		message << std::string("\t") << "Objects:\n";
		for (uint32_t i = 0; i < pCallbackData->objectCount; i++)
		{
			message << std::string("\t\t") << "Object " << i << "\n";
			message << std::string("\t\t\t") << "objectType   = " << vk::to_string(pCallbackData->pObjects[i].objectType) << "\n";
			message << std::string("\t\t\t") << "objectHandle = " << pCallbackData->pObjects[i].objectHandle << "\n";
			if (pCallbackData->pObjects[i].pObjectName)
			{
				message << std::string("\t\t\t") << "objectName   = <" << pCallbackData->pObjects[i].pObjectName << ">\n";
			}
		}
	}
	std::cout << message.str() << std::endl;
	return vk::False;
}

vk::SurfaceFormatKHR DDing::Context::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& availableFormats)
{
	for (const auto& availableFormat : availableFormats) {
		if (availableFormat.format == vk::Format::eB8G8R8A8Unorm && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
			return availableFormat;
		}
	}
	return availableFormats[0];
}

vk::PresentModeKHR DDing::Context::chooseSwapPresentMode(const std::vector<vk::PresentModeKHR>& availablePresentModes)
{
	for (const auto& availablePresentMode : availablePresentModes) {
		if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
			return availablePresentMode;
		}
	}
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D DDing::Context::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}
	else {
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height)
		};

		actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

		return actualExtent;
	}
}
