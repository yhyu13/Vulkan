#include "VulkanTexture.h"
#define STB_IMAGE_IMPLEMENTATION
#include "STB/stb_image.h"
void Texture::UpdateDescriptor()
{
	descriptor.sampler = sampler;
	descriptor.imageView = view;
	descriptor.imageLayout = imageLayout;
}

void Texture::Destroy()
{
	vkDestroyImageView(vulkanDevice->logicalDevice, view, nullptr);
	vkDestroyImage(vulkanDevice->logicalDevice, image, nullptr);
	if (sampler)
	{
		vkDestroySampler(vulkanDevice->logicalDevice, sampler, nullptr);
	}
	vkFreeMemory(vulkanDevice->logicalDevice, deviceMemory, nullptr);
}
void Texture::GenerateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VulkanDevice * _vulkanDevice, VulkanHelper * _vulkanHelper , VkQueue copyQueue) {
	// Check if image format supports linear blitting
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(_vulkanDevice->physicalDevice, imageFormat, &formatProperties);

	if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
		throw std::runtime_error("texture image format does not support linear blitting!");
	}

	VkCommandBuffer commandBuffer = _vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	VkImageMemoryBarrier barrier = {};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.image = image;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.subresourceRange.levelCount = 1;

	int32_t mipWidth = texWidth;
	int32_t mipHeight = texHeight;

	for (uint32_t i = 1; i < mipLevels; i++) {
		barrier.subresourceRange.baseMipLevel = i - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		VkImageBlit blit = {};
		blit.srcOffsets[0] = { 0, 0, 0 };
		blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
		blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = i - 1;
		blit.srcSubresource.baseArrayLayer = 0;
		blit.srcSubresource.layerCount = 1;
		blit.dstOffsets[0] = { 0, 0, 0 };
		blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
		blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.dstSubresource.mipLevel = i;
		blit.dstSubresource.baseArrayLayer = 0;
		blit.dstSubresource.layerCount = 1;

		vkCmdBlitImage(commandBuffer,
			image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1, &blit,
			VK_FILTER_LINEAR);

		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			0, nullptr,
			0, nullptr,
			1, &barrier);

		if (mipWidth > 1) mipWidth /= 2;
		if (mipHeight > 1) mipHeight /= 2;
	}

	barrier.subresourceRange.baseMipLevel = mipLevels - 1;
	barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		0, nullptr,
		0, nullptr,
		1, &barrier);

	_vulkanDevice->FlushCommandBuffer(commandBuffer, copyQueue);
}
void Texture2D::LoadFromFile(std::string filename, VkFormat format, VulkanDevice * _vulkanDevice, VulkanHelper * _vulkanHelper, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout, bool forceLinear)
{
	std::ifstream f(filename.c_str());
	bool hdrEnabled = false;
	if (!f.fail())
	{
		throw std::runtime_error("Error: Texture file not present");
	}
	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");

	std::string fileName = std::string(buffer).substr(0, pos);
	std::size_t found = fileName.find("PC Visual Studio");
	if (found != std::string::npos)
	{
		fileName.erase(fileName.begin() + found, fileName.end());
	}
	fileName = fileName + "Resources\\Textures\\" + filename;
	//gli::texture2d tex2D(gli::load(fileName.c_str()));
	int texWidth, texHeight, texChannels, byteCount =0;
	stbi_uc* pixels; 

	float* pixelsHDR;
	found = fileName.find(".hdr");
	if (found == std::string::npos)
	{
		pixels = stbi_load(fileName.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;
		byteCount = 4;
	}
	else
	{
		pixels = stbi_load(fileName.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		//stbi_ldr_to_hdr_scale(1.0f);
		//stbi_ldr_to_hdr_gamma(2.2f);
		hdrEnabled = true;
		byteCount = 4;
		mipLevels = 1;
	}
	
	//assert(!tex2D.empty());

	this->vulkanDevice = _vulkanDevice;
	this->vulkanHelper = _vulkanHelper;
	//width = static_cast<uint32_t>(tex2D[0].extent().x);
	//height = static_cast<uint32_t>(tex2D[0].extent().y);
	//mipLevels = static_cast<uint32_t>(tex2D.levels());

	width = static_cast<uint32_t>(texWidth);
	height = static_cast<uint32_t>(texHeight);
	
	
	// Get device properites for the requested texture format
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, format, &formatProperties);

	// Only use linear tiling if requested (and supported by the device)
	// Support for linear tiling is mostly limited, so prefer to use
	// optimal tiling instead
	// On most implementations linear tiling will only support a very
	// limited amount of formats and features (mip maps, cubemaps, arrays, etc.)
	VkBool32 useStaging = !forceLinear;

	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	VkMemoryRequirements memReqs;

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	if (useStaging)
	{
		// Create a host-visible staging buffer that contains the raw image data
		VkBuffer stagingBuffer;
		VkDeviceMemory stagingMemory;


		VkBufferCreateInfo bufferCreateInfo{};
		bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferCreateInfo.size = width * height * byteCount;
		totalSize = (int)width*height * byteCount;
		// This buffer is used as a transfer source for the buffer copy
		bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
		bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkResult res = vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Buffer");
		}

		// Get memory requirements for the staging buffer (alignment, memory type bits)
		vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, stagingBuffer, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;
		// Get memory type index for a host visible buffer
		memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

		res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Allocate Memory");
		}
		res = vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Bind Buffer Memory");
		}
		// Copy texture data into staging buffer
		uint8_t *data;
		res = vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Map Memory");
		}
		memcpy(data, pixels, width*height * byteCount);
		vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

		

		

		// Create optimal tiled target image
		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = format;
		imageCreateInfo.mipLevels = mipLevels;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageCreateInfo.extent = { width, height, 1 };
		imageCreateInfo.usage = imageUsageFlags;
		// Ensure that the TRANSFER_DST bit is set for staging
		if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
		{
			imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		}
		res = vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &image);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image");
		}
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, image, &memReqs);

		memAllocInfo.allocationSize = memReqs.size;

		memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &deviceMemory);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Allocate Memory");
		}
		res = vkBindImageMemory(vulkanDevice->logicalDevice, image, deviceMemory, 0);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Bind Image Memory");
		}

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevels;
		subresourceRange.layerCount = 1;

		// Image barrier for optimal image (target)
		// Optimal image will be used as destination for the copy
		vulkanHelper->SetImageLayout(copyCmd,
			image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);
		// Setup buffer copy regions for each mip level
		std::vector<VkBufferImageCopy> bufferCopyRegions;
		//uint32_t offset = 0;
		for (uint32_t i = 0; i < 1; i++)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = i;
			bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(width);
			bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(height);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = 0;

			bufferCopyRegions.push_back(bufferCopyRegion);

			//offset += static_cast<uint32_t>(tex2D[i].size());
		}
		// Copy mip levels from staging buffer
		vkCmdCopyBufferToImage(
			copyCmd,
			stagingBuffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			static_cast<uint32_t>(bufferCopyRegions.size()),
			bufferCopyRegions.data()
		);
		
		// Change texture image layout to shader read after all mip levels have been copied
		this->imageLayout = imageLayout;
		/*vulkanHelper->SetImageLayout(
			copyCmd,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			imageLayout,
			subresourceRange);*/

		vulkanDevice->FlushCommandBuffer(copyCmd, copyQueue);

		// Clean up staging resources
		vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);
		vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);

		//VkCommandBuffer Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		/*vulkanHelper->SetImageLayout(Cmd,
			image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL ,
			subresourceRange);*/
		 found = fileName.find(".hdr");
		if (found == std::string::npos)
		{
			GenerateMipmaps(image, format, width, height, mipLevels, vulkanDevice, vulkanHelper, copyQueue);
		}
		else
		{
			VkCommandBuffer Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	
			vulkanHelper->SetImageLayout(
			Cmd,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

			vulkanDevice->FlushCommandBuffer(Cmd, copyQueue);
		}
		
		/*vulkanHelper->SetImageLayout(
			Cmd,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			imageLayout,
			subresourceRange);

		vulkanDevice->FlushCommandBuffer(Cmd, copyQueue);*/
	}


	// Create a defaultsampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	// Max level-of-detail should match mip level count
	samplerCreateInfo.maxLod = (useStaging) ? (float)mipLevels : 0.0f;
	// Only enable anisotropic filtering if enabled on the devicec
	if (vulkanDevice->physicalDeviceFeatures.samplerAnisotropy) {
		// Use max. level of anisotropy for this example
		samplerCreateInfo.maxAnisotropy = vulkanDevice->physicalDeviceProperties.limits.maxSamplerAnisotropy;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
	}
	else {
		// The device does not support anisotropic filtering
		samplerCreateInfo.maxAnisotropy = 1.0;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
	}

	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VkResult res = vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Sampler");
	}

	// Create image view
	// Textures are not directly accessed by the shaders and
	// are abstracted by image views containing additional
	// information and sub resource ranges
	VkImageViewCreateInfo viewCreateInfo = {};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	// Linear tiling usually won't support mip maps
	// Only set mip map count if optimal tiling is used
	viewCreateInfo.subresourceRange.levelCount = (useStaging) ? mipLevels : 1;
	viewCreateInfo.image = image;
	res = vkCreateImageView(vulkanDevice->logicalDevice, &viewCreateInfo, nullptr, &view);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Image View");
	}
	// Update descriptor image info member that can be used for setting up descriptor sets
	UpdateDescriptor();
}
glm::uvec3 alignedDivision(const VkExtent3D& extent, const VkExtent3D& granularity)
{
	glm::uvec3 res;
	res.x = extent.width / granularity.width + ((extent.width  % granularity.width) ? 1u : 0u);
	res.y = extent.height / granularity.height + ((extent.height % granularity.height) ? 1u : 0u);
	res.z = extent.depth / granularity.depth + ((extent.depth  % granularity.depth) ? 1u : 0u);
	return res;
}
void TextureCubeMap::loadFromFile(std::string filename, VkFormat format, VulkanDevice * _vulkanDevice, VulkanHelper * _vulkanHelper, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{

	std::ifstream f(filename.c_str());
	
	if (!f.fail())
	{
		throw std::runtime_error("Error: Texture file not present");
	}
	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");

	std::string fileName = std::string(buffer).substr(0, pos);
	std::size_t found = fileName.find("PC Visual Studio");
	if (found != std::string::npos)
	{
		fileName.erase(fileName.begin() + found, fileName.end());
	}
	fileName = fileName + "Resources\\Textures\\" + filename;
	gli::texture_cube texCube(gli::load(fileName.c_str()));

	assert(!texCube.empty());

	this->vulkanDevice = _vulkanDevice;
	this->vulkanHelper = _vulkanHelper;
	width = static_cast<uint32_t>(texCube.extent().x);
	height = static_cast<uint32_t>(texCube.extent().y);
	mipLevels = static_cast<uint32_t>(texCube.levels());

	VkMemoryAllocateInfo memAllocInfo{};
	memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;

	VkMemoryRequirements memReqs;

	// Create a host-visible staging buffer that contains the raw image data
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo{};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;

	bufferCreateInfo.size = texCube.size();
	// This buffer is used as a transfer source for the buffer copy
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult res = vkCreateBuffer(vulkanDevice->logicalDevice, &bufferCreateInfo, nullptr, &stagingBuffer);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Buffer");
	}
	// Get memory requirements for the staging buffer (alignment, memory type bits)
	vkGetBufferMemoryRequirements(vulkanDevice->logicalDevice, stagingBuffer, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	// Get memory type index for a host visible buffer
	memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &stagingMemory);
	vkBindBufferMemory(vulkanDevice->logicalDevice, stagingBuffer, stagingMemory, 0);

	// Copy texture data into staging buffer
	uint8_t *data;
	vkMapMemory(vulkanDevice->logicalDevice, stagingMemory, 0, memReqs.size, 0, (void **)&data);
	memcpy(data, texCube.data(), texCube.size());
	vkUnmapMemory(vulkanDevice->logicalDevice, stagingMemory);

	// Setup buffer copy regions for each face including all of it's miplevels
	std::vector<VkBufferImageCopy> bufferCopyRegions;
	size_t offset = 0;

	for (uint32_t face = 0; face < 6; face++)
	{
		for (uint32_t level = 0; level < mipLevels; level++)
		{
			VkBufferImageCopy bufferCopyRegion = {};
			bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			bufferCopyRegion.imageSubresource.mipLevel = level;
			bufferCopyRegion.imageSubresource.baseArrayLayer = face;
			bufferCopyRegion.imageSubresource.layerCount = 1;
			bufferCopyRegion.imageExtent.width = static_cast<uint32_t>(texCube[face][level].extent().x);
			bufferCopyRegion.imageExtent.height = static_cast<uint32_t>(texCube[face][level].extent().y);
			bufferCopyRegion.imageExtent.depth = 1;
			bufferCopyRegion.bufferOffset = offset;

			bufferCopyRegions.push_back(bufferCopyRegion);

			// Increase offset into staging buffer for next level / face
			offset += texCube[face][level].size();
		}
	}

	// Create optimal tiled target image
	VkImageCreateInfo imageCreateInfo{};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = mipLevels;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageCreateInfo.extent = { width, height, 1 };
	imageCreateInfo.usage = imageUsageFlags;
	// Ensure that the TRANSFER_DST bit is set for staging
	if (!(imageCreateInfo.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT))
	{
		imageCreateInfo.usage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	}
	// Cube faces count as array layers in Vulkan
	imageCreateInfo.arrayLayers = 6;
	// This flag is required for cube map images
	imageCreateInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;


	vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &image);

	vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, image, &memReqs);

	memAllocInfo.allocationSize = memReqs.size;
	memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &deviceMemory);
	vkBindImageMemory(vulkanDevice->logicalDevice, image, deviceMemory, 0);

	// Use a separate command buffer for texture loading
	VkCommandBuffer copyCmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// Image barrier for optimal image (target)
	// Set initial layout for all array layers (faces) of the optimal (target) tiled texture
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.layerCount = 6;

	vulkanHelper->SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// Copy the cube map faces from the staging buffer to the optimal tiled image
	vkCmdCopyBufferToImage(
		copyCmd,
		stagingBuffer,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		static_cast<uint32_t>(bufferCopyRegions.size()),
		bufferCopyRegions.data());

	// Change texture image layout to shader read after all faces have been copied
	this->imageLayout = imageLayout;
	vulkanHelper->SetImageLayout(
		copyCmd,
		image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		imageLayout,
		subresourceRange);

	vulkanDevice->FlushCommandBuffer(copyCmd, copyQueue);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = samplerCreateInfo.addressModeU;
	samplerCreateInfo.addressModeW = samplerCreateInfo.addressModeU;
	samplerCreateInfo.mipLodBias = 0.0f;
	// Only enable anisotropic filtering if enabled on the devicec
	if (vulkanDevice->physicalDeviceFeatures.samplerAnisotropy) {
		// Use max. level of anisotropy for this example
		samplerCreateInfo.maxAnisotropy = vulkanDevice->physicalDeviceProperties.limits.maxSamplerAnisotropy;
		samplerCreateInfo.anisotropyEnable = VK_TRUE;
	}
	else {
		// The device does not support anisotropic filtering
		samplerCreateInfo.maxAnisotropy = 1.0;
		samplerCreateInfo.anisotropyEnable = VK_FALSE;
	}

	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = (float)mipLevels;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	vkCreateSampler(vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler);

	// Create image view
	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
	viewCreateInfo.format = format;
	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	viewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	viewCreateInfo.subresourceRange.layerCount = 6;
	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	viewCreateInfo.image = image;
	vkCreateImageView(vulkanDevice->logicalDevice, &viewCreateInfo, nullptr, &view);

	// Clean up staging resources
	vkFreeMemory(vulkanDevice->logicalDevice, stagingMemory, nullptr);
	vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer, nullptr);

	// Update descriptor image info member that can be used for setting up descriptor sets
	UpdateDescriptor();
}

void SparseTexture::LoadFromFile(std::string filename, VkFormat format, int width, int height, int layerCount, int memoryBlocks, VulkanDevice * _vulkanDevice, VulkanHelper * _vulkanHelper, VkQueue copyQueue, VkImageUsageFlags imageUsageFlags, VkImageLayout imageLayout)
{
	//
	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");

	std::string path = std::string(buffer).substr(0, pos);
	std::size_t found = path.find("PC Visual Studio");
	if (found != std::string::npos)
	{
		path.erase(path.begin() + found, path.end());
	}
	path = path + "Resources\\Textures\\MegaTexture\\" + filename;
	textureFile = fopen(path.c_str(),"rb");
	if (textureFile == NULL)
	{ 
		throw std::runtime_error("Error: fopen");
	}
	//
	mipLevels = floor(log2(std::max(width, height))) + 1;
	
	// Get device properites for the requested texture format
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(_vulkanDevice->physicalDevice, format, &formatProperties);

	// Get sparse image properties
	std::vector<VkSparseImageFormatProperties> sparseProperties;
	// Sparse properties count for the desired format
	uint32_t sparsePropertiesCount;
	vkGetPhysicalDeviceSparseImageFormatProperties(
		_vulkanDevice->physicalDevice,
		format,
		VK_IMAGE_TYPE_2D,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		&sparsePropertiesCount,
		nullptr);
	// Check if sparse is supported for this format
	if (sparsePropertiesCount == 0)
	{
		throw std::runtime_error("Error: Requested format does not support sparse features!");
		return;
	}

	// Get actual image format properties
	sparseProperties.resize(sparsePropertiesCount);
	vkGetPhysicalDeviceSparseImageFormatProperties(
		_vulkanDevice->physicalDevice,
		format,
		VK_IMAGE_TYPE_2D,
		VK_SAMPLE_COUNT_1_BIT,
		VK_IMAGE_USAGE_SAMPLED_BIT,
		VK_IMAGE_TILING_OPTIMAL,
		&sparsePropertiesCount,
		sparseProperties.data());



	// Create sparse image
	VkImageCreateInfo sparseImageCreateInfo{};
	sparseImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	sparseImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	sparseImageCreateInfo.format = format;
	sparseImageCreateInfo.mipLevels = mipLevels;
	sparseImageCreateInfo.arrayLayers = layerCount;
	sparseImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	sparseImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	sparseImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	sparseImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	sparseImageCreateInfo.extent = { uint32_t(width), uint32_t(height),  uint32_t(1) };
	sparseImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	sparseImageCreateInfo.flags = VK_IMAGE_CREATE_SPARSE_BINDING_BIT | VK_IMAGE_CREATE_SPARSE_RESIDENCY_BIT;
	VkResult res = vkCreateImage(_vulkanDevice->logicalDevice, &sparseImageCreateInfo, nullptr, &image);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Image");
	}

	// Get memory requirements
	VkMemoryRequirements sparseImageMemoryReqs;
	// Sparse image memory requirement counts
	vkGetImageMemoryRequirements(_vulkanDevice->logicalDevice, image, &sparseImageMemoryReqs);



	// Check requested image size against hardware sparse limit
	if (sparseImageMemoryReqs.size > _vulkanDevice->physicalDeviceProperties.limits.sparseAddressSpaceSize)
	{
		throw std::runtime_error("Error: Requested sparse image size exceeds supportes sparse address space size!");
	};

	// Get sparse memory requirements
	// Count
	uint32_t sparseMemoryReqsCount = 32;
	std::vector<VkSparseImageMemoryRequirements> sparseMemoryReqs(sparseMemoryReqsCount);
	vkGetImageSparseMemoryRequirements(_vulkanDevice->logicalDevice, image, &sparseMemoryReqsCount, sparseMemoryReqs.data());
	if (sparseMemoryReqsCount == 0)
	{
		
		throw std::runtime_error("Error: No memory requirements for the sparse image!");

	}
	sparseMemoryReqs.resize(sparseMemoryReqsCount);
	// Get actual requirements
	vkGetImageSparseMemoryRequirements(_vulkanDevice->logicalDevice, image, &sparseMemoryReqsCount, sparseMemoryReqs.data());

	// Get sparse image requirements for the color aspect
	VkSparseImageMemoryRequirements sparseMemoryReq;
	bool colorAspectFound = false;
	for (auto reqs : sparseMemoryReqs)
	{
		if (reqs.formatProperties.aspectMask & VK_IMAGE_ASPECT_COLOR_BIT)
		{
			sparseMemoryReq = reqs;
			colorAspectFound = true;
			break;
		}
	}
	if (!colorAspectFound)
	{
		throw std::runtime_error("Error: Could not find sparse image memory requirements for color aspect bit!");
	}
	uint32_t memoryTypeIndex;
	// todo:
	// Calculate number of required sparse memory bindings by alignment
	assert((sparseImageMemoryReqs.size % sparseImageMemoryReqs.alignment) == 0);
	memoryTypeIndex = _vulkanDevice->GetMemoryType(sparseImageMemoryReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

	// Get sparse bindings
	uint32_t sparseBindsCount = static_cast<uint32_t>(sparseImageMemoryReqs.size / sparseImageMemoryReqs.alignment);
	std::vector<VkSparseMemoryBind>	sparseMemoryBinds(sparseBindsCount);

	// Check if the format has a single mip tail for all layers or one mip tail for each layer
	// The mip tail contains all mip levels > sparseMemoryReq.imageMipTailFirstLod
	bool singleMipTail = sparseMemoryReq.formatProperties.flags & VK_SPARSE_IMAGE_FORMAT_SINGLE_MIPTAIL_BIT;


	
		// sparseMemoryReq.imageMipTailFirstLod is the first mip level that's stored inside the mip tail
		for (uint32_t mipLevel = 0; mipLevel < sparseMemoryReq.imageMipTailFirstLod; mipLevel++)
		{
			VkExtent3D extent;
			extent.width = std::max(sparseImageCreateInfo.extent.width >> mipLevel, 1u);
			extent.height = std::max(sparseImageCreateInfo.extent.height >> mipLevel, 1u);
			extent.depth = std::max(sparseImageCreateInfo.extent.depth >> mipLevel, 1u);

			VkImageSubresource subResource{};
			subResource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subResource.mipLevel = mipLevel;
			subResource.arrayLayer = 0;

			// Aligned sizes by image granularity
			VkExtent3D imageGranularity = sparseMemoryReq.formatProperties.imageGranularity;
			glm::uvec3 sparseBindCounts = alignedDivision(extent, imageGranularity);
			glm::uvec3 lastBlockExtent;
			lastBlockExtent.x = (extent.width % imageGranularity.width) ? extent.width % imageGranularity.width : imageGranularity.width;
			lastBlockExtent.y = (extent.height % imageGranularity.height) ? extent.height % imageGranularity.height : imageGranularity.height;
			lastBlockExtent.z = (extent.depth % imageGranularity.depth) ? extent.depth % imageGranularity.depth : imageGranularity.depth;

			// Alllocate memory for some blocks
		
			for (uint32_t z = 0; z < sparseBindCounts.z; z++)
			{
				for (uint32_t y = 0; y < sparseBindCounts.y; y++)
				{
					for (uint32_t x = 0; x < sparseBindCounts.x; x++)
					{
						// Offset 
						VkOffset3D offset;
						offset.x = x * imageGranularity.width;
						offset.y = y * imageGranularity.height;
						offset.z = z * imageGranularity.depth;
						// Size of the page
						VkExtent3D extent;
						extent.width = (x == sparseBindCounts.x - 1) ? lastBlockExtent.x : imageGranularity.width;
						extent.height = (y == sparseBindCounts.y - 1) ? lastBlockExtent.y : imageGranularity.height;
						extent.depth = (z == sparseBindCounts.z - 1) ? lastBlockExtent.z : imageGranularity.depth;

						VkSparseImageMemoryBind imageBind = {};
						imageBind.extent = extent;
						imageBind.offset = offset;
						imageBind.subresource = subResource;
						imageBind.memory = VK_NULL_HANDLE;
						imageBind.memoryOffset = 0;
						imageBinds[mipLevel].emplace_back(imageBind);
					}
				}
			}
		}
		

		//
		int tempSize = 0;
		for (int i = 0; i < imageBinds.size(); ++i)
		{
			readByteOffset.emplace_back(tempSize);
			mipAndPages[i] = imageBinds[i].size();
			tempSize += imageBinds[i].size();
		}


	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo{};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.maxAnisotropy = 1.0f;
	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = static_cast<float>(mipLevels);
	samplerCreateInfo.maxAnisotropy = _vulkanDevice->physicalDeviceFeatures.samplerAnisotropy ? _vulkanDevice->physicalDeviceProperties.limits.maxSamplerAnisotropy : 1.0f;
	samplerCreateInfo.anisotropyEnable = false;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	res = vkCreateSampler(_vulkanDevice->logicalDevice, &samplerCreateInfo, nullptr, &sampler);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Image");
	}
	// Create image view
	VkImageViewCreateInfo viewCreateInfo{};
	viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewCreateInfo.image = VK_NULL_HANDLE;
	viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewCreateInfo.format = format;
	viewCreateInfo.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewCreateInfo.subresourceRange.baseMipLevel = 0;
	viewCreateInfo.subresourceRange.baseArrayLayer = 0;
	viewCreateInfo.subresourceRange.layerCount = 1;
	viewCreateInfo.subresourceRange.levelCount = mipLevels;
	viewCreateInfo.image = image;
	res = vkCreateImageView(_vulkanDevice->logicalDevice, &viewCreateInfo, nullptr, &view);
	if (res != VK_SUCCESS) {
		throw std::runtime_error("Error: Create Image");
	}
	// Fill image descriptor image info that can be used during the descriptor set setup
	this->imageLayout = imageLayout;
	UpdateDescriptor();

	VkCommandBuffer Cmd = _vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = mipLevels;
	subresourceRange.layerCount = 1;

	_vulkanHelper->SetImageLayout(
		Cmd,
		image,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange);

	_vulkanDevice->FlushCommandBuffer(Cmd, copyQueue);

	// Assign Memory

	for (int i = 0; i < memoryBlocks; ++i)
	{
		
		memoryCount.emplace_back(i);
		
		
		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = sparseMemoryReq.imageMipTailSize;
		allocInfo.memoryTypeIndex = memoryTypeIndex;

		VkDeviceMemory deviceMemory;
		VkResult res = vkAllocateMemory(_vulkanDevice->logicalDevice, &allocInfo, nullptr, &deviceMemory);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Allocate Memory");
		}
		memoryDeviceList.emplace_back(deviceMemory);
	}
	// Create Staging Buffer
	_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
		&stagingBuffer, (memoryBlocks)*sparseMemoryReq.imageMipTailSize);
	stagingBuffer.Map();

	//todo
	//Buffer mipTailBuffer;
	//_vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	//	&mipTailBuffer, sparseMemoryReq.imageMipTailSize);
	//mipTailBuffer.Map();

	////Allocate miptail Memory
	//VkMemoryAllocateInfo allocInfo{};
	//allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	//allocInfo.allocationSize = sparseMemoryReq.imageMipTailSize;
	//allocInfo.memoryTypeIndex = memoryTypeIndex;

	//VkDeviceMemory deviceMemoryMipTail;
	//res = vkAllocateMemory(_vulkanDevice->logicalDevice, &allocInfo, nullptr, &deviceMemoryMipTail);
	//if (res != VK_SUCCESS) {
	//	throw std::runtime_error("Error: Allocate Memory");
	//}

	//mipTailBind.memory = deviceMemoryMipTail;
	//mipTailBind.memoryOffset = 0;
	//mipTailBind.flags = 0;//  VK_SPARSE_MEMORY_BIND_METADATA_BIT;
	////miptail bind
	//mipTailBind.resourceOffset = sparseMemoryReq.imageMipTailOffset + sparseMemoryReq.imageMipTailStride;
	//mipTailBind.size = sparseMemoryReq.imageMipTailSize;
	////mipTailBind.resourceOffset = 0;
	////
	//imageBinds[7][0].memory = deviceMemoryMipTail;
	//std::vector<VkSparseImageMemoryBind> currentImageBind;
	//currentImageBind.push_back(imageBinds[7][0]);
	////

	//fseek(textureFile, -65536, SEEK_END);
	////std::vector<unsigned char> tempMipTailData;
	////tempMipTailData.resize(21844);
	//int result = fread(mipTailBuffer.mapped, sizeof(char), 65536, textureFile);
	////memcpy(mipTailBuffer.mapped, tempMipTailData.data(), 21844);
	//fseek(textureFile, 0, SEEK_SET);
	//if (!result)
	//{
	//	throw std::runtime_error("Error: fread");
	//}

	//std::vector < VkBufferImageCopy> imageCopy;
	//int offsetSize = 0;
	////for (int i = 8; i < 9; ++i)
	//{
	//	int pageWidth = pow(2, 14 - 7);
	//	//
	//	VkImageSubresourceLayers  subresourceRange1 = {};
	//	subresourceRange1.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//	subresourceRange1.mipLevel = 7;
	//	subresourceRange1.layerCount = 1;
	//	subresourceRange1.baseArrayLayer = 0;
	//	//
	//	VkBufferImageCopy imageCopyInst;
	//	imageCopyInst.bufferOffset = offsetSize;
	//	imageCopyInst.bufferRowLength = 0;
	//	imageCopyInst.bufferImageHeight = 0;
	//	imageCopyInst.imageOffset = VkOffset3D{0,0,0};
	//	imageCopyInst.imageExtent = VkExtent3D{ uint32_t(pageWidth) ,uint32_t(pageWidth) ,uint32_t(1)};
	//	imageCopyInst.imageSubresource = subresourceRange1;
	//	
	//	imageCopy.push_back(imageCopyInst);

	//	offsetSize += pageWidth * pageWidth *4;
	//}

	////
	//VkCommandBuffer  CmdTemp = _vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	// subresourceRange = {};
	//subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	//subresourceRange.baseMipLevel = 0;
	//subresourceRange.levelCount = mipLevels;
	//subresourceRange.layerCount = 1;

	//_vulkanHelper->SetImageLayout(
	//	CmdTemp,
	//	image,
	//	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	subresourceRange);

	//_vulkanDevice->FlushCommandBuffer(CmdTemp, copyQueue);
	//CmdTemp = _vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	//vkCmdCopyBufferToImage(
	//	CmdTemp,
	//	mipTailBuffer.buffer,
	//	image,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	imageCopy.size(),
	//	imageCopy.data());

	//_vulkanDevice->FlushCommandBuffer(CmdTemp, copyQueue);

	//CmdTemp = _vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

	//_vulkanHelper->SetImageLayout(
	//	CmdTemp,
	//	image,
	//	VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
	//	VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
	//	subresourceRange);
	//_vulkanDevice->FlushCommandBuffer(CmdTemp, copyQueue);
	//mipTailBuffer.Destroy();
	////

	//VkSparseImageMemoryBindInfo imageBindInfo;					// Sparse image memory bind info 
	//VkSparseImageOpaqueMemoryBindInfo mipTailBindInfo;

	//VkBindSparseInfo bindSparseInfo{};
	//bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
	//// Image memory binds
	//imageBindInfo.image = image;
	//imageBindInfo.bindCount = 1;
	//imageBindInfo.pBinds = currentImageBind.data();
	//bindSparseInfo.imageBindCount = (imageBindInfo.bindCount > 0) ? 1 : 0;
	//bindSparseInfo.pImageBinds = &imageBindInfo;

	//// Opaque image memory binds (mip tail)
	//mipTailBindInfo.image = image;
	//mipTailBindInfo.bindCount = 0;// static_cast<uint32_t>(opaqueMemoryBinds.size());
	////mipTailBindInfo.pBinds = &mipTailBind;
	//bindSparseInfo.imageOpaqueBindCount = (mipTailBindInfo.bindCount > 0) ? 1 : 0;
	//bindSparseInfo.pImageOpaqueBinds = &mipTailBindInfo;



	//vkQueueBindSparse(copyQueue, 1, &bindSparseInfo, VK_NULL_HANDLE);
	////todo: use sparse bind semaphore
	//vkQueueWaitIdle(copyQueue);

}
