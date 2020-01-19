#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>
#include <vulkan/vulkan.h>
#include "VulkanBuffer.h"
#include "VulkanDevice.h"
#include "imgui/imgui.h"
#include "VulkanHelper.h"

class GUI
{
public:
	
	VulkanDevice *vulkanDevice;
	VulkanHelper *vulkanHelper;
	VkQueue queue;

	VkSampleCountFlagBits rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	uint32_t subpass = 0;

	Buffer vertexBuffer;
	Buffer indexBuffer;
	int32_t vertexCount = 0;
	int32_t indexCount = 0;

	std::vector<VkPipelineShaderStageCreateInfo> shaders;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkPipeline pipeline;

	VkDeviceMemory fontMemory = VK_NULL_HANDLE;
	VkImage fontImage = VK_NULL_HANDLE;
	VkImageView fontView = VK_NULL_HANDLE;
	VkSampler sampler;

	struct PushConstBlock {
		glm::vec2 scale;
		glm::vec2 translate;
	} pushConstBlock;

	bool visible = true;
	bool updated = false;
	float scale = 1.0f;

	GUI(VulkanDevice* _vulkanDevice, VulkanHelper* _vulkanHelper);
	~GUI();

	void PreparePipeline(const VkPipelineCache pipelineCache, const VkRenderPass renderPass);
	void PrepareResources();

	bool UpdateGuiDetails();
	void Draw(const VkCommandBuffer commandBuffer);
	void Resize(uint32_t width, uint32_t height);


	bool Header(const char* caption);
	bool CheckBox(const char* caption, bool* value);
	bool CheckBox(const char* caption, int32_t* value);
	bool InputFloat(const char* caption, float* value, float step, uint32_t precision);
	bool SliderFloat(const char* caption, float* value, float min, float max);
	bool SliderInt(const char* caption, int32_t* value, int32_t min, int32_t max);
	bool ComboBox(const char* caption, int32_t* itemindex, std::vector<std::string> items);
	bool Button(const char* caption);
	void Text(const char* formatstr, ...);

	bool InputVec3(const char* caption, float* value, float step, uint32_t precision);

};