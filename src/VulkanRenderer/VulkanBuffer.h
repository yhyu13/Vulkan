#pragma once
#include <vector>
#include <assert.h>
#include "vulkan/vulkan.h"
#include <glm/glm.hpp>

class Buffer
{
public:
	VkDevice device;
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkDescriptorBufferInfo descriptor;
	VkDeviceSize size = 0;
	VkDeviceSize alignment = 0;
	void* mapped = nullptr;

	//
	VkBufferUsageFlags usageFlags;
	//
	VkMemoryPropertyFlags memoryPropertyFlags;

	//
	void Map(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	

	//
	void UnMap();
	

	//
	void Bind(VkDeviceSize offset = 0);
	

	//
	void SetupDescriptor(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	

	//
	void CopyToMem(void* data, VkDeviceSize size);
	

	//
	void Flush(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);
	

	//
	void  Invalidate(VkDeviceSize size = VK_WHOLE_SIZE, VkDeviceSize offset = 0);


	//
	void Destroy();
	

};
