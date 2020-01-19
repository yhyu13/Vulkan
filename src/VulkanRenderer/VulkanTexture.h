#pragma once

#include "vulkan/vulkan.h"
#include "VulkanDevice.h"
#include "VulkanHelper.h"
#include <string>
#include <fstream>
#include <gli/gli.hpp>
#include "VulkanHelper.h"



class Texture {
public:
	VulkanDevice *vulkanDevice;
	VulkanHelper *vulkanHelper;
	VkImage image;
	VkImageLayout imageLayout;
	VkDeviceMemory deviceMemory;
	VkImageView view;
	uint32_t width, height;
	uint32_t mipLevels;
	uint32_t layerCount;
	VkDescriptorImageInfo descriptor;
	//Sampler for the texture
	VkSampler sampler;
	//Total size of image including mipmaps
	int totalSize;

	// Update image descriptor from current sampler, view and image layout 
	void UpdateDescriptor();
	// Release all Vulkan resources held by this texture 
	void Destroy();
	//
	void GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VulkanDevice * _vulkanDevice, VulkanHelper * _vulkanHelper, VkQueue copyQueue);
};

class Texture2D : public Texture {
public:
	
	void LoadFromFile(
		std::string filename,
		VkFormat format,
		VulkanDevice* _vulkanDevice, VulkanHelper* _vulkanHelper,
		VkQueue copyQueue,
		VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		bool forceLinear = false);
	
	
};

/** @brief Cube map texture */
class TextureCubeMap : public Texture {
public:
	
	void loadFromFile(
		std::string filename,
		VkFormat format,
		VulkanDevice* _vulkanDevice, VulkanHelper * _vulkanHelper,
		VkQueue copyQueue,
		VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	
};
/** @brief Sparse texture */
class SparseTexture : public Texture 
{
public:
	FILE* textureFile;
	VkSparseMemoryBind mipTailBind;
	std::deque<int> memoryCount;
	std::deque<VkDeviceMemory> memoryDeviceList;
	std::unordered_map<int, int> mipAndPages;
	std::unordered_map<int,std::vector < VkSparseImageMemoryBind >> imageBinds;
	//
	Buffer stagingBuffer;
	// 
	std::vector<int> readByteOffset;
	//
	void  LoadFromFile(std::string filename,
		VkFormat format, int width, int height, int layerCount, int memoryBlocks,
		VulkanDevice* _vulkanDevice, VulkanHelper * _vulkanHelper,
		VkQueue copyQueue,
		VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT,
		VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
};