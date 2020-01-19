#include "VulkanBuffer.h"

void Buffer::Map(VkDeviceSize size, VkDeviceSize offset)
{
	VkResult res = vkMapMemory(device, memory, offset, size, 0, &mapped);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Map Memory");
	}
}

void Buffer::UnMap()
{
	if (mapped)
	{
		vkUnmapMemory(device, memory);
		mapped = nullptr;
	}
}

void Buffer::Bind(VkDeviceSize offset)
{
	
	VkResult res = vkBindBufferMemory(device, buffer, memory, offset);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Bind Buffer Memory");
	}
	
}

void Buffer::SetupDescriptor(VkDeviceSize size, VkDeviceSize offset)
{
	descriptor.offset = offset;
	descriptor.buffer = buffer;
	descriptor.range = size;
}

void Buffer::CopyToMem(void * data, VkDeviceSize size)
{
	assert(mapped);
	memcpy(mapped, data, size);
}

void Buffer::Flush(VkDeviceSize size, VkDeviceSize offset)
{
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	VkResult res = vkFlushMappedMemoryRanges(device, 1, &mappedRange);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Flush Mapped Memory Ranges");
	}
}

void Buffer::Invalidate(VkDeviceSize size, VkDeviceSize offset)
{
	VkMappedMemoryRange mappedRange = {};
	mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
	mappedRange.memory = memory;
	mappedRange.offset = offset;
	mappedRange.size = size;
	VkResult res = vkInvalidateMappedMemoryRanges(device, 1, &mappedRange);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Invalidate Mapped Memory Ranges");
	}
}

void Buffer::Destroy()
{
	if (buffer)
	{
		vkDestroyBuffer(device, buffer, nullptr);
	}
	if (memory)
	{
		vkFreeMemory(device, memory, nullptr);
	}
}
