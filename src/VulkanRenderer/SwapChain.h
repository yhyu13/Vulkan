#pragma once
#include "vulkan/vulkan.h"
#include "VulkanDevice.h"


typedef struct _SwapChainBuffers {
	VkImage image;
	VkImageView view;
} SwapChainBuffer;

class SwapChain
{
public:
	//
	VulkanDevice* vulkanDevice;
	//
	VkSurfaceKHR* surface;
	//
	// Queue family index of the detected graphics and presenting device queue 
	uint32_t queueNodeIndex = UINT32_MAX;
	//
	VkFormat colorFormat;
	//
	VkColorSpaceKHR colorSpace;
	//
	/** @brief Handle to the current swap chain, required for recreation */
	VkSwapchainKHR currentSwapChain = VK_NULL_HANDLE;
	//
	uint32_t imageCount;
	//
	std::vector<VkImage> images;
	//
	std::vector<SwapChainBuffer> buffers;
	///////////////////////////////////////////////////////////////////
	SwapChain(VulkanDevice *_vulkanDevice, VkSurfaceKHR* _surface);
	~SwapChain();
	//
	void SetupSwapChain(uint32_t *width, uint32_t *height, bool vsync = false);
	//
	void AcquireNextImage(VkSemaphore presentCompleteSemaphore, uint32_t *imageIndex);
	//
	void QueuePresent(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore = VK_NULL_HANDLE);

private:

};


