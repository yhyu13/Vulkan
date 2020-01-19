#include <vulkan/vulkan.h>
#include "VulkanRenderer.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "VulkanModel.h"
#include <cstring>
#include <stdlib.h> 
#include <ctime>

#define MAX_MODEL_COUNT 1
#define MAX_LIGHT_COUNT 1
#define MODEL_TYPE_COUNT 3



struct LightInstanceDetails
{
	alignas(16) glm::vec3 position;
	alignas(16) glm::vec3 color;
	alignas(16) glm::mat4 mvpMatrix;
	alignas(16) glm::mat4 viewMatrix;
};
struct Weights
{
	alignas(16) float weight;
	
} weight;
struct LightData
{
	int lightType;
	float constant;
	float linear;
	float quadratic;
	glm::vec3 direction;
	float shininess;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;
	float cutOff;
	float outerCutOff;
};
struct Transform
{
	glm::vec3 positionPerAxis;
	glm::vec3 scalePerAxis;
	glm::vec3 rotateAngelPerAxis;
};
struct Material
{
	Texture2D albedoTexture;
	Texture2D normalTexture;
	Texture2D specularTexture;
	int textureCount;
};
struct GameObjectDetails
{
	glm::vec4 color;
};
struct GameObject
{
	Model* model;
	Material* material;
	Transform transform;
	GameObjectDetails gameObjectDetails;
};

struct AllModel
{
	Model modelData;
	Material materialData;
};
struct Scene
{
	std::vector<GameObject> gameObjectList;
};

Scene scene;
std::vector<AllModel> allModelList;
std::vector<LightInstanceDetails> allLightsList;

class UnitTest : public VulkanRenderer
{
public:
	//
	Buffer vertBuffer;
	//
	Buffer instanceLightBuffer;
	//
	Buffer lightDetailsBuffer;
	//
	Buffer weightBuffer;
	//
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;

	//
	std::vector<VkDescriptorSet> descriptorSets;
	//
	LightData lightData;
	//
	VkSampler colorSampler;
	//
	VkSampler shadowSampler;
	//
	Texture2D computeTextureIn;
	//
	Texture2D computeTextureOut;
	//
	Texture2D computeTextureFinal;
	//
	VkCommandBuffer deferredCmdBuffer = VK_NULL_HANDLE;
	//
	VkCommandBuffer temporaryCmdBuffer = VK_NULL_HANDLE;
	//
	VkCommandBuffer computeCmdBuffer = VK_NULL_HANDLE;
	// Semaphore used to synchronize between GBuffer and final scene rendering
	VkSemaphore gBufferSemaphore = VK_NULL_HANDLE;
	//
	std::vector<VkDescriptorSet> computeDescriptorSets;
	//
	VkPipeline computePipelineHorizontal;
	//
	VkPipeline computePipelineVertical;
	//
	VkPipelineLayout computePipelineLayout;
	//
	VkCommandPool computeCommandPool;
	//
	std::vector<VkDescriptorSetLayout> computeDescriptorSetLayouts;
	// Depth bias (and slope) are used to avoid shadowing artefacts
	float depthBiasConstant = 0.0f;
	float depthBiasSlope = 0.0f;
	// Struct for sending vertices to vertex shader
	VertexLayout vertexLayout = VertexLayout({
		VERTEX_COMPONENT_POSITION,
		VERTEX_COMPONENT_NORMAL,
		VERTEX_COMPONENT_UV,
		VERTEX_COMPONENT_COLOR,
		VERTEX_COMPONENT_TANGENT
		});
	//Contain details of how the vertex data will be send to the shader
	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	struct {
		glm::mat4 projViewMat;
		glm::mat4 viewMat;
		glm::vec3 viewPos;
	} uboVert;

	// Data send to the shader per model
	struct PerModelData
	{
		glm::mat4 modelMat;
		glm::vec4 color;
		int textureCount;
		int debugValue;
		float bias;
	}perModelData;
	//
	struct AttachmentCreateInfo
	{
		uint32_t width, height;
		uint32_t layerCount;
		VkFormat format;
		VkImageUsageFlags usage;
	};
	// Framebuffer for offscreen rendering
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkFormat format;
	};
	//
	struct FrameBuffer {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment position, normal, albedo, specular;
		FrameBufferAttachment depth;
		VkRenderPass renderPass;
	} offScreenFrameBuf;
	struct FrameBufferShadow {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment depth,shadowDepth;
		VkRenderPass renderPass;
	} shadowFrameBuf;

	struct {
		VkPipeline deferred;
		VkPipeline gBuffer;
		VkPipeline shadow;
	} pipelines;

	struct {
		VkPipelineLayout deferred;
		VkPipelineLayout gBuffer;
	} pipelineLayouts;

	struct {
		VkDeviceMemory deviceMemory;
		VkDescriptorSet descriptorSet;
	}quad;

	UnitTest() : VulkanRenderer()
	{
		appName = "DeferredRendering";
		lightData.lightType = 1;
		lightData.ambient = glm::vec4(0.1f, 0.1f, 0.1f, 1.0f);
		lightData.diffuse = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		lightData.specular = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		lightData.constant = 1.0f;
		lightData.linear = 0.02f;
		lightData.quadratic = 0.07f;
		lightData.shininess = 32.0f;
		lightData.direction = glm::vec3(0.0f, -1.0f, -1.5f);
		lightData.cutOff = 0.96f;
		lightData.outerCutOff = 0.93f;
		perModelData.debugValue = 0;
		perModelData.bias = 0.023f;
	}

	~UnitTest()
	{
		vkDestroyPipeline(vulkanDevice->logicalDevice, pipelines.gBuffer, nullptr);
		vkDestroyPipeline(vulkanDevice->logicalDevice, pipelines.deferred, nullptr);
		vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayouts.gBuffer, nullptr);
		vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayouts.deferred, nullptr);

		vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayouts[0], nullptr);

		std::vector<AllModel>::iterator ptr;
		for (ptr = allModelList.begin(); ptr != allModelList.end(); ++ptr)
		{
			ptr->modelData.Destroy();
			if (ptr->materialData.textureCount == 1)
			{
				ptr->materialData.albedoTexture.Destroy();
			}
			if (ptr->materialData.textureCount == 2)
			{
				ptr->materialData.albedoTexture.Destroy();
				ptr->materialData.normalTexture.Destroy();
			}
			if (ptr->materialData.textureCount == 3)
			{
				ptr->materialData.albedoTexture.Destroy();
				ptr->materialData.normalTexture.Destroy();
				ptr->materialData.specularTexture.Destroy();
			}

		}

		vertBuffer.Destroy();
		instanceLightBuffer.Destroy();
		lightDetailsBuffer.Destroy();
		vkDestroySampler(vulkanDevice->logicalDevice, colorSampler, nullptr);


		// Color attachments
		vkDestroyImageView(vulkanDevice->logicalDevice, offScreenFrameBuf.position.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, offScreenFrameBuf.position.image, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, offScreenFrameBuf.position.mem, nullptr);

		vkDestroyImageView(vulkanDevice->logicalDevice, offScreenFrameBuf.normal.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, offScreenFrameBuf.normal.image, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, offScreenFrameBuf.normal.mem, nullptr);

		vkDestroyImageView(vulkanDevice->logicalDevice, offScreenFrameBuf.albedo.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, offScreenFrameBuf.albedo.image, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, offScreenFrameBuf.albedo.mem, nullptr);

		vkDestroyImageView(vulkanDevice->logicalDevice, offScreenFrameBuf.specular.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, offScreenFrameBuf.specular.image, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, offScreenFrameBuf.specular.mem, nullptr);

		// Depth attachment
		vkDestroyImageView(vulkanDevice->logicalDevice, offScreenFrameBuf.depth.view, nullptr);
		vkDestroyImage(vulkanDevice->logicalDevice, offScreenFrameBuf.depth.image, nullptr);
		vkFreeMemory(vulkanDevice->logicalDevice, offScreenFrameBuf.depth.mem, nullptr);

		vkDestroyFramebuffer(vulkanDevice->logicalDevice, offScreenFrameBuf.frameBuffer, nullptr);

		vkDestroyRenderPass(vulkanDevice->logicalDevice, offScreenFrameBuf.renderPass, nullptr);

		vkDestroySemaphore(vulkanDevice->logicalDevice, gBufferSemaphore, nullptr);
	}

	void LoadScene()
	{
		float posX = 0.0f;
		float posY = 0.0f;
		float posZ = 0.0f;
		AllModel allModel;
		//Load Model Globe
		allModel.modelData.LoadModel("Sofa.fbx", vertexLayout, 0.00005f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("SofaAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("SofaNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.specularTexture.LoadFromFile("SofaSpecular.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.textureCount = 3;
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Box.obj", vertexLayout, 0.05f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.textureCount = 1;
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Block.obj", vertexLayout, 0.5f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("WoodSquare.jpg", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		//allModel.materialData.normalTexture.LoadFromFile("BrickNormal.dds", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		//allModel.materialData.specularTexture.LoadFromFile("BrickSpecular.dds", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.textureCount = 1;
		allModelList.push_back(allModel);

		for (int i = 0; i < MAX_MODEL_COUNT; ++i)
		{
			GameObject gameObj;
			gameObj.model = &(allModelList[0].modelData);
			gameObj.material = &(allModelList[0].materialData);
			gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

			gameObj.transform.positionPerAxis = glm::vec3(posX, -0.50f, posZ);
			gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(180.0f), 0.0f, 0.0f);
			gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);

			scene.gameObjectList.emplace_back(gameObj);
			posZ -= 2.0f;
			if (posZ < -10.0f)
			{
				posX += 2.0f;
				posZ = 0.0f;
			}
		}

		posX = 0.0f;
		posY = 2.0000f;
		posZ = 2.0f;
		for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
		{
			GameObject gameObj;
			gameObj.model = &(allModelList[1].modelData);
			gameObj.material = &(allModelList[1].materialData);
			gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

			gameObj.transform.positionPerAxis = glm::vec3(posX, posY, posZ);
			gameObj.transform.rotateAngelPerAxis = glm::vec3(0.0f, 0.0f, 0.0f);
			gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);

			scene.gameObjectList.emplace_back(gameObj);

			LightInstanceDetails lightDetails;
			lightDetails.position = glm::vec3(posX, posY, posZ);
			lightDetails.color = glm::vec3(1.0f, 1.0f, 1.0f);

			//
			glm::mat4 shadowProj = glm::perspective(lightData.outerCutOff, 1.0f, 0.1f, 100.0f);
			//glm::mat4 shadowProj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
			glm::mat4 shadowView = glm::lookAt(lightDetails.position, lightDetails.position + lightData.direction, glm::vec3(0.0f, 1.0f, 0.0f));

			lightDetails.mvpMatrix = shadowProj * shadowView;
			lightDetails.viewMatrix = shadowView;

			allLightsList.emplace_back(lightDetails);

			posZ += 2.0f;
			if (posZ > 10.0f)
			{
				posX += 2.0f;
				posZ = 0.0f;
			}
		}
		//Load shed model

		GameObject gameObj;
		gameObj.model = &(allModelList[2].modelData);
		gameObj.material = &(allModelList[2].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

		gameObj.transform.positionPerAxis = glm::vec3(0.3f, 0.04f, 0.30f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(0.0f, glm::radians(00.0f), 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(50.0f, 5.0f, 40.0f);

		scene.gameObjectList.emplace_back(gameObj);

		//gameObj.transform.positionPerAxis = glm::vec3(0.0f, 0.5f, -2.0f);
		//gameObj.transform.rotateAngelPerAxis = glm::vec3(0.0f, glm::radians(00.0f), 0.0f);
		//gameObj.transform.scalePerAxis = glm::vec3(15.0f, 5.0f, 1.0f);

		//scene.gameObjectList.emplace_back(gameObj);
	}


	void SetupVertexDescriptions()
	{
		//Specify the Input Binding Description
		VkVertexInputBindingDescription vertexInputBindDescription{};
		vertexInputBindDescription.binding = 0;
		vertexInputBindDescription.stride = vertexLayout.stride();
		vertexInputBindDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		// Binding description
		vertices.bindingDescriptions.resize(1);
		vertices.bindingDescriptions[0] = vertexInputBindDescription;

		// Attribute descriptions
		// Describes memory layout and shader positions
		vertices.attributeDescriptions.resize(5);

		// Location 0 : Position
		VkVertexInputAttributeDescription posVertexInputAttribDescription{};
		posVertexInputAttribDescription.location = 0;
		posVertexInputAttribDescription.binding = 0;
		posVertexInputAttribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
		posVertexInputAttribDescription.offset = 0;

		vertices.attributeDescriptions[0] = posVertexInputAttribDescription;

		// Location 1 : Normal
		VkVertexInputAttributeDescription normVertexInputAttribDescription{};
		normVertexInputAttribDescription.location = 1;
		normVertexInputAttribDescription.binding = 0;
		normVertexInputAttribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
		normVertexInputAttribDescription.offset = sizeof(float) * 3;

		vertices.attributeDescriptions[1] = normVertexInputAttribDescription;

		// Location 2 : Texture coordinates
		VkVertexInputAttributeDescription texVertexInputAttribDescription{};
		texVertexInputAttribDescription.location = 2;
		texVertexInputAttribDescription.binding = 0;
		texVertexInputAttribDescription.format = VK_FORMAT_R32G32_SFLOAT;
		texVertexInputAttribDescription.offset = sizeof(float) * 6;

		vertices.attributeDescriptions[2] = texVertexInputAttribDescription;

		// Location 3 : Color coordinates
		VkVertexInputAttributeDescription colorVertexInputAttribDescription{};
		colorVertexInputAttribDescription.location = 3;
		colorVertexInputAttribDescription.binding = 0;
		colorVertexInputAttribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
		colorVertexInputAttribDescription.offset = sizeof(float) * 8;

		vertices.attributeDescriptions[3] = colorVertexInputAttribDescription;

		// Location 4 : Tangent coordinates
		VkVertexInputAttributeDescription tangentVertexInputAttribDescription{};
		tangentVertexInputAttribDescription.location = 4;
		tangentVertexInputAttribDescription.binding = 0;
		tangentVertexInputAttribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
		tangentVertexInputAttribDescription.offset = sizeof(float) * 11;

		vertices.attributeDescriptions[4] = tangentVertexInputAttribDescription;

		// Create PipelineVertexInputState
		VkPipelineVertexInputStateCreateInfo pipelineVertexInputStateCreateInfo{};
		pipelineVertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		// Assign pipeline vertex to vertices input
		vertices.inputState = pipelineVertexInputStateCreateInfo;
		vertices.inputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertices.bindingDescriptions.size());
		vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
		vertices.inputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertices.attributeDescriptions.size());
		vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
	}
	void CreateAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment *attachment, AttachmentCreateInfo attachmentInfo)
	{
		VkImageAspectFlags aspectMask = 0;
		VkImageLayout imageLayout;

		attachment->format = format;

		if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		}
		if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		assert(aspectMask > 0);

		VkImageCreateInfo image{};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = attachmentInfo.format;
		image.extent.width = attachmentInfo.width;
		image.extent.height = attachmentInfo.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = attachmentInfo.layerCount;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

		VkMemoryAllocateInfo memAlloc{};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;


		VkResult res = vkCreateImage(vulkanDevice->logicalDevice, &image, nullptr, &attachment->image);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image");
		}
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, attachment->image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAlloc, nullptr, &attachment->mem);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Allocate Memory");
		}
		res = vkBindImageMemory(vulkanDevice->logicalDevice, attachment->image, attachment->mem, 0);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Bind Image Memory");
		}

		VkImageViewCreateInfo imageView{};
		imageView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageView.viewType = (attachmentInfo.layerCount == 1) ? VK_IMAGE_VIEW_TYPE_2D : VK_IMAGE_VIEW_TYPE_2D_ARRAY;
		imageView.format = attachmentInfo.format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = attachmentInfo.layerCount;
		imageView.image = attachment->image;
		res = vkCreateImageView(vulkanDevice->logicalDevice, &imageView, nullptr, &attachment->view);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image View");
		}
	}
	void PrepareGBuffer()
	{
		offScreenFrameBuf.width = 2048;
		offScreenFrameBuf.height = 2048;
		AttachmentCreateInfo attachmentInfo;
		attachmentInfo.width = 2048;
		attachmentInfo.height = 2048;
		attachmentInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
		attachmentInfo.layerCount = 1;

		// (World space) Positions
		CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.position, attachmentInfo);

		// (World space) Normals
		CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.normal, attachmentInfo);

		attachmentInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		// Albedo (color)
		CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.albedo, attachmentInfo);

		// Specular (color)
		CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.specular, attachmentInfo);

		attachmentInfo.format = depthFormat;
		// Depth Format
		CreateAttachment(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &offScreenFrameBuf.depth, attachmentInfo);

		// Set up separate renderpass with references to the color and depth attachments
		std::array<VkAttachmentDescription, 5> attachmentDescs = {};

		// Init attachment properties
		for (uint32_t i = 0; i < 5; ++i)
		{
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			if (i == 4)
			{
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
			}
			else
			{
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			}
		}

		// Formats
		attachmentDescs[0].format = offScreenFrameBuf.position.format;
		attachmentDescs[1].format = offScreenFrameBuf.normal.format;
		attachmentDescs[2].format = offScreenFrameBuf.albedo.format;
		attachmentDescs[3].format = offScreenFrameBuf.specular.format;
		attachmentDescs[4].format = offScreenFrameBuf.depth.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 4;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		subpass.pDepthStencilAttachment = &depthReference;

		// Use subpass dependencies for attachment layput transitions
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
		renderPassInfo.pAttachments = attachmentDescs.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();

		VkResult res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &offScreenFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 5> attachments;
		attachments[0] = offScreenFrameBuf.position.view;
		attachments[1] = offScreenFrameBuf.normal.view;
		attachments[2] = offScreenFrameBuf.albedo.view;
		attachments[3] = offScreenFrameBuf.specular.view;
		attachments[4] = offScreenFrameBuf.depth.view;

		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = offScreenFrameBuf.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = offScreenFrameBuf.width;
		fbufCreateInfo.height = offScreenFrameBuf.height;
		fbufCreateInfo.layers = 1;
		res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &offScreenFrameBuf.frameBuffer);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Frame Buffer");
		}

		// Create sampler to sample from the color attachments
		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = VK_FILTER_NEAREST;
		sampler.minFilter = VK_FILTER_NEAREST;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &colorSampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
	}
	void PrepareShadowBuffer()
	{
		AttachmentCreateInfo attachmentInfo;
		attachmentInfo.width = 2048;
		attachmentInfo.height = 2048;
		attachmentInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attachmentInfo.layerCount = 1;


		// Shadow attachment
		CreateAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &shadowFrameBuf.depth, attachmentInfo);
		// Shadow attachment Depth
		attachmentInfo.format = depthFormat;
		CreateAttachment(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &shadowFrameBuf.shadowDepth, attachmentInfo);
		//
		std::array<VkAttachmentDescription, 2> attachmentDescs = {};
		attachmentDescs[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
		attachmentDescs[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDescs[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		attachmentDescs[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachmentDescs[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachmentDescs[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachmentDescs[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachmentDescs[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachmentDescs[1].format = depthFormat;
		attachmentDescs[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachmentDescs[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		// Create sampler to sample from the color attachments
		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1.0f;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		VkResult res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &shadowSampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 1;
		depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = static_cast<uint32_t>(1);
		subpass.pColorAttachments = colorReferences.data();
		subpass.pDepthStencilAttachment = &depthReference;
	
		// Use subpass dependencies for attachment layput transitions
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
		renderPassInfo.pAttachments = attachmentDescs.data();
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 2;
		renderPassInfo.pDependencies = dependencies.data();

		res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &shadowFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 2> attachments;
		attachments[0] = shadowFrameBuf.depth.view;
		attachments[1] = shadowFrameBuf.shadowDepth.view;


		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = shadowFrameBuf.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = attachmentInfo.width;
		fbufCreateInfo.height = attachmentInfo.height;
		fbufCreateInfo.layers = attachmentInfo.layerCount;
		res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &shadowFrameBuf.frameBuffer);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Frame Buffer");
		}


	}
	void PrepareUniformBuffers()
	{
		// Vertex shader uniform buffer block
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertBuffer, sizeof(uboVert));
		// Map persistent
		vertBuffer.Map();
		//
		UpdateUniformBuffers();

		//Light position and color details buffer
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&instanceLightBuffer, allLightsList.size() * sizeof(LightInstanceDetails));
		// Map persistent
		instanceLightBuffer.Map();
		//
		UpdateInstanceLightBuffers();

		//Light position and color details buffer
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&lightDetailsBuffer, sizeof(LightData));

		//Weights details buffer
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&weightBuffer, sizeof(Weights)*101);
		
		weightBuffer.Map();

		UpdateWeightBuffers();
		// Map persistent
		lightDetailsBuffer.Map();
		//
		UpdateLightDataBuffers();
	}
	void UpdateWeightBuffers()
	{
		float totalSum = 0;
		std::vector<Weights> weightValueVec;
		for (int i = 0; i < 101; ++i)
		{
			int i_val = i - 50;
			float power_val = (-1.0f)*((i_val*i_val) / (2.0f*25.0f*25.0f));
			float val = pow(exp(1), power_val);
			totalSum += val;
			Weights temp;
			temp.weight = val;
			weightValueVec.push_back(temp);
		}
		float temp = 0.0f;
		for (int i = 0; i < weightValueVec.size(); ++i)
		{
			weightValueVec[i].weight /= totalSum;
			
		}
		memcpy(weightBuffer.mapped, weightValueVec.data(), sizeof(Weights) * weightValueVec.size());
		
	}
	void UpdateUniformBuffers()
	{
		glm::mat4 viewMatrix = camera->view;
		uboVert.viewMat = viewMatrix;
		uboVert.projViewMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f) * viewMatrix;
		memcpy(vertBuffer.mapped, &uboVert, sizeof(uboVert));
	}
	void UpdateInstanceLightBuffers()
	{
		memcpy(instanceLightBuffer.mapped, allLightsList.data(), allLightsList.size() * sizeof(LightInstanceDetails));
	}
	void UpdateLightDataBuffers()
	{
		memcpy(lightDetailsBuffer.mapped, &lightData, sizeof(LightData));
	}
	void SetupDescriptorSetLayout()
	{
		VkDescriptorSetLayout descriptorSetLayout;

		VkDescriptorSetLayoutBinding uniformSetLayoutBinding{};
		uniformSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
		uniformSetLayoutBinding.binding = 0;
		uniformSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding positionTextureSamplerSetLayoutBinding{};
		positionTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		positionTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		positionTextureSamplerSetLayoutBinding.binding = 1;
		positionTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding albedoSpecDeferredTextureSamplerSetLayoutBinding{};
		albedoSpecDeferredTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSpecDeferredTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		albedoSpecDeferredTextureSamplerSetLayoutBinding.binding = 2;
		albedoSpecDeferredTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding normalDeferredTextureSamplerSetLayoutBinding{};
		normalDeferredTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalDeferredTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		normalDeferredTextureSamplerSetLayoutBinding.binding = 3;
		normalDeferredTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding specularDeferredTextureSamplerSetLayoutBinding{};
		specularDeferredTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		specularDeferredTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		specularDeferredTextureSamplerSetLayoutBinding.binding = 4;
		specularDeferredTextureSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding lightDataSetLayoutBinding{};
		lightDataSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_GEOMETRY_BIT;
		lightDataSetLayoutBinding.binding = 5;
		lightDataSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding instanceLightsSetLayoutBinding{};
		instanceLightsSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instanceLightsSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT;
		instanceLightsSetLayoutBinding.binding = 6;
		instanceLightsSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding albedoModelTextureSamplerSetLayoutBinding{};
		albedoModelTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoModelTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		albedoModelTextureSamplerSetLayoutBinding.binding = 7;
		albedoModelTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding normalModelTextureSamplerSetLayoutBinding{};
		normalModelTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalModelTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		normalModelTextureSamplerSetLayoutBinding.binding = 8;
		normalModelTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding specularModelTextureSamplerSetLayoutBinding{};
		specularModelTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		specularModelTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		specularModelTextureSamplerSetLayoutBinding.binding = 9;
		specularModelTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding shadowTextureSamplerSetLayoutBinding{};
		shadowTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		shadowTextureSamplerSetLayoutBinding.binding = 10;
		shadowTextureSamplerSetLayoutBinding.descriptorCount = 1;

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.push_back(uniformSetLayoutBinding);
		setLayoutBindings.push_back(positionTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(albedoSpecDeferredTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(normalDeferredTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(specularDeferredTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(lightDataSetLayoutBinding);
		setLayoutBindings.push_back(instanceLightsSetLayoutBinding);
		setLayoutBindings.push_back(albedoModelTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(normalModelTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(specularModelTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(shadowTextureSamplerSetLayoutBinding);

		//Push constant for model data
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(PerModelData);


		VkDescriptorSetLayoutCreateInfo descriptorLayout{};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.pBindings = setLayoutBindings.data();
		descriptorLayout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

		VkResult res = vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Descriptor Set Layout");
		}
		// 
		descriptorSetLayouts.emplace_back(descriptorSetLayout);
		descriptorSetLayouts.emplace_back(descriptorSetLayout);
		descriptorSetLayouts.emplace_back(descriptorSetLayout);
		descriptorSetLayouts.emplace_back(descriptorSetLayout);

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo{};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayouts.size();
		pPipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();


		// Push constant ranges are part of the pipeline layout
		pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;


		res = vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.deferred);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Pipeline Layout");
		}



		res = vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayouts.gBuffer);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Pipeline Layout");
		}
	}
	void PreparePipelines()
	{
		VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{};
		inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssemblyState.flags = 0;
		inputAssemblyState.primitiveRestartEnable = VK_FALSE;

		VkPipelineRasterizationStateCreateInfo rasterizationState{};
		rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
		rasterizationState.cullMode = VK_CULL_MODE_NONE;
		rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
		rasterizationState.flags = 0;
		rasterizationState.depthClampEnable = VK_FALSE;
		rasterizationState.lineWidth = 1.0f;

		VkPipelineColorBlendAttachmentState blendAttachmentState{};
		blendAttachmentState.colorWriteMask = 0xf;
		blendAttachmentState.blendEnable = VK_FALSE;

		VkPipelineColorBlendStateCreateInfo colorBlendState{};
		colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		//colorBlendState.attachmentCount = 1;
		//colorBlendState.pAttachments = &blendAttachmentState;

		VkPipelineDepthStencilStateCreateInfo depthStencilState{};
		depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencilState.depthTestEnable = VK_TRUE;
		depthStencilState.depthWriteEnable = VK_TRUE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
		depthStencilState.front = depthStencilState.back;
		depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;
		viewportState.flags = 0;

		VkPipelineMultisampleStateCreateInfo multisampleState{};
		multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampleState.flags = 0;


		std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
		};
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.pDynamicStates = dynamicStateEnables.data();
		dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
		dynamicState.flags = 0;


		// Model rendering pipeline
		// Load shaders
		std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;
		//Get working Debug/Release directory address
		char buffer[MAX_PATH];
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::string::size_type pos = std::string(buffer).find_last_of("\\/");

		std::string fileName = std::string(buffer).substr(0, pos);
		std::size_t found = fileName.find("PC Visual Studio");
		if (found != std::string::npos)
		{
			fileName.erase(fileName.begin() + found, fileName.end());
		}

		// GBuffer shader pipeline
		std::string vertAddress = fileName + "src\\MomentShadowMap\\Shaders\\GBuffer.vert.spv";
		std::string fragAddress = fileName + "src\\MomentShadowMap\\Shaders\\GBuffer.frag.spv";
		//Vertex pipeline shader stage
		VkPipelineShaderStageCreateInfo vertShaderStage = {};
		vertShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStage.module = resourceLoader->LoadShader(vertAddress);
		vertShaderStage.pName = "main";
		assert(vertShaderStage.module != VK_NULL_HANDLE);
		shaderStages[0] = vertShaderStage;

		//fragment pipeline shader stage
		VkPipelineShaderStageCreateInfo fragShaderStage = {};
		fragShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		fragShaderStage.pName = "main";
		assert(fragShaderStage.module != VK_NULL_HANDLE);
		shaderStages[1] = fragShaderStage;

		//
		VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
		pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineCreateInfo.layout = pipelineLayouts.gBuffer;
		pipelineCreateInfo.renderPass = offScreenFrameBuf.renderPass;
		pipelineCreateInfo.flags = 0;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;


		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		// Blend attachment states required for all color attachments
	// This is important, as color write mask will otherwise be 0x0 and you
	// won't see anything rendered to the attachment
		std::array<VkPipelineColorBlendAttachmentState, 4> blendAttachmentStates;

		VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
		pipelineColorBlendAttachmentState.colorWriteMask = 0xf;
		pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
		blendAttachmentStates[0] = pipelineColorBlendAttachmentState;
		blendAttachmentStates[1] = pipelineColorBlendAttachmentState;
		blendAttachmentStates[2] = pipelineColorBlendAttachmentState;
		blendAttachmentStates[3] = pipelineColorBlendAttachmentState;

		colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
		colorBlendState.pAttachments = blendAttachmentStates.data();

		VkResult res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.gBuffer);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);
		////////////////////////////////////////////////
				// Empty vertex input state, quads are generated by the vertex shader
		VkPipelineVertexInputStateCreateInfo emptyInputState{};
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineCreateInfo.pVertexInputState = &emptyInputState;
		pipelineCreateInfo.renderPass = renderPass;
		pipelineCreateInfo.layout = pipelineLayouts.deferred;
		// Deffered shader pipeline
		vertAddress = fileName + "src\\MomentShadowMap\\Shaders\\Deferred.vert.spv";
		fragAddress = fileName + "src\\MomentShadowMap\\Shaders\\Deferred.frag.spv";

		//Vertex pipeline shader stage
		vertShaderStage.module = resourceLoader->LoadShader(vertAddress);

		assert(vertShaderStage.module != VK_NULL_HANDLE);
		shaderStages[0] = vertShaderStage;

		//fragment pipeline shader stage
		fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		assert(fragShaderStage.module != VK_NULL_HANDLE);
		shaderStages[1] = fragShaderStage;

		//

		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();


		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentState;

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.deferred);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}

		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);
		///
		////////
		// Shadow Pipeline
		////////////////////////////////////////////////
		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.renderPass = shadowFrameBuf.renderPass;
		pipelineCreateInfo.layout = pipelineLayouts.gBuffer;
		fragShaderStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

		// Shadow shader pipeline
		vertAddress = fileName + "src\\MomentShadowMap\\Shaders\\Shadow.vert.spv";
		fragAddress = fileName + "src\\MomentShadowMap\\Shaders\\Shadow.frag.spv";

		//Vertex pipeline shader stage
		vertShaderStage.module = resourceLoader->LoadShader(vertAddress);

		assert(vertShaderStage.module != VK_NULL_HANDLE);
		shaderStages[0] = vertShaderStage;

		//fragment pipeline shader stage
		fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		assert(fragShaderStage.module != VK_NULL_HANDLE);
		shaderStages[1] = fragShaderStage;

		//
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		//colorBlendState.attachmentCount = 0;
		//colorBlendState.pAttachments = nullptr;

		//rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
		//rasterizationState.frontFace = VK_FRONT_FACE_CLOCKWISE;
		depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS;
		// Enable depth bias
		rasterizationState.depthBiasEnable = VK_TRUE;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

		// Add depth bias to dynamic state, so we can change it at runtime
		//dynamicStateEnables.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
		//dynamicState.pDynamicStates = dynamicStateEnables.data();
		//dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStateEnables.size());
		//pipelineCreateInfo.pDynamicState = &dynamicState;

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.shadow);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}

		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);
	}
	void SetupDescriptorPool()
	{
		//Descriptor pool for uniform buffer
		VkDescriptorPoolSize uniformDescriptorPoolSize{};
		uniformDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Position Sampler Deferred
		VkDescriptorPoolSize positionSamplerDescriptorPoolSize{};
		positionSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		positionSamplerDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Albedo Specular Sampler Deferred
		VkDescriptorPoolSize albedoSpecDeferredSamplerDescriptorPoolSize{};
		albedoSpecDeferredSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSpecDeferredSamplerDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Normal Sampler Deferred
		VkDescriptorPoolSize normalDeferredSamplerDescriptorPoolSize{};
		normalDeferredSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalDeferredSamplerDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Specular Sampler Deferred
		VkDescriptorPoolSize specularDeferredSamplerDescriptorPoolSize{};
		specularDeferredSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		specularDeferredSamplerDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Albedo Sampler Model
		VkDescriptorPoolSize albedoModelSamplerDescriptorPoolSize{};
		albedoModelSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoModelSamplerDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Normal Sampler Model
		VkDescriptorPoolSize normalModelSamplerDescriptorPoolSize{};
		normalModelSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalModelSamplerDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Specular Sampler Model
		VkDescriptorPoolSize specularModelSamplerDescriptorPoolSize{};
		specularModelSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		specularModelSamplerDescriptorPoolSize.descriptorCount = 12;

		//Descriptor pool for Shadow Map
		VkDescriptorPoolSize shadowMapSamplerDescriptorPoolSize{};
		shadowMapSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowMapSamplerDescriptorPoolSize.descriptorCount = 12;

		// 
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.push_back(uniformDescriptorPoolSize);
		poolSizes.push_back(positionSamplerDescriptorPoolSize);
		poolSizes.push_back(albedoSpecDeferredSamplerDescriptorPoolSize);
		poolSizes.push_back(normalDeferredSamplerDescriptorPoolSize);
		poolSizes.push_back(specularDeferredSamplerDescriptorPoolSize);
		poolSizes.push_back(albedoModelSamplerDescriptorPoolSize);
		poolSizes.push_back(normalModelSamplerDescriptorPoolSize);
		poolSizes.push_back(specularModelSamplerDescriptorPoolSize);
		poolSizes.push_back(shadowMapSamplerDescriptorPoolSize);


		// Create Descriptor pool info
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = MODEL_TYPE_COUNT + 1;

		VkResult res = vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Descriptor Pool");
		}
	}
	void SetupDescriptorSet()
	{
		for (int i = 0; i < MODEL_TYPE_COUNT; ++i)
		{
			VkDescriptorSet modelDescriptorSet;
			descriptorSets.emplace_back(modelDescriptorSet);
		}
		descriptorSets.emplace_back(quad.descriptorSet);
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.pSetLayouts = descriptorSetLayouts.data();
		allocInfo.descriptorSetCount = MODEL_TYPE_COUNT + 1;
		VkResult res = vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, descriptorSets.data());
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Allocate Descriptor Sets");
		}
		//  Image descriptors for the GBuffer color attachments
		VkDescriptorImageInfo positionDescriptorImageInfo{};
		positionDescriptorImageInfo.sampler = colorSampler;
		positionDescriptorImageInfo.imageView = offScreenFrameBuf.position.view;
		positionDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo albedoDescriptorImageInfo{};
		albedoDescriptorImageInfo.sampler = colorSampler;
		albedoDescriptorImageInfo.imageView = offScreenFrameBuf.albedo.view;
		albedoDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo normalDescriptorImageInfo{};
		normalDescriptorImageInfo.sampler = colorSampler;
		normalDescriptorImageInfo.imageView = offScreenFrameBuf.normal.view;
		normalDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo specularDescriptorImageInfo{};
		specularDescriptorImageInfo.sampler = colorSampler;
		specularDescriptorImageInfo.imageView = offScreenFrameBuf.specular.view;
		specularDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo shadowDescriptorImageInfo{};
		shadowDescriptorImageInfo.sampler = shadowSampler;
		shadowDescriptorImageInfo.imageView = shadowFrameBuf.depth.view;
		shadowDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		//
			// Binding 0 : Vertex shader uniform buffer
		VkWriteDescriptorSet uniforBufferWriteDescriptorSet{};
		uniforBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniforBufferWriteDescriptorSet.dstSet = descriptorSets[3];
		uniforBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniforBufferWriteDescriptorSet.dstBinding = 0;
		uniforBufferWriteDescriptorSet.pBufferInfo = &(vertBuffer.descriptor);
		uniforBufferWriteDescriptorSet.descriptorCount = 1;

		//  Binding 1 : Positon Texture
		VkWriteDescriptorSet positionSamplerWriteDescriptorSet{};
		positionSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		positionSamplerWriteDescriptorSet.dstSet = descriptorSets[3];
		positionSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		positionSamplerWriteDescriptorSet.dstBinding = 1;
		positionSamplerWriteDescriptorSet.pImageInfo = &positionDescriptorImageInfo;
		positionSamplerWriteDescriptorSet.descriptorCount = 1;

		//  Binding 2 : Albedo Texture Deferred
		VkWriteDescriptorSet albedoSamplerWriteDescriptorSet{};
		albedoSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		albedoSamplerWriteDescriptorSet.dstSet = descriptorSets[3];
		albedoSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSamplerWriteDescriptorSet.dstBinding = 2;
		albedoSamplerWriteDescriptorSet.pImageInfo = &albedoDescriptorImageInfo;
		albedoSamplerWriteDescriptorSet.descriptorCount = 1;

		//  Binding 3 : Normal Texture Deferred
		VkWriteDescriptorSet normalSamplerWriteDescriptorSet{};
		normalSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normalSamplerWriteDescriptorSet.dstSet = descriptorSets[3];
		normalSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalSamplerWriteDescriptorSet.dstBinding = 3;
		normalSamplerWriteDescriptorSet.pImageInfo = &normalDescriptorImageInfo;
		normalSamplerWriteDescriptorSet.descriptorCount = 1;

		//  Binding 4 : Specular Texture Deferred
		VkWriteDescriptorSet specularSamplerWriteDescriptorSet{};
		specularSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		specularSamplerWriteDescriptorSet.dstSet = descriptorSets[3];
		specularSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		specularSamplerWriteDescriptorSet.dstBinding = 4;
		specularSamplerWriteDescriptorSet.pImageInfo = &specularDescriptorImageInfo;
		specularSamplerWriteDescriptorSet.descriptorCount = 1;

		// Binding 5 : Light data buffer
		VkWriteDescriptorSet lightDataBufferWriteDescriptorSet{};
		lightDataBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightDataBufferWriteDescriptorSet.dstSet = descriptorSets[3];
		lightDataBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataBufferWriteDescriptorSet.dstBinding = 5;
		lightDataBufferWriteDescriptorSet.pBufferInfo = &(lightDetailsBuffer.descriptor);
		lightDataBufferWriteDescriptorSet.descriptorCount = 1;

		// Binding 6 : Light Instance
		VkWriteDescriptorSet instancLightBufferWriteDescriptorSet{};
		instancLightBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		instancLightBufferWriteDescriptorSet.dstSet = descriptorSets[3];
		instancLightBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instancLightBufferWriteDescriptorSet.dstBinding = 6;
		instancLightBufferWriteDescriptorSet.pBufferInfo = &(instanceLightBuffer.descriptor);
		instancLightBufferWriteDescriptorSet.descriptorCount = 1;


		//  Binding 10 : Shadow Texture Deferred
		VkWriteDescriptorSet shadowSamplerWriteDescriptorSet{};
		shadowSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowSamplerWriteDescriptorSet.dstSet = descriptorSets[3];
		shadowSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		shadowSamplerWriteDescriptorSet.dstBinding = 10;
		shadowSamplerWriteDescriptorSet.pImageInfo = &shadowDescriptorImageInfo;
		shadowSamplerWriteDescriptorSet.descriptorCount = 1;

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet);
		writeDescriptorSets.push_back(positionSamplerWriteDescriptorSet);
		writeDescriptorSets.push_back(albedoSamplerWriteDescriptorSet);
		writeDescriptorSets.push_back(normalSamplerWriteDescriptorSet);
		writeDescriptorSets.push_back(specularSamplerWriteDescriptorSet);
		writeDescriptorSets.push_back(lightDataBufferWriteDescriptorSet);
		writeDescriptorSets.push_back(instancLightBufferWriteDescriptorSet);
		writeDescriptorSets.push_back(shadowSamplerWriteDescriptorSet);

		vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		//
		for (int i = 0; i < MODEL_TYPE_COUNT; ++i)
		{
			// Binding 0 : Vertex shader uniform buffer
			VkWriteDescriptorSet uniforBufferWriteDescriptorSet{};
			uniforBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			uniforBufferWriteDescriptorSet.dstSet = descriptorSets[i];
			uniforBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uniforBufferWriteDescriptorSet.dstBinding = 0;
			uniforBufferWriteDescriptorSet.pBufferInfo = &(vertBuffer.descriptor);
			uniforBufferWriteDescriptorSet.descriptorCount = 1;

			//  Binding 1 : Positon Texture
			VkWriteDescriptorSet positionSamplerWriteDescriptorSet{};
			positionSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			positionSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			positionSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			positionSamplerWriteDescriptorSet.dstBinding = 1;
			positionSamplerWriteDescriptorSet.pImageInfo = &positionDescriptorImageInfo;
			positionSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 2 : Albedo Texture Deferred
			VkWriteDescriptorSet albedoSamplerWriteDescriptorSet{};
			albedoSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			albedoSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			albedoSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			albedoSamplerWriteDescriptorSet.dstBinding = 2;
			albedoSamplerWriteDescriptorSet.pImageInfo = &albedoDescriptorImageInfo;
			albedoSamplerWriteDescriptorSet.descriptorCount = 1;


			//  Binding 3 : Normal Texture Deferred
			VkWriteDescriptorSet normalSamplerWriteDescriptorSet{};
			normalSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			normalSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			normalSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			normalSamplerWriteDescriptorSet.dstBinding = 3;
			normalSamplerWriteDescriptorSet.pImageInfo = &normalDescriptorImageInfo;
			normalSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 4 : Specular Texture Deferred
			VkWriteDescriptorSet specularSamplerWriteDescriptorSet{};
			specularSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			specularSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			specularSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			specularSamplerWriteDescriptorSet.dstBinding = 4;
			specularSamplerWriteDescriptorSet.pImageInfo = &specularDescriptorImageInfo;
			specularSamplerWriteDescriptorSet.descriptorCount = 1;

			// Binding 5 : Light data buffer
			VkWriteDescriptorSet lightDataBufferWriteDescriptorSet{};
			lightDataBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			lightDataBufferWriteDescriptorSet.dstSet = descriptorSets[i];
			lightDataBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			lightDataBufferWriteDescriptorSet.dstBinding = 5;
			lightDataBufferWriteDescriptorSet.pBufferInfo = &(lightDetailsBuffer.descriptor);
			lightDataBufferWriteDescriptorSet.descriptorCount = 1;

			// Binding 6 : Light Instance
			VkWriteDescriptorSet instancLightBufferWriteDescriptorSet{};
			instancLightBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			instancLightBufferWriteDescriptorSet.dstSet = descriptorSets[i];
			instancLightBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			instancLightBufferWriteDescriptorSet.dstBinding = 6;
			instancLightBufferWriteDescriptorSet.pBufferInfo = &(instanceLightBuffer.descriptor);
			instancLightBufferWriteDescriptorSet.descriptorCount = 1;

			//  Binding 7 : Model Albedo Texture
			VkWriteDescriptorSet albedoModelSamplerWriteDescriptorSet{};
			albedoModelSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			albedoModelSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			albedoModelSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			albedoModelSamplerWriteDescriptorSet.dstBinding = 7;
			albedoModelSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.albedoTexture.descriptor);
			albedoModelSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 8 : Model Normal Texture
			VkWriteDescriptorSet normalModelSamplerWriteDescriptorSet{};
			normalModelSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			normalModelSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			normalModelSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			normalModelSamplerWriteDescriptorSet.dstBinding = 8;
			normalModelSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.normalTexture.descriptor);
			normalModelSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 9 : Model Specular Texture
			VkWriteDescriptorSet specularModelSamplerWriteDescriptorSet{};
			specularModelSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			specularModelSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			specularModelSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			specularModelSamplerWriteDescriptorSet.dstBinding = 9;
			specularModelSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.specularTexture.descriptor);
			specularModelSamplerWriteDescriptorSet.descriptorCount = 1;



			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(positionSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(albedoSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(normalSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(specularSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(lightDataBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(instancLightBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(albedoModelSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(normalModelSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(specularModelSamplerWriteDescriptorSet);


			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);


		}
	}
	void PreparecomputeTextureIn()
	{
		if (temporaryCmdBuffer == VK_NULL_HANDLE)
		{
			temporaryCmdBuffer = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
		}
		VkFormatProperties formatProperties;

		// Get device properties for the requested texture format
		vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, VK_FORMAT_R32G32B32A32_SFLOAT, &formatProperties);
		// Check if requested image format supports image storage operations
		assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		// Prepare blit target texture
		computeTextureIn.width = 2048;
		computeTextureIn.height = 2048;

		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageCreateInfo.extent = { computeTextureIn.width, computeTextureIn.height , 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		// Image will be sampled in the fragment shader and used as storage target in the compute shader
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL | VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageCreateInfo.flags = 0;
		// Sharing mode exclusive means that ownership of the image does not need to be explicitly transferred between the compute and graphics queue
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;

		vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &computeTextureIn.image);

		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, computeTextureIn.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &computeTextureIn.deviceMemory);
		vkBindImageMemory(vulkanDevice->logicalDevice, computeTextureIn.image, computeTextureIn.deviceMemory, 0);

		//VkCommandBuffer layoutCmd;
		////
		//VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
		//cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		//cmdBufAllocateInfo.commandPool = vulkanDevice->defaultCommandPool;
		//cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		//cmdBufAllocateInfo.commandBufferCount = 1;

		//VkResult res = vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufAllocateInfo, &layoutCmd);
		//if (res != VK_SUCCESS) {
		//	throw std::runtime_error("Error: Allocate Command Buffers");
		//}

		/*VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkResult res = vkBeginCommandBuffer(deferredCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}*/

		computeTextureIn.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureIn.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, computeTextureIn.imageLayout);

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);


		// Create sampler
		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &computeTextureIn.sampler);

		// Create image view
		VkImageViewCreateInfo view{};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.image = VK_NULL_HANDLE;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.image = computeTextureIn.image;
		vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &computeTextureIn.view);

		// Initialize a descriptor for later use
		computeTextureIn.descriptor.imageLayout = computeTextureIn.imageLayout;
		computeTextureIn.descriptor.imageView = computeTextureIn.view;
		computeTextureIn.descriptor.sampler = computeTextureIn.sampler;
		//computeTextureIn.device = vulkanDevice;
	}
	void PreparecomputeTextureOut()
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VkResult res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}
	
		VkFormatProperties formatProperties;

		// Get device properties for the requested texture format
		vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, VK_FORMAT_R32G32B32A32_SFLOAT, &formatProperties);
		// Check if requested image format supports image storage operations
		assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		// Prepare blit target texture
		computeTextureOut.width = 2048;
		computeTextureOut.height = 2048;

		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageCreateInfo.extent = { computeTextureOut.width, computeTextureOut.height , 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		// Image will be sampled in the fragment shader and used as storage target in the compute shader
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL | VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageCreateInfo.flags = 0;
		// Sharing mode exclusive means that ownership of the image does not need to be explicitly transferred between the compute and graphics queue
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;

		vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &computeTextureOut.image);

		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, computeTextureOut.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &computeTextureOut.deviceMemory);
		vkBindImageMemory(vulkanDevice->logicalDevice, computeTextureOut.image, computeTextureOut.deviceMemory, 0);

		

		computeTextureOut.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureOut.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, computeTextureOut.imageLayout);

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);


		// Create sampler
		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &computeTextureOut.sampler);

		// Create image view
		VkImageViewCreateInfo view{};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.image = VK_NULL_HANDLE;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.image = computeTextureOut.image;
		vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &computeTextureOut.view);

		// Initialize a descriptor for later use
		computeTextureOut.descriptor.imageLayout = computeTextureOut.imageLayout;
		computeTextureOut.descriptor.imageView = computeTextureOut.view;
		computeTextureOut.descriptor.sampler = computeTextureOut.sampler;
		//computeTextureIn.device = vulkanDevice;
	}
	void PreparecomputeTextureFinal()
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		VkResult res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}

		VkFormatProperties formatProperties;

		// Get device properties for the requested texture format
		vkGetPhysicalDeviceFormatProperties(vulkanDevice->physicalDevice, VK_FORMAT_R32G32B32A32_SFLOAT, &formatProperties);
		// Check if requested image format supports image storage operations
		assert(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT);

		// Prepare blit target texture
		computeTextureFinal.width = 2048;
		computeTextureFinal.height = 2048;

		VkImageCreateInfo imageCreateInfo{};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		imageCreateInfo.extent = { computeTextureFinal.width, computeTextureFinal.height , 1 };
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		// Image will be sampled in the fragment shader and used as storage target in the compute shader
		imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL | VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		imageCreateInfo.flags = 0;
		// Sharing mode exclusive means that ownership of the image does not need to be explicitly transferred between the compute and graphics queue
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		VkMemoryAllocateInfo memAllocInfo{};
		memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;

		vkCreateImage(vulkanDevice->logicalDevice, &imageCreateInfo, nullptr, &computeTextureFinal.image);

		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, computeTextureFinal.image, &memReqs);
		memAllocInfo.allocationSize = memReqs.size;
		memAllocInfo.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		vkAllocateMemory(vulkanDevice->logicalDevice, &memAllocInfo, nullptr, &computeTextureFinal.deviceMemory);
		vkBindImageMemory(vulkanDevice->logicalDevice, computeTextureFinal.image, computeTextureFinal.deviceMemory, 0);



		computeTextureFinal.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureFinal.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, computeTextureFinal.imageLayout);

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);


		// Create sampler
		VkSamplerCreateInfo sampler{};
		sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		sampler.maxAnisotropy = 1.0f;
		sampler.magFilter = VK_FILTER_LINEAR;
		sampler.minFilter = VK_FILTER_LINEAR;
		sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		sampler.addressModeV = sampler.addressModeU;
		sampler.addressModeW = sampler.addressModeU;
		sampler.mipLodBias = 0.0f;
		sampler.maxAnisotropy = 1.0f;
		sampler.compareOp = VK_COMPARE_OP_NEVER;
		sampler.minLod = 0.0f;
		sampler.maxLod = 1;
		sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &computeTextureFinal.sampler);

		// Create image view
		VkImageViewCreateInfo view{};
		view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		view.image = VK_NULL_HANDLE;
		view.viewType = VK_IMAGE_VIEW_TYPE_2D;
		view.format = VK_FORMAT_R32G32B32A32_SFLOAT;
		view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
		view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
		view.image = computeTextureFinal.image;
		vkCreateImageView(vulkanDevice->logicalDevice, &view, nullptr, &computeTextureFinal.view);

		// Initialize a descriptor for later use
		computeTextureFinal.descriptor.imageLayout = computeTextureFinal.imageLayout;
		computeTextureFinal.descriptor.imageView = computeTextureFinal.view;
		computeTextureFinal.descriptor.sampler = computeTextureFinal.sampler;
		//computeTextureIn.device = vulkanDevice;
	}
	void CreaateComputePipeline()
	{
		VkDescriptorSetLayout descriptorSetLayout;

		VkDescriptorSetLayoutBinding inputImageLayoutBinding{};
		inputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		inputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		inputImageLayoutBinding.binding = 0;
		inputImageLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding outputImageLayoutBinding{};
		outputImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		outputImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		outputImageLayoutBinding.binding = 1;
		outputImageLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding weightsImageLayoutBinding{};
		weightsImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		weightsImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		weightsImageLayoutBinding.binding = 2;
		weightsImageLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding finalImageLayoutBinding{};
		finalImageLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		finalImageLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
		finalImageLayoutBinding.binding = 3;
		finalImageLayoutBinding.descriptorCount = 1;

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.push_back(inputImageLayoutBinding);
		setLayoutBindings.push_back(outputImageLayoutBinding);
		setLayoutBindings.push_back(weightsImageLayoutBinding);
		setLayoutBindings.push_back(finalImageLayoutBinding);

		VkDescriptorSetLayoutCreateInfo descriptorLayout{};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.pBindings = setLayoutBindings.data();
		descriptorLayout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

		VkResult res = vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Descriptor Set Layout");
		}

		computeDescriptorSetLayouts.emplace_back(descriptorSetLayout);

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo{};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.setLayoutCount = 1;
		pPipelineLayoutCreateInfo.pSetLayouts = computeDescriptorSetLayouts.data();

		res = vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &computePipelineLayout);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Pipeline Layout");
		}

		//Descriptor pool for Shadow Map
		VkDescriptorPoolSize computeShadowMapSamplerDescriptorPoolSize{};
		computeShadowMapSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		computeShadowMapSamplerDescriptorPoolSize.descriptorCount = 1;

		std::vector<VkDescriptorPoolSize> poolSizes;

		poolSizes.push_back(computeShadowMapSamplerDescriptorPoolSize);

		// Create Descriptor pool info
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = 1;

		res = vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &computeDescriptorPool);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Descriptor Pool");
		}

		VkDescriptorSet imageDescriptorSet;

		computeDescriptorSets.emplace_back(imageDescriptorSet);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = computeDescriptorPool;
		allocInfo.pSetLayouts = computeDescriptorSetLayouts.data();
		allocInfo.descriptorSetCount = 1;

		res = vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, computeDescriptorSets.data());
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Allocate Descriptor Sets");
		}
		//Shadow
		VkDescriptorImageInfo shadowDescriptorImageInfo{};
		shadowDescriptorImageInfo.sampler = computeTextureIn.sampler;
		shadowDescriptorImageInfo.imageView = computeTextureIn.view;
		shadowDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkDescriptorImageInfo shadowOutPutDescriptorImageInfo{};
		shadowOutPutDescriptorImageInfo.sampler = computeTextureOut.sampler;
		shadowOutPutDescriptorImageInfo.imageView = computeTextureOut.view;
		shadowOutPutDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkDescriptorImageInfo shadowFinalPutDescriptorImageInfo{};
		shadowFinalPutDescriptorImageInfo.sampler = computeTextureFinal.sampler;
		shadowFinalPutDescriptorImageInfo.imageView = computeTextureFinal.view;
		shadowFinalPutDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		

		// Binding 0 : Input Image compute shader 
		VkWriteDescriptorSet imageComputeWriteDescriptorSet{};
		imageComputeWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageComputeWriteDescriptorSet.dstSet = computeDescriptorSets[0];
		imageComputeWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imageComputeWriteDescriptorSet.dstBinding = 0;
		imageComputeWriteDescriptorSet.pImageInfo = &(shadowDescriptorImageInfo);
		imageComputeWriteDescriptorSet.descriptorCount = 1;

		// Binding 1 : out Put Image compute shader 
		VkWriteDescriptorSet imageOutPutComputeWriteDescriptorSet{};
		imageOutPutComputeWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageOutPutComputeWriteDescriptorSet.dstSet = computeDescriptorSets[0];
		imageOutPutComputeWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imageOutPutComputeWriteDescriptorSet.dstBinding = 1;
		imageOutPutComputeWriteDescriptorSet.pImageInfo = &(shadowOutPutDescriptorImageInfo);
		imageOutPutComputeWriteDescriptorSet.descriptorCount = 1;
	

		// Binding 2 : Weights compute shader 
		VkWriteDescriptorSet weightsComputeWriteDescriptorSet{};
		weightsComputeWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		weightsComputeWriteDescriptorSet.dstSet = computeDescriptorSets[0];
		weightsComputeWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		weightsComputeWriteDescriptorSet.dstBinding = 2;
		weightsComputeWriteDescriptorSet.pBufferInfo = &(weightBuffer.descriptor);
		weightsComputeWriteDescriptorSet.descriptorCount = 1;

		// Binding 3 : final Image compute shader 
		VkWriteDescriptorSet imageFinalComputeWriteDescriptorSet{};
		imageFinalComputeWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		imageFinalComputeWriteDescriptorSet.dstSet = computeDescriptorSets[0];
		imageFinalComputeWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		imageFinalComputeWriteDescriptorSet.dstBinding = 3;
		imageFinalComputeWriteDescriptorSet.pImageInfo = &(shadowFinalPutDescriptorImageInfo);
		imageFinalComputeWriteDescriptorSet.descriptorCount = 1;
		
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		
		writeDescriptorSets.push_back(imageComputeWriteDescriptorSet);
		writeDescriptorSets.push_back(imageOutPutComputeWriteDescriptorSet);
		writeDescriptorSets.push_back(weightsComputeWriteDescriptorSet);
		writeDescriptorSets.push_back(imageFinalComputeWriteDescriptorSet);

		vkUpdateDescriptorSets(vulkanDevice->logicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);

		VkComputePipelineCreateInfo computePipelineCreateInfo{};
		computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		computePipelineCreateInfo.layout = computePipelineLayout;
		computePipelineCreateInfo.flags = 0;

		//Get working Debug/Release directory address
		char buffer[MAX_PATH];
		GetModuleFileName(NULL, buffer, MAX_PATH);
		std::string::size_type pos = std::string(buffer).find_last_of("\\/");

		std::string fileName = std::string(buffer).substr(0, pos);
		std::size_t found = fileName.find("PC Visual Studio");
		if (found != std::string::npos)
		{
			fileName.erase(fileName.begin() + found, fileName.end());
		}
		std::string compAddress = fileName + "src\\MomentShadowMap\\Shaders\\MomentShadowMapH.comp.spv";
		//Vertex pipeline shader stage
		VkPipelineShaderStageCreateInfo computeShaderStage = {};
		computeShaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		computeShaderStage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		computeShaderStage.module = resourceLoader->LoadShader(compAddress);
		computeShaderStage.pName = "main";
		assert(computeShaderStage.module != VK_NULL_HANDLE);

		computePipelineCreateInfo.stage = computeShaderStage;

		res = vkCreateComputePipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &computePipelineHorizontal);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Compute Pipelines");
		}
		/////////////////////////////////////////////////////////////////////
		compAddress = fileName + "src\\MomentShadowMap\\Shaders\\MomentShadowMapV.comp.spv";
		//Vertex pipeline shader stage
		
		computeShaderStage.module = resourceLoader->LoadShader(compAddress);
		computeShaderStage.pName = "main";
		assert(computeShaderStage.module != VK_NULL_HANDLE);

		computePipelineCreateInfo.stage = computeShaderStage;

		res = vkCreateComputePipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &computePipelineCreateInfo, nullptr, &computePipelineVertical);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Compute Pipelines");
		}



		// Separate command pool as queue family for compute may be different than graphics
		VkCommandPoolCreateInfo cmdPoolInfo = {};
		cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		cmdPoolInfo.queueFamilyIndex = vulkanDevice->getQueueFamilyIndex(VK_QUEUE_COMPUTE_BIT);
		cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
		vkCreateCommandPool(vulkanDevice->logicalDevice, &cmdPoolInfo, nullptr, &computeCommandPool);

		// Create a command buffer for compute operations
		VkCommandBufferAllocateInfo cmdBufAllocateInfo{};
		cmdBufAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmdBufAllocateInfo.commandPool = computeCommandPool;
		cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmdBufAllocateInfo.commandBufferCount = 1;


		vkAllocateCommandBuffers(vulkanDevice->logicalDevice, &cmdBufAllocateInfo, &computeCmdBuffer);

	}
	void DrawModels()
	{
		
		//Draw all models 
		vkCmdBindDescriptorSets(deferredCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.gBuffer, 0, 1, &descriptorSets[0], 0, NULL);
		for (int i = 0; i < MAX_MODEL_COUNT; ++i)
		{


			UpdateModelBuffer(scene.gameObjectList[i]);
			VkDeviceSize offsets[1] = { 0 };
			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(deferredCmdBuffer, 0, 1, &scene.gameObjectList[i].model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(deferredCmdBuffer, scene.gameObjectList[i].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(deferredCmdBuffer, scene.gameObjectList[i].model->indexCount, 1, 0, 0, 0);

		}
		//vkCmdBindDescriptorSets(deferredCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.gBuffer, 0, 1, &descriptorSets[1], 0, NULL);
		//for (int i = MAX_MODEL_COUNT; i < MAX_LIGHT_COUNT + MAX_MODEL_COUNT; ++i)
		//{


		//	//UpdateModelBuffer(scene.gameObjectList[i]);
		//	VkDeviceSize offsets[1] = { 0 };
		//	// Bind mesh vertex buffer
		////	vkCmdBindVertexBuffers(deferredCmdBuffer, 0, 1, &scene.gameObjectList[i].model->vertices.buffer, offsets);
		//	// Bind mesh index buffer
		//	//vkCmdBindIndexBuffer(deferredCmdBuffer, scene.gameObjectList[i].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		//	// Render mesh vertex buffer using it's indices
		//	//vkCmdDrawIndexed(deferredCmdBuffer, scene.gameObjectList[i].model->indexCount, 1, 0, 0, 0);

		//}
		// Draw Shed model
		vkCmdBindDescriptorSets(deferredCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.gBuffer, 0, 1, &descriptorSets[2], 0, NULL);
		VkDeviceSize offsets[1] = { 0 };

		//UpdateModelBuffer(scene.gameObjectList[scene.gameObjectList.size() - 2]);
		////// Bind mesh vertex buffer
		//vkCmdBindVertexBuffers(deferredCmdBuffer, 0, 1, &scene.gameObjectList[scene.gameObjectList.size() - 2].model->vertices.buffer, offsets);
		//// Bind mesh index buffer
		//vkCmdBindIndexBuffer(deferredCmdBuffer, scene.gameObjectList[scene.gameObjectList.size() - 2].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		//// Render mesh vertex buffer using it's indices
		//vkCmdDrawIndexed(deferredCmdBuffer, scene.gameObjectList[scene.gameObjectList.size() - 2].model->indexCount, 1, 0, 0, 0);

		UpdateModelBuffer(scene.gameObjectList[scene.gameObjectList.size() - 1]);
		//// Bind mesh vertex buffer
		vkCmdBindVertexBuffers(deferredCmdBuffer, 0, 1, &scene.gameObjectList[scene.gameObjectList.size() - 1].model->vertices.buffer, offsets);
		// Bind mesh index buffer
		vkCmdBindIndexBuffer(deferredCmdBuffer, scene.gameObjectList[scene.gameObjectList.size() - 1].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		// Render mesh vertex buffer using it's indices
		vkCmdDrawIndexed(deferredCmdBuffer, scene.gameObjectList[scene.gameObjectList.size() - 1].model->indexCount, 1, 0, 0, 0);

		
		
	}

	void UpdateModelBuffer(GameObject _gameObject)
	{
		perModelData.color = _gameObject.gameObjectDetails.color;
		glm::mat4 model = glm::mat4(1);
		model = glm::scale(model, _gameObject.transform.scalePerAxis);
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.z, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::translate(model, glm::vec3(_gameObject.transform.positionPerAxis.x, _gameObject.transform.positionPerAxis.y, _gameObject.transform.positionPerAxis.z));
		perModelData.modelMat = model;
		perModelData.textureCount = _gameObject.material->textureCount;
		vkCmdPushConstants(deferredCmdBuffer, pipelineLayouts.gBuffer, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
	}



	void BuildCommandBuffers()
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkClearValue clearValues[2];
		clearValues[0].color = defaultClearColor;
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
		{
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = frameBuffers[i];

			VkResult res = vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("Error: Begin Command Buffer");
			}
			vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport{};
			viewport.width = (float)width;
			viewport.height = (float)height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			//viewport.x = 0;
			//viewport.y = float(height);


			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);



			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayouts.deferred, 0, 1, &descriptorSets[3], 0, NULL);

			// Final composition as full screen quad
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.deferred);
			vkCmdPushConstants(drawCmdBuffers[i], pipelineLayouts.deferred, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
			//vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &quad.vertexBuffer.buffer, offsets);
			//vkCmdBindIndexBuffer(drawCmdBuffers[i], quad.indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
			//vkCmdDrawIndexed(drawCmdBuffers[i], 6, 1, 0, 0, 1);
			vkCmdDraw(drawCmdBuffers[i], 6, 1, 0, 0);
			// IMGUI
				// Render imGui
			DrawGUI(drawCmdBuffers[i]);

			vkCmdEndRenderPass(drawCmdBuffers[i]);

			res = vkEndCommandBuffer(drawCmdBuffers[i]);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("Error: End Command Buffer");
			}
		}
	}
	void BuildDeferredCommandBuffer()
	{
		if (deferredCmdBuffer == VK_NULL_HANDLE)
		{
			deferredCmdBuffer = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}

		// Create a semaphore used to synchronize offscreen rendering and usage

		VkSemaphoreCreateInfo semaphoreCreateInfo{};
		semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkResult res = vkCreateSemaphore(vulkanDevice->logicalDevice, &semaphoreCreateInfo, nullptr, &gBufferSemaphore);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Semaphore");
		}


		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		// Clear values for all attachments written in the fragment sahder
		std::array<VkClearValue, 5> clearValues;


		VkRenderPassBeginInfo renderPassBeginInfo{};


		res = vkBeginCommandBuffer(deferredCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}


		VkViewport viewport{};
		viewport.width = (float)2048;
		viewport.height = (float)2048;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		//viewport.x = 0;
		//viewport.y = float(2048);

		vkCmdSetViewport(deferredCmdBuffer, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent.width = 2048;
		scissor.extent.height = 2048;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(deferredCmdBuffer, 0, 1, &scissor);

		// Shadow Pass
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = shadowFrameBuf.renderPass;
		renderPassBeginInfo.framebuffer = shadowFrameBuf.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = 2048;
		renderPassBeginInfo.renderArea.extent.height = 2048;
		renderPassBeginInfo.clearValueCount = 2;

		renderPassBeginInfo.pClearValues = clearValues.data();
		// Set depth bias (aka "Polygon offset")
		//vkCmdSetDepthBias(deferredCmdBuffer, depthBiasConstant, 0.0f, depthBiasSlope);

		vkCmdBeginRenderPass(deferredCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(deferredCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.shadow);

		DrawModels();
		vkCmdEndRenderPass(deferredCmdBuffer);

		clearValues.empty();
		////////////////////////////////////////////////////
		viewport.width = (float)offScreenFrameBuf.width;
		viewport.height = -(float)offScreenFrameBuf.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = offScreenFrameBuf.height;

		vkCmdSetViewport(deferredCmdBuffer, 0, 1, &viewport);

		scissor.extent.width = offScreenFrameBuf.width;
		scissor.extent.height = offScreenFrameBuf.height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(deferredCmdBuffer, 0, 1, &scissor);

		// GBuffer render target clear
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[3].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[4].depthStencil = { 1.0f, 0 };

		// Setup render pass for GBuffer

		renderPassBeginInfo.renderPass = offScreenFrameBuf.renderPass;
		renderPassBeginInfo.framebuffer = offScreenFrameBuf.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = offScreenFrameBuf.width;
		renderPassBeginInfo.renderArea.extent.height = offScreenFrameBuf.height;
		renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassBeginInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(deferredCmdBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
		vkCmdBindPipeline(deferredCmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.gBuffer);

		VkDeviceSize offsets[1] = { 0 };

		DrawModels();

		vkCmdEndRenderPass(deferredCmdBuffer);

		vkEndCommandBuffer(deferredCmdBuffer);

	}
	void  BuildComputeCommandBufferHorizontal()
	{
		if (computeCmdBuffer == VK_NULL_HANDLE)
		{
			computeCmdBuffer = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		vkBeginCommandBuffer(computeCmdBuffer, &cmdBufInfo);

		vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineHorizontal);
		vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSets[0], 0, 0);

		vkCmdDispatch(computeCmdBuffer,  (2048 / 128), 2048, 1);

		vkEndCommandBuffer(computeCmdBuffer);
	}
	void  BuildComputeCommandBufferVertical()
	{
		/*if (computeCmdBuffer == VK_NULL_HANDLE)
		{
			computeCmdBuffer = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
		}*/
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		vkBeginCommandBuffer(computeCmdBuffer, &cmdBufInfo);

		vkCmdBindPipeline(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineVertical);
		vkCmdBindDescriptorSets(computeCmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout, 0, 1, &computeDescriptorSets[0], 0, 0);

		vkCmdDispatch(computeCmdBuffer, 2048 , (2048/128), 1);

		vkEndCommandBuffer(computeCmdBuffer);
	}
	void Prepare()
	{
		VulkanRenderer::Prepare();
		LoadScene();
		SetupVertexDescriptions();
		PrepareGBuffer();
		PrepareShadowBuffer();
		PrepareUniformBuffers();
		SetupDescriptorSetLayout();
		PreparePipelines();
		SetupDescriptorPool();
		PreparecomputeTextureIn();
		PreparecomputeTextureOut();
		PreparecomputeTextureFinal();
		SetupDescriptorSet();

		CreaateComputePipeline();
		BuildCommandBuffers();
		BuildDeferredCommandBuffer();
		
		//
		
		prepared = true;

	}
	virtual void Update()
	{
		uboVert.viewPos = camera->position;
		camera->UpdateViewMatrix();
		UpdateUniformBuffers();

	}
	void DrawLoop()
	{

		// Get next image in the swap chain (back/front buffer)
		swapChain->AcquireNextImage(semaphores.presentComplete, &currentBuffer);


		// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// The submit info structure specifices a command buffer queue submission batch
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
		submitInfo.pWaitSemaphores = &semaphores.presentComplete;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
		submitInfo.waitSemaphoreCount = 1;												// One wait semaphore																				
		submitInfo.pSignalSemaphores = &gBufferSemaphore;						// Semaphore(s) to be signaled when command buffers have completed
		submitInfo.signalSemaphoreCount = 1;											// One signal semaphore
		submitInfo.pCommandBuffers = &deferredCmdBuffer;
		submitInfo.commandBufferCount = 1;												// One command buffer


		vkQueueSubmit(queue.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue.graphicQueue);
		/////////////////////////////////////////////////////////////////////////////////////
		
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkResult res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}

		vulkanHelper->SetImageLayout(temporaryCmdBuffer, shadowFrameBuf.depth.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureIn.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);
		vkQueueWaitIdle(queue.graphicQueue);
		////////////////////////////////////////////////////////////////////////////////////


		 res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}

		VkImageBlit imageBlit{};
		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.layerCount = 1;
		imageBlit.dstSubresource.mipLevel = 0;
		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.layerCount = 1;
		imageBlit.srcSubresource.mipLevel = 0;
		imageBlit.dstOffsets[0].x = 0;
		imageBlit.dstOffsets[0].y = 0;
		imageBlit.dstOffsets[0].z = 0;
		imageBlit.srcOffsets[0].x = 0;
		imageBlit.srcOffsets[0].y = 0;
		imageBlit.srcOffsets[0].z = 0;

		imageBlit.dstOffsets[1].x = 2048;
		imageBlit.dstOffsets[1].y = 2048;
		imageBlit.dstOffsets[1].z = 1;
		imageBlit.srcOffsets[1].x = 2048;
		imageBlit.srcOffsets[1].y = 2048;
		imageBlit.srcOffsets[1].z = 1;
		

		vkCmdBlitImage(
			temporaryCmdBuffer,
			shadowFrameBuf.depth.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			computeTextureIn.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageBlit,
			VK_FILTER_LINEAR);

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);
		vkQueueWaitIdle(queue.graphicQueue);
		//////////////////////////////////////////////////////////////////////////////////////
		 res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}

		vulkanHelper->SetImageLayout(temporaryCmdBuffer, shadowFrameBuf.depth.image, VK_IMAGE_ASPECT_COLOR_BIT,  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureIn.image, VK_IMAGE_ASPECT_COLOR_BIT,  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);
		vkQueueWaitIdle(queue.graphicQueue);

		/////////////////////////////////////////////////////////////////////////////////
		
		BuildComputeCommandBufferHorizontal();
		
		VkSubmitInfo computeSubmitInfo{};
		computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		computeSubmitInfo.commandBufferCount = 1;
		computeSubmitInfo.pCommandBuffers = &computeCmdBuffer;

		vkQueueSubmit(queue.computeQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE);

		vkQueueWaitIdle(queue.computeQueue);
		/////////////////////////////////////////////////////////////////////////////////////
		
		BuildComputeCommandBufferVertical();

		//VkSubmitInfo computeSubmitInfo{};
		//computeSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		//computeSubmitInfo.commandBufferCount = 1;
		//computeSubmitInfo.pCommandBuffers = &computeCmdBuffer;

		vkQueueSubmit(queue.computeQueue, 1, &computeSubmitInfo, VK_NULL_HANDLE);

		vkQueueWaitIdle(queue.computeQueue);
		
		////////////////////////////////////////////////////////////////////////////

		 res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}

		//vulkanHelper->SetImageLayout(temporaryCmdBuffer, shadowFrameBuf.depth.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
		//vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureOut.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureFinal.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		//vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureIn.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);
		vkQueueWaitIdle(queue.graphicQueue);
		////////////////////////////////////////////////////////////////////////////////////


		res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}

		


		vkCmdBlitImage(
			temporaryCmdBuffer,
			computeTextureFinal.image,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			shadowFrameBuf.depth.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&imageBlit,
			VK_FILTER_LINEAR);

		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);
		vkQueueWaitIdle(queue.graphicQueue);
		//////////////////////////////////////////////////////////////////////////////////////
		res = vkBeginCommandBuffer(temporaryCmdBuffer, &cmdBufInfo);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Begin Command Buffer");
		}

		vulkanHelper->SetImageLayout(temporaryCmdBuffer, shadowFrameBuf.depth.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		//vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureOut.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		//vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureOut.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		vulkanHelper->SetImageLayout(temporaryCmdBuffer, computeTextureFinal.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);
		vulkanDevice->FlushCommandBuffer(temporaryCmdBuffer, queue.graphicQueue, false);
		vkQueueWaitIdle(queue.graphicQueue);


		/////////////////////////////////////////////////////////////////////////////////////

		// Use a fence to wait until the command buffer has finished execution before using it again
		vkWaitForFences(vulkanDevice->logicalDevice, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX);
		vkResetFences(vulkanDevice->logicalDevice, 1, &waitFences[currentBuffer]);
		//
		submitInfo.pWaitSemaphores = &gBufferSemaphore;
		//
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
		//
		submitInfo.pSignalSemaphores = &semaphores.renderComplete;
		//
		// Submit to the graphics queue passing a wait fence
		vkQueueSubmit(queue.graphicQueue, 1, &submitInfo, waitFences[currentBuffer]);

		swapChain->QueuePresent(queue.graphicQueue, currentBuffer, semaphores.renderComplete);

		vkQueueWaitIdle(queue.graphicQueue);
	}
	virtual void Draw()
	{
		if (!prepared)
			return;
		DrawLoop();
	}

	virtual void GetEnabledDeviceFeatures()
	{
		if (vulkanDevice->physicalDeviceFeatures.samplerAnisotropy) {
			enabledFeatures.samplerAnisotropy = VK_TRUE;
		}
		if (vulkanDevice->physicalDeviceFeatures.geometryShader) {
			enabledFeatures.geometryShader = VK_TRUE;
		}

	}
	virtual void GuiUpdate(GUI * gui)
	{
		if (gui->Header("Scene Details"))
		{
			gui->Text("Model Count: %i", MAX_MODEL_COUNT);

		}
		if (gui->InputFloat("Bias", &perModelData.bias, 0.00000000001f, 10))
		{
			UpdateLightDataBuffers();
			BuildCommandBuffers();
		}
		if (gui->Header("Light Details"))
		{
			if (gui->CheckBox(" LightType", &lightData.lightType))
			{
				if (lightData.lightType == 0)
				{
					lightData.linear = 0.8f;
					lightData.quadratic = 0.7f;
				}
				else
				{
					lightData.constant = 1.0f;

					lightData.linear = 0.02f;
					lightData.quadratic = 0.07f;
				}
				UpdateLightDataBuffers();
				BuildCommandBuffers();
			}

			if (gui->SliderInt("Debug", &perModelData.debugValue, 0, 4))
			{
				UpdateLightDataBuffers();
				BuildCommandBuffers();
			}
			if (gui->InputFloat("Constant", &lightData.constant, 0.01f, 2))
			{
				UpdateLightDataBuffers();
				BuildCommandBuffers();
			}
			if (gui->InputFloat("Linear", &lightData.linear, 0.01f, 2))
			{
				UpdateLightDataBuffers();
				BuildCommandBuffers();
			}
			if (gui->InputFloat("Quadratic", &lightData.quadratic, 0.01f, 2))
			{
				UpdateLightDataBuffers();
				BuildCommandBuffers();
			}
			if (gui->InputFloat("Shininess", &lightData.shininess, 0.1f, 2))
			{
				UpdateLightDataBuffers();
				BuildCommandBuffers();
			}
			if (lightData.lightType == 1)
			{
				if (gui->InputFloat("CutOff", &lightData.cutOff, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Outer CutOff", &lightData.outerCutOff, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}

				if (gui->Header("Light Direction"))
				{
					if (gui->InputFloat("Direction in axis X", &lightData.direction.x, 0.01f, 2))
					{
						UpdateLightDataBuffers();
						BuildCommandBuffers();
					}
					if (gui->InputFloat("Direction in axis Y", &lightData.direction.y, 0.01f, 2))
					{
						UpdateLightDataBuffers();
						BuildCommandBuffers();
					}
					if (gui->InputFloat("Direction in axis Z", &lightData.direction.z, 0.01f, 2))
					{
						UpdateLightDataBuffers();
						BuildCommandBuffers();
					}

				}
			}


			if (gui->Header("Ambient Color"))
			{
				if (gui->InputFloat("Ambient Color X", &lightData.ambient.x, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Ambient Color Y", &lightData.ambient.y, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Ambient Color Z", &lightData.ambient.z, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Ambient Color W", &lightData.ambient.w, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
			}
			if (gui->Header("Diffuse Color"))
			{
				if (gui->InputFloat("Diffuse Color X", &lightData.diffuse.x, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Diffuse Color Y", &lightData.diffuse.y, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Diffuse Color Z", &lightData.diffuse.z, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Diffuse Color W", &lightData.diffuse.w, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
			}
			if (gui->Header("Specular Color"))
			{
				if (gui->InputFloat("Specular Color X", &lightData.specular.x, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Specular Color Y", &lightData.specular.y, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Specular Color Z", &lightData.specular.z, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
				if (gui->InputFloat("Specular Color W", &lightData.specular.w, 0.01f, 2))
				{
					UpdateLightDataBuffers();
					BuildCommandBuffers();
				}
			}



		}
	}
};

VULKAN_RENDERER()