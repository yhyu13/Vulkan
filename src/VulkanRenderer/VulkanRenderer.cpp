#include "VulkanRenderer.h"

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

	return VK_FALSE;
}
static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
	auto app = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	app->WindowResize();;
}
static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	auto app = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	app->currentMouseScrolOffsetX = float(xoffset);
	app->currentMouseScrolOffsetY = float(yoffset);
	
	
	app->camera->position += app->currentMouseScrolOffsetY * app->camera->camFront *app->camera->zoomSpeed;;
	app->camera->UpdateViewMatrix();

	app->previousMouseScrolOffsetX = app->currentMouseScrolOffsetX;
	app->previousMouseScrolOffsetY = app->currentMouseScrolOffsetY;
}
static void mouse_callback(GLFWwindow* window, double xpos, double ypos)
{
	auto app = reinterpret_cast<VulkanRenderer*>(glfwGetWindowUserPointer(window));
	app->currentMousePosX = float(xpos);
	app->currentMousePosY = float(ypos);

	int dx =   app->previousMousePosX - app->currentMousePosX;
	int dy = app->currentMousePosY - app->previousMousePosY ;
	
	if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
		app->camera->rotation.x += dy  * app->camera->rotationSpeed;
		app->camera->rotation.y -= dx * app->camera->rotationSpeed;
		app->camera->rotation += glm::vec3(dy * app->camera->rotationSpeed, -dx * app->camera->rotationSpeed, 0.0f);
		app->camera->UpdateViewMatrix();
	}
	
	app->previousMousePosX = app->currentMousePosX;
	app->previousMousePosY = app->currentMousePosY;
}
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) 
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}
void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) 
{
	createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	createInfo.pfnUserCallback = debugCallback;
}
std::vector<const char*> getRequiredExtensions() 
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


bool checkValidationLayerSupport() 
{
	uint32_t layerCount;
	vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

	std::vector<VkLayerProperties> availableLayers(layerCount);
	vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

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

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
VulkanRenderer::VulkanRenderer()
{
	
}
VulkanRenderer::~VulkanRenderer()
{
	delete gui;
	delete resourceLoader;
	delete camera;
	delete vulkanHelper;
	delete swapChain;
	

	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(vulkanDevice->logicalDevice, descriptorPool, nullptr);
	}
	DestroyCommandBuffers();
	vkDestroyRenderPass(vulkanDevice->logicalDevice, renderPass, nullptr);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(vulkanDevice->logicalDevice, frameBuffers[i], nullptr);
	}
	vkDestroyImageView(vulkanDevice->logicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(vulkanDevice->logicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(vulkanDevice->logicalDevice, depthStencil.mem, nullptr);

	vkDestroyPipelineCache(vulkanDevice->logicalDevice, pipelineCache, nullptr);

	

	vkDestroySemaphore(vulkanDevice->logicalDevice, semaphores.presentComplete, nullptr);
	vkDestroySemaphore(vulkanDevice->logicalDevice, semaphores.renderComplete, nullptr);
	for (auto& fence : waitFences) {
		vkDestroyFence(vulkanDevice->logicalDevice, fence, nullptr);
	}

	//
	delete vulkanDevice;
	//

	if (enableValidationLayers)
	{
		DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
	}

	vkDestroyInstance(instance, nullptr);

	glfwDestroyWindow(window);

	glfwTerminate();
}
void VulkanRenderer::CreateInstance()
{
	if (enableValidationLayers && !checkValidationLayerSupport()) {
		throw std::runtime_error("validation layers requested, but not available!");
	}

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = appName.c_str();
	appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.pEngineName = engineName.c_str();
	appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	appInfo.apiVersion = VK_API_VERSION_1_0;

	VkInstanceCreateInfo createInfo = {};
	createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	createInfo.pApplicationInfo = &appInfo;

	auto extensions = getRequiredExtensions();

	createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	createInfo.ppEnabledExtensionNames = extensions.data();

	VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
	if (enableValidationLayers) {
		createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
		createInfo.ppEnabledLayerNames = validationLayers.data();

		populateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo;
	}
	else {
		createInfo.enabledLayerCount = 0;

		createInfo.pNext = nullptr;
	}

	if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
		throw std::runtime_error("failed to create instance!");
	}
}

void VulkanRenderer::SetupDebugMessenger()
{
	if (!enableValidationLayers) return;

	VkDebugUtilsMessengerCreateInfoEXT createInfo;
	populateDebugMessengerCreateInfo(createInfo);

	VkResult res;
	res = CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("failed to set up debug messenger!");
	}
}

void VulkanRenderer::CreateSurface()
{
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
	}
}

void VulkanRenderer::PickPhysicalDevice()
{
	
	uint32_t deviceCount = 0;
	vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

	if (deviceCount == 0) {
		throw std::runtime_error("failed to find GPUs with Vulkan support!");
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
	//
	std::cout.flush();
	//
	std::cout << "Select Available Vulkan devices" << std::endl;
	for (int i = 0;i < devices.size();++i)
	{
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(devices[i], &deviceProperties);
		std::cout << i << ". " << deviceProperties.deviceName << std::endl;
	}
	std::cout << "Enter Device number and press Enter" << std::endl;
	UINT selectedDevice;
	std::cin >> selectedDevice;
	vulkanDevice = new VulkanDevice(&instance);
	vulkanDevice->physicalDevice = devices[selectedDevice];
	//
	if (vulkanDevice->physicalDevice == VK_NULL_HANDLE) {
		throw std::runtime_error("failed to find a suitable GPU!");
	}
	vkGetPhysicalDeviceProperties(vulkanDevice->physicalDevice, &vulkanDevice->physicalDeviceProperties);
	vkGetPhysicalDeviceFeatures(vulkanDevice->physicalDevice, &vulkanDevice->physicalDeviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(vulkanDevice->physicalDevice, &vulkanDevice->physicalDeviceMemoryProperties);

	vulkanDevice->Initialize();

}

void VulkanRenderer::CreateLogicalDevice()
{
	// Derived 
	GetEnabledDeviceFeatures();
	vulkanDevice->CreateLogicalDevice(enabledFeatures, enabledDeviceExtensions, deviceCreatepNextChain);
	
	// Get a graphics queue from the device
	vkGetDeviceQueue(vulkanDevice->logicalDevice, vulkanDevice->queueFamilyIndices.graphics, 0, &queue.graphicQueue);
	// Get a compute queue from the device
	vkGetDeviceQueue(vulkanDevice->logicalDevice, vulkanDevice->queueFamilyIndices.compute, 0, &queue.computeQueue);
	// Get a transfer queue from the device
	vkGetDeviceQueue(vulkanDevice->logicalDevice, vulkanDevice->queueFamilyIndices.transfer, 0, &queue.transferQueue);

	VkBool32 validDepthFormat = GetSupportedDepthFormat(vulkanDevice->physicalDevice, &depthFormat);
	assert(validDepthFormat);

}

void VulkanRenderer::CreateSwapChain()
{

	swapChain = new SwapChain(vulkanDevice,&surface);
}

void VulkanRenderer::SetupSwapChain()
{
	swapChain->SetupSwapChain(&width,&height,vsync);
}

void VulkanRenderer::InitWindow()
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	window = glfwCreateWindow(width, height, "Boulevards Graphic Renderer", nullptr, nullptr);
	glfwSetWindowUserPointer(window, this);
	glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	glfwSetCursorPosCallback(window, mouse_callback);
	glfwSetScrollCallback(window, scroll_callback);
}

void VulkanRenderer::InitVulkan()
{
	CreateInstance();
	SetupDebugMessenger();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	resourceLoader = new ResourceLoader(vulkanDevice);
	camera = new Camera();
	vulkanHelper = new VulkanHelper();
	gui = new GUI(vulkanDevice, vulkanHelper);
}

void VulkanRenderer::CreatePipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VkResult res = vkCreatePipelineCache(vulkanDevice->logicalDevice, &pipelineCacheCreateInfo, nullptr, &pipelineCache);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Pipeline Cache");
	}
}

void VulkanRenderer::mainLoop()
{
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

void VulkanRenderer::CreateCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	drawCmdBuffers.resize(swapChain->imageCount);
	//
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = vulkanDevice->defaultCommandPool;
	cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufAllocateInfo.commandBufferCount = static_cast<uint32_t>(drawCmdBuffers.size());
	
	VkResult res =  vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufAllocateInfo, drawCmdBuffers.data());
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Allocate Command Buffers");
	}

}

void VulkanRenderer::SynchronizationPrimitives()
{
	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo{};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	VkResult res = vkCreateSemaphore(vulkanDevice->logicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Semaphore");
	}
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	res = vkCreateSemaphore(vulkanDevice->logicalDevice, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Semaphore");
	}
	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphores.presentComplete;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphores.renderComplete;
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// Wait fences to sync command buffer access
	VkFenceCreateInfo fenceCreateInfo{};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
	waitFences.resize(drawCmdBuffers.size());
	for (auto& fence : waitFences) 
	{
		VkResult res = vkCreateFence(vulkanDevice->logicalDevice, &fenceCreateInfo, nullptr, &fence);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Fence");
		}
	}
}



void VulkanRenderer::WindowResize()
{

	if (!prepared)
	{
		return;
	}
	prepared = false;

	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(vulkanDevice->logicalDevice);
	//Glfw Window Resize width and heigth
	int glfwWidth = 0, glfwHeight = 0;
	while (glfwWidth == 0 || glfwHeight == 0) {
		glfwGetFramebufferSize(window, &glfwWidth, &glfwHeight);
		glfwWaitEvents();
	}



	// Recreate swap chain
	width = glfwWidth;
	height = glfwHeight;
	SetupSwapChain();

	// Recreate the frame buffers
	vkDestroyImageView(vulkanDevice->logicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(vulkanDevice->logicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(vulkanDevice->logicalDevice, depthStencil.mem, nullptr);
	SetupDepthStencil();
	for (uint32_t i = 0; i < frameBuffers.size(); i++) {
		vkDestroyFramebuffer(vulkanDevice->logicalDevice, frameBuffers[i], nullptr);
	}
	SetupFrameBuffer();

	if ((width > 0.0f) && (height > 0.0f)) 
	{
		
			gui->Resize(width, height);
	}

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	DestroyCommandBuffers();
	CreateCommandBuffers();
	BuildCommandBuffers();




	// Notify derived class
	windowResized();


	prepared = true;
}

void VulkanRenderer::Render()
{
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		ProcessInput(window);
		RenderFrame();
	}
	
	vkDeviceWaitIdle(vulkanDevice->logicalDevice);
}

void VulkanRenderer::RenderFrame()
{
	auto tStart = std::chrono::high_resolution_clock::now();
	Update();
	Draw();

	frameCounter++;
	auto tEnd = std::chrono::high_resolution_clock::now();
	auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
	frameTimer = (float)tDiff / 1000.0f;
	
	//float currentFrame = (float)glfwGetTime();
	//deltaTime = tDiff;
	camera->Update(frameTimer);

	

	float fpsTimer = (float)(std::chrono::duration<double, std::milli>(tEnd - lastTimestamp).count());

	//lastFrame = currentFrame;
	if (fpsTimer > 1000.0f)
	{
		lastFPS = static_cast<uint32_t>((float)frameCounter * (1000.0f / fpsTimer));
		frameCounter = 0;
		lastTimestamp = tEnd;
	}

	
	UpdateGuiDetails();
}

void VulkanRenderer::ProcessInput(GLFWwindow * window)
{
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);

	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
	{
		camera->keys.up = true;
	}
	else
	{
		camera->keys.up = false;
	}

	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
	{
		camera->keys.down = true;
	}
	else
	{
		camera->keys.down = false;
	}

	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
	{
		camera->keys.left = true;
	}
	else
	{
		camera->keys.left = false;
	}

	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
	{
		camera->keys.right = true;
	}
	else
	{
		camera->keys.right = false;
	}
	
}



VkBool32 VulkanRenderer::GetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat * depthFormat)
{

	// Since all depth formats may be optional, we need to find a suitable depth format to use
	// Start with the highest precision packed format
	std::vector<VkFormat> depthFormats = {
		VK_FORMAT_D32_SFLOAT_S8_UINT,
		VK_FORMAT_D32_SFLOAT,
		VK_FORMAT_D24_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM_S8_UINT,
		VK_FORMAT_D16_UNORM
	};

	for (auto& format : depthFormats)
	{
		VkFormatProperties formatProps;
		vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &formatProps);
		// Format must support depth stencil attachment for optimal tiling
		if (formatProps.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			*depthFormat = format;
			return true;
		}
	}

	return false;
}

void VulkanRenderer::DrawGUI(const VkCommandBuffer commandBuffer)
{
	VkViewport viewport{};
	viewport.width = (float)width;
	viewport.height = (float)height;
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	
	VkRect2D scissor{};
	scissor.extent.width = width;
	scissor.extent.height = height;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	
	vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

	gui->Draw(commandBuffer);
}

void VulkanRenderer::GetEnabledDeviceFeatures()
{
}

void VulkanRenderer::Prepare()
{
	CreateSwapChain();
	SetupSwapChain();
	CreateCommandBuffers();
	SynchronizationPrimitives();
	SetupDepthStencil();
	SetupRenderPass();
	CreatePipelineCache();
	SetupFrameBuffer();

	// Setup GUI
	gui->queue = queue.graphicQueue;

	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");

	std::string fileName = std::string(buffer).substr(0, pos);
	std::size_t found = fileName.find("PC Visual Studio");
	if (found != std::string::npos)
	{
			fileName.erase(fileName.begin() + found, fileName.end());
	}
	std::string fileNameVert = fileName + "\\src\\VulkanRenderer\\Shaders\\Gui.vert.spv";
	std::string fileNameFrag = fileName + "\\src\\VulkanRenderer\\Shaders\\Gui.frag.spv";

	VkPipelineShaderStageCreateInfo vertShaderStage = {};
	vertShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertShaderStage.module = resourceLoader->LoadShader(fileNameVert.c_str());
	vertShaderStage.pName = "main";
	assert(vertShaderStage.module != VK_NULL_HANDLE);
	
	VkPipelineShaderStageCreateInfo fragShaderStage = {};
	fragShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragShaderStage.module = resourceLoader->LoadShader(fileNameFrag.c_str());;
	fragShaderStage.pName = "main";
	assert(fragShaderStage.module != VK_NULL_HANDLE);
	
	gui->shaders.push_back(vertShaderStage);
	gui->shaders.push_back(fragShaderStage);
		

	gui->PrepareResources();
	gui->PreparePipeline(pipelineCache, renderPass);

	vkDestroyShaderModule(vulkanDevice->logicalDevice, gui->shaders[0].module, nullptr);
	vkDestroyShaderModule(vulkanDevice->logicalDevice, gui->shaders[1].module, nullptr);

}

void VulkanRenderer::SetupDepthStencil()
{
	VkImageCreateInfo imageCI{};
	imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCI.imageType = VK_IMAGE_TYPE_2D;
	imageCI.format = depthFormat;
	imageCI.extent = { width, height, 1 };
	imageCI.mipLevels = 1;
	imageCI.arrayLayers = 1;
	imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCI.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

	VkResult res = vkCreateImage(vulkanDevice->logicalDevice, &imageCI, nullptr, &depthStencil.image);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Image");
	}
	VkMemoryRequirements memReqs{};
	vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, depthStencil.image, &memReqs);

	VkMemoryAllocateInfo memAllloc{};
	memAllloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memAllloc.allocationSize = memReqs.size;
	memAllloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAllloc, nullptr, &depthStencil.mem);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Allocate Memory");
	}
	res = vkBindImageMemory(vulkanDevice->logicalDevice, depthStencil.image, depthStencil.mem, 0);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Bind Image Memory");
	}
	VkImageViewCreateInfo imageViewCI{};
	imageViewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCI.image = depthStencil.image;
	imageViewCI.format = depthFormat;
	imageViewCI.subresourceRange.baseMipLevel = 0;
	imageViewCI.subresourceRange.levelCount = 1;
	imageViewCI.subresourceRange.baseArrayLayer = 0;
	imageViewCI.subresourceRange.layerCount = 1;
	imageViewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	// Stencil aspect should only be set on depth + stencil formats (VK_FORMAT_D16_UNORM_S8_UINT..VK_FORMAT_D32_SFLOAT_S8_UINT
	if (depthFormat >= VK_FORMAT_D16_UNORM_S8_UINT) {
		imageViewCI.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
	}
	res = vkCreateImageView(vulkanDevice->logicalDevice, &imageViewCI, nullptr, &depthStencil.view);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Image View");
	}
}

void VulkanRenderer::SetupRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = swapChain->colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VkResult res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &renderPass);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Render Pass");
	}
}

void VulkanRenderer::SetupFrameBuffer()
{
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	frameBuffers.resize(swapChain->imageCount);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		attachments[0] = swapChain->buffers[i].view;
		VkResult res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &frameBufferCreateInfo, nullptr, &frameBuffers[i]);
	}
}

void VulkanRenderer::DestroyCommandBuffers()
{
	vkFreeCommandBuffers(vulkanDevice->logicalDevice, vulkanDevice->defaultCommandPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

void VulkanRenderer::BuildCommandBuffers()
{
}

void VulkanRenderer::windowResized()
{
}

void VulkanRenderer::GuiUpdate(GUI * gui)
{
}

void VulkanRenderer::UpdateGuiDetails()
{
	ImGuiIO& io = ImGui::GetIO();

	io.DisplaySize = ImVec2((float)width, (float)height);
	io.DeltaTime = frameTimer;

	io.MousePos = ImVec2(currentMousePosX, currentMousePosY);
	io.MouseDown[0] = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

	io.MouseDown[1] = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

	ImGui::NewFrame();

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::SetNextWindowPos(ImVec2(10, 10));
	ImGui::SetNextWindowSize(ImVec2(150, 150), 4);
	ImGui::Begin("Boulevards Graphic Renderer", nullptr, ImGuiWindowFlags_None| ImGuiWindowFlags_NoBackground);
	ImGui::TextUnformatted(appName.c_str());
	ImGui::TextUnformatted(vulkanDevice->physicalDeviceProperties.deviceName);
	int FPS = int((((1 / frameTimer) * 60) * 16) / 1000);
	ImGui::Text("ms = %f \nFrame = %d fps", (1000.0f/lastFPS), lastFPS);

	//ImGui::Text("\nFrame = %d fps",  59);
	ImGui::PushItemWidth(110.0f * gui->scale);
	GuiUpdate(gui);
	ImGui::PopItemWidth();


	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::Render();

	if (gui->UpdateGuiDetails() || gui->updated) {
		BuildCommandBuffers();
		gui->updated = false;
	}
}



