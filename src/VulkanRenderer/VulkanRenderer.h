#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>

#include "vulkan/vulkan.h"
#include <vector>
#include <array>
#include <iostream>
#include <optional>
#include "VulkanDevice.h"
#include "SwapChain.h"
#include "Camera.h"
#include "VulkanHelper.h"
#include "ResourceLoader.h"
#include "VulkanTexture.h"
#include "VulkanModel.h"
#include "GUI.h"
#include <chrono>
#define DEFAULT_FENCE_TIMEOUT 100000000000
//
const std::vector<const char*> validationLayers = {
	"VK_LAYER_KHRONOS_validation"
};


//
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

//


class VulkanRenderer
{

public:
	//
	GLFWwindow* window;
	//
	VkInstance instance;
	//
	VkDebugUtilsMessengerEXT debugMessenger;
	//
	uint32_t width = 1280;
	//
	uint32_t height = 720;
	//
	std::string engineName = "Boulevards Graphics Renderer";
	//
	std::string appName;;
	//
	VkSurfaceKHR surface;
	//
	VulkanDevice* vulkanDevice;
	//
	ResourceLoader* resourceLoader;
	//
	VulkanHelper* vulkanHelper;
	//
	SwapChain* swapChain;
	//
	bool vsync = false;
	// Set of physical device features to be enabled for this example (must be set in the derived constructor) 
	VkPhysicalDeviceFeatures enabledFeatures{};
	// Set of device extensions to be enabled for this example (must be set in the derived constructor) 
	std::vector<const char*> enabledDeviceExtensions;
	// Optional pNext structure for passing extension structures to device creation 
	void* deviceCreatepNextChain = nullptr;
	// Command buffers used for rendering
	std::vector<VkCommandBuffer> drawCmdBuffers;
	//
	VkCommandBuffer  drawCmdBuffersSSAOGeometry;
	//
	VkCommandBuffer  drawCmdBuffersSSAOPass;
	//
	VkCommandBuffer  drawCmdBuffersSSAOBlurPass;

	std::vector<VkFence> waitFences;
	//
	// Depth buffer format (selected during Vulkan initialization)
	VkFormat depthFormat;
	// Global render pass for frame buffer writes
	VkRenderPass renderPass;
	//
	// Pipeline cache object
	VkPipelineCache pipelineCache;
	// List of available frame buffers (same as number of swap chain images)
	std::vector<VkFramebuffer>frameBuffers;
	//
	//
	bool prepared = false;
	// Pipeline stages used to wait at for graphics queue submissions
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	// Descriptor set pool
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	VkDescriptorPool computeDescriptorPool = VK_NULL_HANDLE;

	// Active frame buffer index
	uint32_t currentBuffer = 0;
	//
	VkClearColorValue defaultClearColor = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	//
	float currentMousePosX = 0;
	float currentMousePosY = 0;
	float previousMousePosX = 0;
	float previousMousePosY = 0;
	float currentMouseScrolOffsetX = 0;
	float currentMouseScrolOffsetY = 0;
	float previousMouseScrolOffsetX = 0;
	float previousMouseScrolOffsetY = 0;

	float frameTimer = 1.0f;
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	std::chrono::time_point<std::chrono::high_resolution_clock> lastTimestamp;
	///////////////////////////////////////////////
//////////////////////////////////////////////
///////////Camera Stuff/////////////////////

// timing
	float deltaTime = 0.0f;	// time between current frame and last frame
	float lastFrame = 0.0f;
	
	Camera* camera;
	float lastX = width / 2.0f;
	float lastY = height / 2.0f;
	bool firstMouse = true;
	
	glm::fvec2 previousMousePos = glm::fvec2(0.0f, 0.0f);
	/////////// window resize//////////
	bool resizing = false;

	/////////////////////////////////////////
	//////////UI stuff///////////////
	GUI* gui;
/////////////////////////////////////
public:
	//
	VulkanRenderer();
	//
	~VulkanRenderer();
	//
	void CreateInstance();
	//
	void SetupDebugMessenger();
	//
	void CreateSurface();
	//
	void PickPhysicalDevice();
	//
	void CreateLogicalDevice();
	//
	void CreateSwapChain();
	//
	void SetupSwapChain();
	//
	void InitWindow();
	//
	void InitVulkan();
	//
	void CreatePipelineCache();
	//
	void mainLoop();
	// Create command buffers for drawing commands
	void CreateCommandBuffers();
	//
	void SynchronizationPrimitives();

	// Called if the window is resized and some resources have to be recreatesd
	void WindowResize();
	// Start Render
	void Render();
	//
	// Render one frame of a render loop on platforms that sync rendering
	void RenderFrame();
	//
	void ProcessInput(GLFWwindow *window);

	//
	VkBool32 GetSupportedDepthFormat(VkPhysicalDevice physicalDevice, VkFormat *depthFormat);
	//
	void DrawGUI(const VkCommandBuffer commandBuffer);




	//
	virtual void GetEnabledDeviceFeatures();
	// Prepare commonly used Vulkan functions
	virtual void Prepare();
	//
	// Setup default depth and stencil views
	virtual void SetupDepthStencil();
	// Setup a default render pass
	// Can be overriden in derived class to setup a custom render pass (e.g. for MSAA)
	virtual void SetupRenderPass();
	// Create framebuffers for all requested swap chain images
	// Can be overriden in derived class to setup a custom framebuffer (e.g. for MSAA)
	virtual void SetupFrameBuffer();
	// Destroy all command buffers and set their handles to VK_NULL_HANDLE
	// May be necessary during runtime if options are toggled 
	void DestroyCommandBuffers();
	//
	// Pure virtual function to be overriden by the dervice class
	// Called in case of an event where e.g. the framebuffer has to be rebuild and thus
	// all command buffers that may reference this
	virtual void BuildCommandBuffers();
	// Called when the window has been resized
// Can be overriden in derived class to recreate or rebuild resources attached to the frame buffer / swapchain
	virtual void windowResized();
	// Pure virtual render function (override in derived class)
	virtual void Draw() = 0;
	// Pure virtual render function (override in derived class)
	virtual void Update() = 0;
	// Containing view dependant matrices

	//
	virtual void GuiUpdate(GUI *gui);
	void UpdateGuiDetails();
	///////////////////////////////////////////////////////////////////////////////////////////////////

	//
		// Synchronization semaphores
	struct {
		// Swap chain image presentation
		VkSemaphore presentComplete;
		// Command buffer submission and execution
		VkSemaphore renderComplete;
	} semaphores;
	//
	struct
	{
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} depthStencil;
	//
	struct
	{
		// Handle to the device graphics queue that command buffers are submitted to
		VkQueue graphicQueue;
		// Handle to the device graphics queue that command buffers are submitted to
		VkQueue computeQueue;
		// Handle to the device graphics queue that command buffers are submitted to
		VkQueue transferQueue;
	} queue;
	
	
};





// Windows entry point
#define VULKAN_RENDERER()							\
int main()										    \
{													\
try {												\
		UnitTest *unitTest;							\
		unitTest = new UnitTest();					\
		unitTest->InitWindow();						\
		unitTest->InitVulkan();						\
		unitTest->Prepare();						\
		unitTest->Render();							\
		delete(unitTest);							\
}													\
catch (const std::exception& e) {					\
			std::cerr << e.what() << std::endl;		\
			return EXIT_FAILURE;					\
	}												\
		return EXIT_SUCCESS;						\
}			