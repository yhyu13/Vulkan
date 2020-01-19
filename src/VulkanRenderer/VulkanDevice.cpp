#include "VulkanDevice.h"








////////////////////////
VulkanDevice::VulkanDevice(VkInstance* _instance)
{
	instance = _instance;
}

VulkanDevice::~VulkanDevice()
{
	if (defaultCommandPool)
	{
		vkDestroyCommandPool(logicalDevice, defaultCommandPool, nullptr);
	}
	if (logicalDevice)
	{
		vkDestroyDevice(logicalDevice, nullptr);
	}
}

	
void VulkanDevice::Initialize()
{
	
	// Queue family properties, used for setting up requested queues upon device creation
	uint32_t queueFamilyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
	assert(queueFamilyCount > 0);
	queueFamilyProperties.resize(queueFamilyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilyProperties.data());

	// Get list of supported extensions
	uint32_t extCount = 0;
	vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, nullptr);
	if (extCount > 0)
	{
		std::vector<VkExtensionProperties> extensions(extCount);
		if (vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extCount, &extensions.front()) == VK_SUCCESS)
		{
			for (auto ext : extensions)
			{
				supportedExtensions.push_back(ext.extensionName);
			}
		}
	}
}

void VulkanDevice::CreateLogicalDevice(VkPhysicalDeviceFeatures enabledFeatures, std::vector<const char*>
	enabledExtensions, void * pNextChain, bool useSwapChain, VkQueueFlags requestedQueueTypes)
{
	std::vector<VkDeviceQueueCreateInfo> queueCreateInfos{};

	// Get queue family indices for the requested queue family types
	// Note that the indices may overlap depending on the implementation

	const float defaultQueuePriority(0.0f);

	// Graphics queue
	if (requestedQueueTypes & VK_QUEUE_GRAPHICS_BIT)
	{
		queueFamilyIndices.graphics = getQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
		VkDeviceQueueCreateInfo queueInfo{};
		queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueInfo.queueFamilyIndex = queueFamilyIndices.graphics;
		queueInfo.queueCount = 1;
		queueInfo.pQueuePriorities = &defaultQueuePriority;
		queueCreateInfos.push_back(queueInfo);
	}
	else
	{
		queueFamilyIndices.graphics = VK_NULL_HANDLE;
	}

	// Dedicated compute queue
	if (requestedQueueTypes & VK_QUEUE_COMPUTE_BIT)
	{
		queueFamilyIndices.compute = getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
		if (queueFamilyIndices.compute != queueFamilyIndices.graphics)
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.compute;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		queueFamilyIndices.compute = queueFamilyIndices.graphics;
	}

	// Dedicated transfer queue
	if (requestedQueueTypes & VK_QUEUE_TRANSFER_BIT)
	{
		queueFamilyIndices.transfer = getQueueFamilyIndex(VK_QUEUE_TRANSFER_BIT);
		if ((queueFamilyIndices.transfer != queueFamilyIndices.graphics) && (queueFamilyIndices.transfer != queueFamilyIndices.compute))
		{
			// If compute family index differs, we need an additional queue create info for the compute queue
			VkDeviceQueueCreateInfo queueInfo{};
			queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueInfo.queueFamilyIndex = queueFamilyIndices.transfer;
			queueInfo.queueCount = 1;
			queueInfo.pQueuePriorities = &defaultQueuePriority;
			queueCreateInfos.push_back(queueInfo);
		}
	}
	else
	{
		// Else we use the same queue
		queueFamilyIndices.transfer = queueFamilyIndices.graphics;
	}
	

	//
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());;
	deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
	deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
	deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensions.size();
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

	// If a pNext(Chain) has been passed, we need to add it to the device creation info
	if (pNextChain) {
		VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{};
		physicalDeviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		physicalDeviceFeatures2.features = enabledFeatures;
		physicalDeviceFeatures2.pNext = pNextChain;
		deviceCreateInfo.pEnabledFeatures = nullptr;
		deviceCreateInfo.pNext = &physicalDeviceFeatures2;
	}

	VkResult res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &logicalDevice);
	if (res != VK_SUCCESS)
	{
		throw std::runtime_error("Could not create Vulkan device");
	}
	// Create a default command pool for transfer command buffers

	defaultCommandPool = CreateCommandPool(queueFamilyIndices.graphics);
	computeCommandPool = CreateCommandPool(queueFamilyIndices.compute);
	transferCommandPool = CreateCommandPool(queueFamilyIndices.transfer);

}

uint32_t VulkanDevice::getQueueFamilyIndex(VkQueueFlagBits queueFlags)
{
	// Dedicated queue for compute
		// Try to find a queue family index that supports compute but not graphics
	if (queueFlags & VK_QUEUE_COMPUTE_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0))
			{
				return i;
				break;
			}
		}
	}

	// Dedicated queue for transfer
	// Try to find a queue family index that supports transfer but not graphics and compute
	if (queueFlags & VK_QUEUE_TRANSFER_BIT)
	{
		for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
		{
			if ((queueFamilyProperties[i].queueFlags & queueFlags) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) && ((queueFamilyProperties[i].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0))
			{
				return i;
				break;
			}
		}
	}

	// For other queue types or if no separate compute queue is present, return the first one to support the requested flags
	for (uint32_t i = 0; i < static_cast<uint32_t>(queueFamilyProperties.size()); i++)
	{
		if (queueFamilyProperties[i].queueFlags & queueFlags)
		{
			return i;
			break;
		}
	}

	throw std::runtime_error("Could not find a matching queue family index");
}

VkCommandPool VulkanDevice::CreateCommandPool(uint32_t queueFamilyIndex, VkCommandPoolCreateFlags createFlags)
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = queueFamilyIndex;
	cmdPoolInfo.flags = createFlags;
	VkCommandPool cmdPool;
	VkResult res;
	res = vkCreateCommandPool(logicalDevice, &cmdPoolInfo, nullptr, &cmdPool);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: failed to create command pool");
	}
	return cmdPool;
}

uint32_t VulkanDevice::GetMemoryType(uint32_t typeBits, VkMemoryPropertyFlags properties, VkBool32 * memTypeFound)
{
	for (uint32_t i = 0; i < physicalDeviceMemoryProperties.memoryTypeCount; i++)
	{
		if ((typeBits & 1) == 1)
		{
			if ((physicalDeviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if (memTypeFound)
				{
					*memTypeFound = true;
				}
				return i;
			}
		}
		typeBits >>= 1;
	}

	if (memTypeFound)
	{
		*memTypeFound = false;
		return 0;
	}
	else
	{
		throw std::runtime_error("Could not find a matching memory type");
	}
}



void VulkanDevice::CreateBuffer(VkBufferUsageFlags usageFlags, VkMemoryPropertyFlags memoryPropertyFlags, Buffer * buffer, VkDeviceSize size, void * data)
{
	buffer->device = logicalDevice;

	// Create the buffer handle
	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.usage = usageFlags;
	bufferCreateInfo.size = size;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	VkResult res = vkCreateBuffer(logicalDevice, &bufferCreateInfo, nullptr, &buffer->buffer);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Buffer");
	}
	

	// Create the memory backing up the buffer handle
	// Create the memory backing up the buffer handle
	VkMemoryRequirements memReqs;
	VkMemoryAllocateInfo memAlloc{};
	memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	vkGetBufferMemoryRequirements(logicalDevice, buffer->buffer, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	// Find a memory type index that fits the properties of the buffer
	memAlloc.memoryTypeIndex = GetMemoryType(memReqs.memoryTypeBits, memoryPropertyFlags);
	res = vkAllocateMemory(logicalDevice, &memAlloc, nullptr, &buffer->memory);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Allocate Memory");
	}

	buffer->alignment = memReqs.alignment;
	buffer->size = memAlloc.allocationSize;
	buffer->usageFlags = usageFlags;
	buffer->memoryPropertyFlags = memoryPropertyFlags;
	
	// If a pointer to the buffer data has been passed, map the buffer and copy over the data
	if (data != nullptr)
	{
		buffer->Map();
		memcpy(buffer->mapped, data, size);
		if ((memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) == 0)
			buffer->Flush();

		buffer->UnMap();
	}

	// Initialize a default descriptor that covers the whole buffer size
	buffer->SetupDescriptor();

	// Attach the memory to the buffer object
	 buffer->Bind();
}

VkCommandBuffer VulkanDevice::CreateCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
	cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufAllocateInfo.commandPool = defaultCommandPool;
	cmdBufAllocateInfo.level = level;
	cmdBufAllocateInfo.commandBufferCount = 1;

	VkCommandBuffer cmdBuffer;
	VkResult res = vkAllocateCommandBuffers(logicalDevice, &cmdBufAllocateInfo, &cmdBuffer);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Allocate Command Buffers");
	}

	// If requested, also start recording for the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VkResult res = vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}
	}

	return cmdBuffer;
}

void VulkanDevice::FlushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
	if (commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}

	VkResult res = vkEndCommandBuffer(commandBuffer);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: End Command Buffer");
	}
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	// Create fence to ensure that the command buffer has finished executing
	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = 0;
	VkFence fence;
	res = vkCreateFence(logicalDevice, &fenceInfo, nullptr, &fence);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Fence");
	}

	// Submit to the queue
	res = vkQueueSubmit(queue, 1, &submitInfo, fence);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Queue Submit");
	}
	// Wait for the fence to signal that command buffer has finished executing
	res = vkWaitForFences(logicalDevice, 1, &fence, VK_TRUE, DEFAULT_FENCE_TIMEOUT);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Wait For Fences");
	}
	vkDestroyFence(logicalDevice, fence, nullptr);

	if (free)
	{
		vkFreeCommandBuffers(logicalDevice, defaultCommandPool, 1, &commandBuffer);
	}
}
