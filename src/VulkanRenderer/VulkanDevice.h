#pragma once
#include <exception>
#include <assert.h>
#include <algorithm>
#include <vector>
#include "vulkan/vulkan.h"
#include "VulkanBuffer.h"
#define DEFAULT_FENCE_TIMEOUT 100000000000
//
const std::vector<const char*> deviceExtensions = {
	VK_KHR_SWAPCHAIN_EXTENSION_NAME, VK_KHR_MAINTENANCE1_EXTENSION_NAME
};

class VulkanDevice
{
public:
	VkInstance* instance;
	//
	VkPhysicalDevice physicalDevice;
	//
	VkDevice logicalDevice;
	//
	VkPhysicalDeviceProperties physicalDeviceProperties;
	//
	VkPhysicalDeviceFeatures physicalDeviceFeatures;
	
	//
	VkPhysicalDeviceMemoryProperties physicalDeviceMemoryProperties;
	//
	std::vector<VkQueueFamilyProperties> queueFamilyProperties;
	// List of extensions supported by the device 
	std::vector<std::string> supportedExtensions;
	//
	VkCommandPool defaultCommandPool = VK_NULL_HANDLE;
	VkCommandPool computeCommandPool = VK_NULL_HANDLE;
	VkCommandPool transferCommandPool = VK_NULL_HANDLE;
	//
	VulkanDevice(VkInstance* _instance);
	//
	~VulkanDevice();
	//
	void Initialize();
	void CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, 
		std::vector<const char*> enabledExtensions, void* pNextChain, bool useSwapChain = true
		, VkQueueFlags requestedQueueTypes = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT| VK_QUEUE_TRANSFER_BIT);
	//
	uint32_t getQueueFamilyIndex(VkQueueFlagBits queueFlags);
	//
	VkCommandPool CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
	//
	
	//
	//
	
	struct
	{
		uint32_t graphics;
		uint32_t compute;
		uint32_t transfer;
	} queueFamilyIndices;

	//

	uint32_t GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 *memTypeFound = nullptr);
	//

	void CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags,
		Buffer *buffer, VkDeviceSize size, void *data = nullptr);

	//
	VkCommandBuffer CreateCommandBuffer(VkCommandBufferLevel level, bool begin = false);
	//
	void FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free = true);

};