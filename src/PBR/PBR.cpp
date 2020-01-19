#include <vulkan/vulkan.h>
#include "VulkanRenderer.h"
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



struct LightInstanceDetails
{
	alignas(16) glm::vec3 position;

	alignas(16) glm::vec3 color;
};
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
	Texture2D heightTexture;
	Texture2D aoTexture;
	Texture2D metalTexture;
	Texture2D roughTexture;
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
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	//
	VkPipelineLayout pipelineLayout;
	//
	VkPipeline modelPipeline;
	//
	std::vector<VkDescriptorSet> descriptorSets;
	//
	LightData lightData;
	// Struct for sending vertices to vertex shader
	VertexLayout vertexLayout = VertexLayout({
		VERTEX_COMPONENT_POSITION,
		VERTEX_COMPONENT_NORMAL,
		VERTEX_COMPONENT_UV,
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
		glm::mat4 viewMatrix;
		glm::vec3 viewPos;
	} uboVert;

	// Data send to the shader per model
	struct PerModelData
	{
		alignas(16) glm::mat4 modelMat;
		alignas(16) glm::vec4 color;
		alignas(16) int textureCount;
	}perModelData;


	UnitTest() : VulkanRenderer()
	{
		appName = "ForwardRendering(Lights)";
		lightData.lightType = 0;
		lightData.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
		lightData.diffuse = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
		lightData.specular = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		lightData.constant = 1.0f;
		lightData.linear = 0.04f;
		lightData.quadratic = 0.08f;
		lightData.shininess = 32.0f;
		lightData.direction = glm::vec3(0.0f, -1.0f, 0.0f);
		lightData.cutOff = 0.93f;
		lightData.outerCutOff = 0.89f;
		srand(time(NULL));
	}

	~UnitTest()
	{
		vkDestroyPipeline(vulkanDevice->logicalDevice, modelPipeline, nullptr);
		vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);

		vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayouts[0], nullptr);

		std::vector<AllModel>::iterator ptr;
		for (ptr = allModelList.begin(); ptr != allModelList.end(); ++ptr)
		{
			ptr->modelData.Destroy();
			if (ptr->materialData.albedoTexture.image)
			{
				ptr->materialData.albedoTexture.Destroy();
			}


		}

		vertBuffer.Destroy();
		instanceLightBuffer.Destroy();
		lightDetailsBuffer.Destroy();
	}

	void LoadScene()
	{
		float posX = 0.0f;
		float posY = 0.5f;
		float posZ = 0.0f;
		AllModel allModel;
		//Load Model Globe
		allModel.modelData.LoadModel("Guitar.obj", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("GuitarAlbedo.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("GuitarNormal.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("GuitarAlbedo.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("GuitarMetal.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("GuitarRough.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.textureCount = 1;
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Box.obj", vertexLayout, 0.05f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("LightColor.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("RustNormal.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("RustMetal.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("RustRough.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.textureCount = 1;
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Block.obj", vertexLayout, 0.5f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("PanelAlbedo.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("PanelNormal.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("PanelHeight.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("PanelAO.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("PanelMetalness.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("PanelRoughness.png", VK_FORMAT_B8G8R8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		
		allModel.materialData.textureCount = 1;
		allModelList.push_back(allModel);



		for (int i = 0; i < MAX_MODEL_COUNT; ++i)
		{
			GameObject gameObj;
			gameObj.model = &(allModelList[0].modelData);
			gameObj.material = &(allModelList[0].materialData);
			gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

			gameObj.transform.positionPerAxis = glm::vec3(posX, posY, posZ);
			gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(0.0f));
			gameObj.transform.scalePerAxis = glm::vec3(0.01f, 0.01f, 0.01f);

			scene.gameObjectList.emplace_back(gameObj);
			posZ += 2.0f;
			if (posZ > 20.0f)
			{
				posX += 2.0f;
				posZ = 0.0f;
			}
		}

		posX = 0.0f;
		posY = 2.0f;
		posZ = -2.0f;
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
			lightDetails.position = glm::vec3(posX, posY-0.25, posZ);
			lightDetails.color = glm::vec3(1.0f, 1.0f, 1.0f);

			//lightDetails.color = glm::vec3(static_cast <float> (rand()) / static_cast <float> (RAND_MAX), static_cast <float> (rand()) / static_cast <float> (RAND_MAX),
		//		static_cast <float> (rand()) / static_cast <float> (RAND_MAX));
			allLightsList.emplace_back(lightDetails);

			posZ += 2.0f;
			if (posZ > 20.0f)
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

		gameObj.transform.positionPerAxis = glm::vec3(0.3f, -0.055f, 0.30f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(0.0f, glm::radians(00.0f), 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(50.0f, 5.0f, 40.0f);

		scene.gameObjectList.emplace_back(gameObj);

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
		vertices.attributeDescriptions.resize(4);

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

		// Location 3 : Tangent coordinates
		VkVertexInputAttributeDescription tangentVertexInputAttribDescription{};
		tangentVertexInputAttribDescription.location = 3;
		tangentVertexInputAttribDescription.binding = 0;
		tangentVertexInputAttribDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
		tangentVertexInputAttribDescription.offset = sizeof(float) * 8;

		vertices.attributeDescriptions[3] = tangentVertexInputAttribDescription;

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
		// Map persistent
		lightDetailsBuffer.Map();
		//
		UpdateLightDataBuffers();
	}
	void UpdateUniformBuffers()
	{
		glm::mat4 viewMatrix = camera->view;
		uboVert.viewMatrix = viewMatrix;
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
		uniformSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uniformSetLayoutBinding.binding = 0;
		uniformSetLayoutBinding.descriptorCount = 1;


		VkDescriptorSetLayoutBinding albedoSamplerSetLayoutBinding{};
		albedoSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		albedoSamplerSetLayoutBinding.binding = 1;
		albedoSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding normalSamplerSetLayoutBinding{};
		normalSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		normalSamplerSetLayoutBinding.binding = 2;
		normalSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding heightSamplerSetLayoutBinding{};
		heightSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		heightSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		heightSamplerSetLayoutBinding.binding = 3;
		heightSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding aoSamplerSetLayoutBinding{};
		aoSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		aoSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		aoSamplerSetLayoutBinding.binding = 4;
		aoSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding metalSamplerSetLayoutBinding{};
		metalSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		metalSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		metalSamplerSetLayoutBinding.binding = 5;
		metalSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding roughSamplerSetLayoutBinding{};
		roughSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		roughSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		roughSamplerSetLayoutBinding.binding = 6;
		roughSamplerSetLayoutBinding.descriptorCount = 1;


		VkDescriptorSetLayoutBinding lightDataSetLayoutBinding{};
		lightDataSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		lightDataSetLayoutBinding.binding = 7;
		lightDataSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding instanceLightsSetLayoutBinding{};
		instanceLightsSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instanceLightsSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		instanceLightsSetLayoutBinding.binding = 8;
		instanceLightsSetLayoutBinding.descriptorCount = 1;



		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.push_back(uniformSetLayoutBinding);
		setLayoutBindings.push_back(albedoSamplerSetLayoutBinding);
		setLayoutBindings.push_back(normalSamplerSetLayoutBinding);
		setLayoutBindings.push_back(heightSamplerSetLayoutBinding);
		setLayoutBindings.push_back(aoSamplerSetLayoutBinding);
		setLayoutBindings.push_back(metalSamplerSetLayoutBinding);
		setLayoutBindings.push_back(roughSamplerSetLayoutBinding);
		setLayoutBindings.push_back(lightDataSetLayoutBinding);
		setLayoutBindings.push_back(instanceLightsSetLayoutBinding);

		//Push constant for model data
		VkPushConstantRange pushConstantRange{};
		pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		pushConstantRange.offset = 0;
		pushConstantRange.size = sizeof(perModelData);


		VkDescriptorSetLayoutCreateInfo descriptorLayout{};
		descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		descriptorLayout.pBindings = setLayoutBindings.data();
		descriptorLayout.bindingCount = static_cast<uint32_t>(setLayoutBindings.size());

		VkResult res = vkCreateDescriptorSetLayout(vulkanDevice->logicalDevice, &descriptorLayout, nullptr, &descriptorSetLayout);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Descriptor Set Layout");
		}
		descriptorSetLayouts.emplace_back(descriptorSetLayout);
		descriptorSetLayouts.emplace_back(descriptorSetLayout);
		descriptorSetLayouts.emplace_back(descriptorSetLayout);
		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo{};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.setLayoutCount = 3;
		pPipelineLayoutCreateInfo.pSetLayouts = descriptorSetLayouts.data();

		// Push constant ranges are part of the pipeline layout
		pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
		pPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

		res = vkCreatePipelineLayout(vulkanDevice->logicalDevice, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout);
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
		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = &blendAttachmentState;

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


		// Solid rendering pipeline
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
		std::string vertAddress = fileName + "src\\PBR\\Shaders\\Lights.vert.spv";
		std::string fragAddress = fileName + "src\\PBR\\Shaders\\Lights.frag.spv";
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
		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderPass;
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

		VkResult res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &modelPipeline);
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
		uniformDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize imgSamplerDescriptorPoolSize{};
		imgSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		imgSamplerDescriptorPoolSize.descriptorCount = 10;

		// Example uses tw0 ubo and one combined image sampler
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.push_back(uniformDescriptorPoolSize);
		poolSizes.push_back(imgSamplerDescriptorPoolSize);

		// Create Descriptor pool info
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = 3;

		VkResult res = vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Descriptor Pool");
		}
	}
	void SetupDescriptorSet()
	{
		//
		VkDescriptorSet model1DescriptorSet;
		//
		VkDescriptorSet model2DescriptorSet;
		//
		VkDescriptorSet model3DescriptorSet;
		//
		descriptorSets.emplace_back(model1DescriptorSet);
		//
		descriptorSets.push_back(model2DescriptorSet);
		//
		descriptorSets.push_back(model3DescriptorSet);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.pSetLayouts = descriptorSetLayouts.data();
		allocInfo.descriptorSetCount = 3;
		VkResult res = vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, descriptorSets.data());
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Allocate Descriptor Sets");
		}


		// Binding 0 : Vertex shader uniform buffer
		VkWriteDescriptorSet uniforBufferWriteDescriptorSet1{};
		uniforBufferWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniforBufferWriteDescriptorSet1.dstSet = descriptorSets[0];
		uniforBufferWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniforBufferWriteDescriptorSet1.dstBinding = 0;
		uniforBufferWriteDescriptorSet1.pBufferInfo = &(vertBuffer.descriptor);
		uniforBufferWriteDescriptorSet1.descriptorCount = 1;

		//  Binding 1 : Albedo map Model 1
		VkWriteDescriptorSet albedoSamplerWriteDescriptorSet1{};
		albedoSamplerWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		albedoSamplerWriteDescriptorSet1.dstSet = descriptorSets[0];
		albedoSamplerWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSamplerWriteDescriptorSet1.dstBinding = 1;
		albedoSamplerWriteDescriptorSet1.pImageInfo = &(allModelList[0].materialData.albedoTexture.descriptor);
		albedoSamplerWriteDescriptorSet1.descriptorCount = 1;

		//  Binding 2 : Normal map Model 1
		VkWriteDescriptorSet normalSamplerWriteDescriptorSet1{};
		normalSamplerWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normalSamplerWriteDescriptorSet1.dstSet = descriptorSets[0];
		normalSamplerWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalSamplerWriteDescriptorSet1.dstBinding = 2;
		normalSamplerWriteDescriptorSet1.pImageInfo = &(allModelList[0].materialData.normalTexture.descriptor);
		normalSamplerWriteDescriptorSet1.descriptorCount = 1;

		//  Binding 3 : Height map Model 1
		VkWriteDescriptorSet heightSamplerWriteDescriptorSet1{};
		heightSamplerWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		heightSamplerWriteDescriptorSet1.dstSet = descriptorSets[0];
		heightSamplerWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		heightSamplerWriteDescriptorSet1.dstBinding = 3;
		heightSamplerWriteDescriptorSet1.pImageInfo = &(allModelList[0].materialData.heightTexture.descriptor);
		heightSamplerWriteDescriptorSet1.descriptorCount = 1;

		//  Binding 4 : AO map Model 1
		VkWriteDescriptorSet aoSamplerWriteDescriptorSet1{};
		aoSamplerWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		aoSamplerWriteDescriptorSet1.dstSet = descriptorSets[0];
		aoSamplerWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		aoSamplerWriteDescriptorSet1.dstBinding = 4;
		aoSamplerWriteDescriptorSet1.pImageInfo = &(allModelList[0].materialData.aoTexture.descriptor);
		aoSamplerWriteDescriptorSet1.descriptorCount = 1;

		//  Binding 5 : Metal map Model 1
		VkWriteDescriptorSet metalSamplerWriteDescriptorSet1{};
		metalSamplerWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		metalSamplerWriteDescriptorSet1.dstSet = descriptorSets[0];
		metalSamplerWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		metalSamplerWriteDescriptorSet1.dstBinding = 5;
		metalSamplerWriteDescriptorSet1.pImageInfo = &(allModelList[0].materialData.metalTexture.descriptor);
		metalSamplerWriteDescriptorSet1.descriptorCount = 1;

		//  Binding 6 : Rough map Model 1
		VkWriteDescriptorSet roughSamplerWriteDescriptorSet1{};
		roughSamplerWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		roughSamplerWriteDescriptorSet1.dstSet = descriptorSets[0];
		roughSamplerWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		roughSamplerWriteDescriptorSet1.dstBinding = 6;
		roughSamplerWriteDescriptorSet1.pImageInfo = &(allModelList[0].materialData.roughTexture.descriptor);
		roughSamplerWriteDescriptorSet1.descriptorCount = 1;


		// Binding 7 : Light data buffer
		VkWriteDescriptorSet lightDataBufferWriteDescriptorSet1{};
		lightDataBufferWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightDataBufferWriteDescriptorSet1.dstSet = descriptorSets[0];
		lightDataBufferWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataBufferWriteDescriptorSet1.dstBinding = 7;
		lightDataBufferWriteDescriptorSet1.pBufferInfo = &(lightDetailsBuffer.descriptor);
		lightDataBufferWriteDescriptorSet1.descriptorCount = 1;

		// Binding 8 : Light Instance
		VkWriteDescriptorSet instancLightBufferWriteDescriptorSet1{};
		instancLightBufferWriteDescriptorSet1.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		instancLightBufferWriteDescriptorSet1.dstSet = descriptorSets[0];
		instancLightBufferWriteDescriptorSet1.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instancLightBufferWriteDescriptorSet1.dstBinding = 8;
		instancLightBufferWriteDescriptorSet1.pBufferInfo = &(instanceLightBuffer.descriptor);
		instancLightBufferWriteDescriptorSet1.descriptorCount = 1;

		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet1);
		writeDescriptorSets.push_back(albedoSamplerWriteDescriptorSet1);
		writeDescriptorSets.push_back(normalSamplerWriteDescriptorSet1);
		writeDescriptorSets.push_back(heightSamplerWriteDescriptorSet1);
		writeDescriptorSets.push_back(aoSamplerWriteDescriptorSet1);
		writeDescriptorSets.push_back(metalSamplerWriteDescriptorSet1);
		writeDescriptorSets.push_back(roughSamplerWriteDescriptorSet1);
		writeDescriptorSets.push_back(lightDataBufferWriteDescriptorSet1);
		writeDescriptorSets.push_back(instancLightBufferWriteDescriptorSet1);

		vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		writeDescriptorSets.clear();

		// Binding 0 : Vertex shader uniform buffer
		VkWriteDescriptorSet uniforBufferWriteDescriptorSet2{};
		uniforBufferWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniforBufferWriteDescriptorSet2.dstSet = descriptorSets[1];
		uniforBufferWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniforBufferWriteDescriptorSet2.dstBinding = 0;
		uniforBufferWriteDescriptorSet2.pBufferInfo = &(vertBuffer.descriptor);
		uniforBufferWriteDescriptorSet2.descriptorCount = 1;

		//  Binding 1 : Albedo map Model 1
		VkWriteDescriptorSet albedoSamplerWriteDescriptorSet2{};
		albedoSamplerWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		albedoSamplerWriteDescriptorSet2.dstSet = descriptorSets[1];
		albedoSamplerWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSamplerWriteDescriptorSet2.dstBinding = 1;
		albedoSamplerWriteDescriptorSet2.pImageInfo = &(allModelList[1].materialData.albedoTexture.descriptor);
		albedoSamplerWriteDescriptorSet2.descriptorCount = 1;

		//  Binding 2 : Normal map Model 1
		VkWriteDescriptorSet normalSamplerWriteDescriptorSet2{};
		normalSamplerWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normalSamplerWriteDescriptorSet2.dstSet = descriptorSets[1];
		normalSamplerWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalSamplerWriteDescriptorSet2.dstBinding = 2;
		normalSamplerWriteDescriptorSet2.pImageInfo = &(allModelList[1].materialData.normalTexture.descriptor);
		normalSamplerWriteDescriptorSet2.descriptorCount = 1;

		//  Binding 3 : Height map Model 1
		VkWriteDescriptorSet heightSamplerWriteDescriptorSet2{};
		heightSamplerWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		heightSamplerWriteDescriptorSet2.dstSet = descriptorSets[1];
		heightSamplerWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		heightSamplerWriteDescriptorSet2.dstBinding = 3;
		heightSamplerWriteDescriptorSet2.pImageInfo = &(allModelList[1].materialData.heightTexture.descriptor);
		heightSamplerWriteDescriptorSet2.descriptorCount = 1;

		//  Binding 4 : AO map Model 1
		VkWriteDescriptorSet aoSamplerWriteDescriptorSet2{};
		aoSamplerWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		aoSamplerWriteDescriptorSet2.dstSet = descriptorSets[1];
		aoSamplerWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		aoSamplerWriteDescriptorSet2.dstBinding = 4;
		aoSamplerWriteDescriptorSet2.pImageInfo = &(allModelList[1].materialData.aoTexture.descriptor);
		aoSamplerWriteDescriptorSet2.descriptorCount = 1;

		//  Binding 5 : Metal map Model 1
		VkWriteDescriptorSet metalSamplerWriteDescriptorSet2{};
		metalSamplerWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		metalSamplerWriteDescriptorSet2.dstSet = descriptorSets[1];
		metalSamplerWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		metalSamplerWriteDescriptorSet2.dstBinding = 5;
		metalSamplerWriteDescriptorSet2.pImageInfo = &(allModelList[1].materialData.metalTexture.descriptor);
		metalSamplerWriteDescriptorSet2.descriptorCount = 1;

		//  Binding 6 : Rough map Model 1
		VkWriteDescriptorSet roughSamplerWriteDescriptorSet2{};
		roughSamplerWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		roughSamplerWriteDescriptorSet2.dstSet = descriptorSets[1];
		roughSamplerWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		roughSamplerWriteDescriptorSet2.dstBinding = 6;
		roughSamplerWriteDescriptorSet2.pImageInfo = &(allModelList[1].materialData.roughTexture.descriptor);
		roughSamplerWriteDescriptorSet2.descriptorCount = 1;

		// Binding 2 : Light data buffer
		VkWriteDescriptorSet lightDataBufferWriteDescriptorSet2{};
		lightDataBufferWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightDataBufferWriteDescriptorSet2.dstSet = descriptorSets[1];
		lightDataBufferWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataBufferWriteDescriptorSet2.dstBinding = 7;
		lightDataBufferWriteDescriptorSet2.pBufferInfo = &(lightDetailsBuffer.descriptor);
		lightDataBufferWriteDescriptorSet2.descriptorCount = 1;

		// Binding 3 : Light Instance
		VkWriteDescriptorSet instancLightBufferWriteDescriptorSet2{};
		instancLightBufferWriteDescriptorSet2.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		instancLightBufferWriteDescriptorSet2.dstSet = descriptorSets[1];
		instancLightBufferWriteDescriptorSet2.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instancLightBufferWriteDescriptorSet2.dstBinding = 8;
		instancLightBufferWriteDescriptorSet2.pBufferInfo = &(instanceLightBuffer.descriptor);
		instancLightBufferWriteDescriptorSet2.descriptorCount = 1;


		writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet2);
		writeDescriptorSets.push_back(albedoSamplerWriteDescriptorSet2);
		writeDescriptorSets.push_back(normalSamplerWriteDescriptorSet2);
		writeDescriptorSets.push_back(heightSamplerWriteDescriptorSet2);
		writeDescriptorSets.push_back(aoSamplerWriteDescriptorSet2);
		writeDescriptorSets.push_back(metalSamplerWriteDescriptorSet2);
		writeDescriptorSets.push_back(roughSamplerWriteDescriptorSet2);
		writeDescriptorSets.push_back(lightDataBufferWriteDescriptorSet2);
		writeDescriptorSets.push_back(instancLightBufferWriteDescriptorSet2);

		vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

		writeDescriptorSets.clear();

		// Binding 0 : Vertex shader uniform buffer
		VkWriteDescriptorSet uniforBufferWriteDescriptorSet3{};
		uniforBufferWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		uniforBufferWriteDescriptorSet3.dstSet = descriptorSets[2];
		uniforBufferWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniforBufferWriteDescriptorSet3.dstBinding = 0;
		uniforBufferWriteDescriptorSet3.pBufferInfo = &(vertBuffer.descriptor);
		uniforBufferWriteDescriptorSet3.descriptorCount = 1;

		//  Binding 1 : Albedo map Model 1
		VkWriteDescriptorSet albedoSamplerWriteDescriptorSet3{};
		albedoSamplerWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		albedoSamplerWriteDescriptorSet3.dstSet = descriptorSets[2];
		albedoSamplerWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSamplerWriteDescriptorSet3.dstBinding = 1;
		albedoSamplerWriteDescriptorSet3.pImageInfo = &(allModelList[2].materialData.albedoTexture.descriptor);
		albedoSamplerWriteDescriptorSet3.descriptorCount = 1;

		//  Binding 2 : Normal map Model 1
		VkWriteDescriptorSet normalSamplerWriteDescriptorSet3{};
		normalSamplerWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normalSamplerWriteDescriptorSet3.dstSet = descriptorSets[2];
		normalSamplerWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalSamplerWriteDescriptorSet3.dstBinding = 2;
		normalSamplerWriteDescriptorSet3.pImageInfo = &(allModelList[2].materialData.normalTexture.descriptor);
		normalSamplerWriteDescriptorSet3.descriptorCount = 1;

		//  Binding 3 : Height map Model 1
		VkWriteDescriptorSet heightSamplerWriteDescriptorSet3{};
		heightSamplerWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		heightSamplerWriteDescriptorSet3.dstSet = descriptorSets[2];
		heightSamplerWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		heightSamplerWriteDescriptorSet3.dstBinding = 3;
		heightSamplerWriteDescriptorSet3.pImageInfo = &(allModelList[2].materialData.heightTexture.descriptor);
		heightSamplerWriteDescriptorSet3.descriptorCount = 1;

		//  Binding 4 : AO map Model 1
		VkWriteDescriptorSet aoSamplerWriteDescriptorSet3{};
		aoSamplerWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		aoSamplerWriteDescriptorSet3.dstSet = descriptorSets[2];
		aoSamplerWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		aoSamplerWriteDescriptorSet3.dstBinding = 4;
		aoSamplerWriteDescriptorSet3.pImageInfo = &(allModelList[2].materialData.aoTexture.descriptor);
		aoSamplerWriteDescriptorSet3.descriptorCount = 1;

		//  Binding 5 : Metal map Model 1
		VkWriteDescriptorSet metalSamplerWriteDescriptorSet3{};
		metalSamplerWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		metalSamplerWriteDescriptorSet3.dstSet = descriptorSets[2];
		metalSamplerWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		metalSamplerWriteDescriptorSet3.dstBinding = 5;
		metalSamplerWriteDescriptorSet3.pImageInfo = &(allModelList[2].materialData.metalTexture.descriptor);
		metalSamplerWriteDescriptorSet3.descriptorCount = 1;

		//  Binding 6 : Rough map Model 1
		VkWriteDescriptorSet roughSamplerWriteDescriptorSet3{};
		roughSamplerWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		roughSamplerWriteDescriptorSet3.dstSet = descriptorSets[2];
		roughSamplerWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		roughSamplerWriteDescriptorSet3.dstBinding = 6;
		roughSamplerWriteDescriptorSet3.pImageInfo = &(allModelList[2].materialData.roughTexture.descriptor);
		roughSamplerWriteDescriptorSet3.descriptorCount = 1;

		// Binding 2 : Light data buffer
		VkWriteDescriptorSet lightDataBufferWriteDescriptorSet3{};
		lightDataBufferWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightDataBufferWriteDescriptorSet3.dstSet = descriptorSets[2];
		lightDataBufferWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataBufferWriteDescriptorSet3.dstBinding = 7;
		lightDataBufferWriteDescriptorSet3.pBufferInfo = &(lightDetailsBuffer.descriptor);
		lightDataBufferWriteDescriptorSet3.descriptorCount = 1;

		// Binding 3 : Light Instance
		VkWriteDescriptorSet instancLightBufferWriteDescriptorSet3{};
		instancLightBufferWriteDescriptorSet3.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		instancLightBufferWriteDescriptorSet3.dstSet = descriptorSets[2];
		instancLightBufferWriteDescriptorSet3.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instancLightBufferWriteDescriptorSet3.dstBinding = 8;
		instancLightBufferWriteDescriptorSet3.pBufferInfo = &(instanceLightBuffer.descriptor);
		instancLightBufferWriteDescriptorSet3.descriptorCount = 1;


		writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet3);
		writeDescriptorSets.push_back(albedoSamplerWriteDescriptorSet3);
		writeDescriptorSets.push_back(normalSamplerWriteDescriptorSet3);
		writeDescriptorSets.push_back(heightSamplerWriteDescriptorSet3);
		writeDescriptorSets.push_back(aoSamplerWriteDescriptorSet3);
		writeDescriptorSets.push_back(metalSamplerWriteDescriptorSet3);
		writeDescriptorSets.push_back(roughSamplerWriteDescriptorSet3);
		writeDescriptorSets.push_back(lightDataBufferWriteDescriptorSet3);
		writeDescriptorSets.push_back(instancLightBufferWriteDescriptorSet3);

		vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
	}

	void DrawModels(int commandBufferCount)
	{


		//Draw all models 

		for (int i = 0; i < MAX_MODEL_COUNT; ++i)
		{

			vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferCount], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[0], 0, NULL);


			UpdateModelBuffer(scene.gameObjectList[i], commandBufferCount);
			VkDeviceSize offsets[1] = { 0 };

			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(drawCmdBuffers[commandBufferCount], 0, 1, &scene.gameObjectList[i].model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(drawCmdBuffers[commandBufferCount], scene.gameObjectList[i].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(drawCmdBuffers[commandBufferCount], scene.gameObjectList[i].model->indexCount, 1, 0, 0, 0);

		}
		for (int i = MAX_MODEL_COUNT; i < MAX_LIGHT_COUNT + MAX_MODEL_COUNT; ++i)
		{

			vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferCount], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[1], 0, NULL);


			UpdateModelBuffer(scene.gameObjectList[i], commandBufferCount);
			VkDeviceSize offsets[1] = { 0 };

			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(drawCmdBuffers[commandBufferCount], 0, 1, &scene.gameObjectList[i].model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(drawCmdBuffers[commandBufferCount], scene.gameObjectList[i].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(drawCmdBuffers[commandBufferCount], scene.gameObjectList[i].model->indexCount, 1, 0, 0, 0);

		}
		// Draw Shed model
		vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferCount], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[2], 0, NULL);


		UpdateModelBuffer(scene.gameObjectList[scene.gameObjectList.size() - 1], commandBufferCount);
		VkDeviceSize offsets[1] = { 0 };

		// Bind mesh vertex buffer
		vkCmdBindVertexBuffers(drawCmdBuffers[commandBufferCount], 0, 1, &scene.gameObjectList[scene.gameObjectList.size() - 1].model->vertices.buffer, offsets);
		// Bind mesh index buffer
		vkCmdBindIndexBuffer(drawCmdBuffers[commandBufferCount], scene.gameObjectList[scene.gameObjectList.size() - 1].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
		// Render mesh vertex buffer using it's indices
		vkCmdDrawIndexed(drawCmdBuffers[commandBufferCount], scene.gameObjectList[scene.gameObjectList.size() - 1].model->indexCount, 1, 0, 0, 0);

	}

	void UpdateModelBuffer(GameObject _gameObject, int commandBufferCount)
	{
		perModelData.color = _gameObject.gameObjectDetails.color;
		glm::mat4 model = glm::mat4(1);
		
		
		model = glm::translate(model, glm::vec3(_gameObject.transform.positionPerAxis.x, _gameObject.transform.positionPerAxis.y, _gameObject.transform.positionPerAxis.z));
		
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.z, glm::vec3(0.0f, 0.0f, 1.0f));
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.y, glm::vec3(0.0f, 1.0f, 0.0f));
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, _gameObject.transform.scalePerAxis);
		
		perModelData.modelMat = model;
		vkCmdPushConstants(drawCmdBuffers[commandBufferCount], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
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
			viewport.height = -(float)height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			viewport.x = 0;
			viewport.y = float(height);


			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);


			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, modelPipeline);

			DrawModels(i);

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
	void Prepare()
	{
		VulkanRenderer::Prepare();
		LoadScene();
		SetupVertexDescriptions();
		PrepareUniformBuffers();
		SetupDescriptorSetLayout();
		PreparePipelines();
		SetupDescriptorPool();
		SetupDescriptorSet();

		BuildCommandBuffers();
		prepared = true;

	}
	virtual void Update()
	{
		uboVert.viewPos = camera->position;
		uboVert.viewPos = -uboVert.viewPos;
		UpdateUniformBuffers();

	}
	void DrawLoop()
	{

		// Get next image in the swap chain (back/front buffer)
		swapChain->AcquireNextImage(semaphores.presentComplete, &currentBuffer);

		// Use a fence to wait until the command buffer has finished execution before using it again
		vkWaitForFences(vulkanDevice->logicalDevice, 1, &waitFences[currentBuffer], VK_TRUE, UINT64_MAX);
		vkResetFences(vulkanDevice->logicalDevice, 1, &waitFences[currentBuffer]);

		// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		// The submit info structure specifices a command buffer queue submission batch
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
		submitInfo.pWaitSemaphores = &semaphores.presentComplete;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
		submitInfo.waitSemaphoreCount = 1;												// One wait semaphore																				
		submitInfo.pSignalSemaphores = &semaphores.renderComplete;						// Semaphore(s) to be signaled when command buffers have completed
		submitInfo.signalSemaphoreCount = 1;											// One signal semaphore
		submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];					// Command buffers(s) to execute in this batch (submission)
		submitInfo.commandBufferCount = 1;												// One command buffer

		// Submit to the graphics queue passing a wait fence
		vkQueueSubmit(queue.graphicQueue, 1, &submitInfo, waitFences[currentBuffer]);

		// Present the current buffer to the swap chain
		// Pass the semaphore signaled by the command buffer submission from the submit info as the wait semaphore for swap chain presentation
		// This ensures that the image is not presented to the windowing system until all commands have been submitted
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
	}
	virtual void GuiUpdate(GUI * gui)
	{
		if (gui->Header("Scene Details"))
		{
			gui->Text("Model Count: %i", MAX_MODEL_COUNT);

		}
		if (gui->Header("Light Details"))
		{
			if (gui->CheckBox(" LightType", &lightData.lightType))
			{
				if (lightData.lightType == 0)
				{
					lightData.linear = 1.34f;
					lightData.quadratic = 1.68f;
				}
				else
				{
					lightData.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f);
					lightData.linear = 0.06f;
					lightData.quadratic = 0.1f;
				}
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
			if (gui->InputFloat("Shininess", &lightData.shininess, 0.01f, 2))
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