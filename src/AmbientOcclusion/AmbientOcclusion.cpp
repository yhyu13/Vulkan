#include <vulkan/vulkan.h>
#include "VulkanRenderer.h"
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "VulkanModel.h"
#include <cstring>
#include <random>
/* Some physics constants */
#define DAMPING 0.01f // how much to damp the cloth simulation each frame
#define TIME_STEPSIZE2 0.5f*0.5f // how large time step each particle takes each frame
#define CONSTRAINT_ITERATIONS 15 // how many iterations of constraint satisfaction each frame (more is rigid, less is soft)

#define GAME_OBJECT_COUNT 2

struct Transform
{
	glm::vec3 positionPerAxis;
	glm::vec3 scalePerAxis;
	glm::vec3 rotateAngelPerAxis;
};
struct Material
{
	Texture2D albedoTexture;
};
struct GameObjectDetails
{
	glm::vec4 color;
	int textureTypeCount;
};
struct GameObject
{
	Model* model;
	Material* material;
	Transform transform;
	GameObjectDetails gameObjectDetails;
	int tag;
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




class UnitTest : public VulkanRenderer
{

public:
	//
	std::uniform_real_distribution<GLfloat> randomFloats; // generates random floats between 0.0 and 1.0
	std::default_random_engine generator;
	std::vector<glm::vec4> ssaoKernel;
	//
	Buffer vertBuffer;
	//
	Buffer lightBuffer;
	//
	Buffer kernalBuffer;
	//
	Buffer kernalNoiseBuffer;
	//
	Texture2D noiseTexture;
	//
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	//
	VkPipelineLayout pipelineLayout;
	//
	VkPipeline pipeline;
	//
	VkPipeline pipelineSSAOGeometry;
	//
	VkPipeline pipelineSSAOPass;
	//
	VkPipeline pipelineSSAOBlurPass;
	//
	std::vector<VkDescriptorSet> descriptorSets;
	//
	
	//
	VkSampler colorSamplerSSBOPass, colorSamplerSSBOBlurPass, colorSamplerPass1;
	// Struct for sending vertices to vertex shader
	VertexLayout vertexLayout = VertexLayout({
		VERTEX_COMPONENT_POSITION,
		VERTEX_COMPONENT_NORMAL,
		VERTEX_COMPONENT_UV
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
		glm::mat4 projMatrix;

	} uboVert;
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
		FrameBufferAttachment position, normal, albedo, ssboPass, ssboBlur;
		FrameBufferAttachment depth;
		VkRenderPass renderPass;
	} offScreenFrameBuf, ssboFrameBuf,ssboBlurFrameBuf;

	// Data send to the shader per model
	struct PerModelData
	{
		alignas(16) glm::mat4 modelMat;
		alignas(16) glm::vec4 color;
		alignas(16) glm::vec4 viewPosition;
	}perModelData;

	struct Light
	{
		alignas(16) glm::vec3 lightPos;
		alignas(16) glm::vec3 lightColor;
		alignas(16) float Linear;
		alignas(16) float Quadratic;
	}light;
	

	UnitTest() : VulkanRenderer()
	{
		appName = "Ambient Occlusion";

		light.lightColor = glm::vec3(1.0f);
		light.lightPos = glm::vec3(1.0f);
		light.Linear = 0.09f;
		light.Quadratic = 0.032f;
	}

	~UnitTest()
	{
		vkDestroyPipeline(vulkanDevice->logicalDevice, pipeline, nullptr);
		vkDestroyPipelineLayout(vulkanDevice->logicalDevice, pipelineLayout, nullptr);

		vkDestroyDescriptorSetLayout(vulkanDevice->logicalDevice, descriptorSetLayouts[0], nullptr);



		std::vector<AllModel>::iterator ptr;
		for (ptr = allModelList.begin(); ptr != allModelList.end(); ++ptr)
		{
			ptr->modelData.Destroy();
			ptr->materialData.albedoTexture.Destroy();
		}

		vertBuffer.Destroy();
	
	}

	void LoadScene()
	{
		float posX = 0.0f;
		float posY = 0.0f;
		float posZ = 0.0f;
		AllModel allModel;
		//Load Model Globe
		allModel.modelData.LoadModel("Animation2.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("arissaAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Block.obj", vertexLayout, 0.5f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("WoodSquare.jpg", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModelList.push_back(allModel);





		GameObject gameObj;
		gameObj.model = &(allModelList[0].modelData);
		gameObj.material = &(allModelList[0].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, 0.3f, 0.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(-90.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(0.000100f, 0.000100f, 0.000100f);
		scene.gameObjectList.emplace_back(gameObj);


		//Load shed model
		gameObj.model = &(allModelList[1].modelData);
		gameObj.material = &(allModelList[1].materialData);
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
		vertices.attributeDescriptions.resize(3);

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
		UpdateUniformBuffers();

		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&lightBuffer, sizeof(Light));
		// Map persistent
		lightBuffer.Map();
	}
	void UpdateLightBuffers()
	{
		memcpy(lightBuffer.mapped, &light, sizeof(Light));
	}
	void UpdateUniformBuffers()
	{
		glm::mat4 viewMatrix = camera->view;
		uboVert.viewMatrix = viewMatrix;
		uboVert.projViewMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f) * viewMatrix;
		uboVert.projMatrix = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f);
		memcpy(vertBuffer.mapped, &uboVert, sizeof(uboVert));
		glm::vec4 cameraPos = glm::vec4(camera->position, 1.0f);
		cameraPos.y = -cameraPos.y;
		perModelData.viewPosition = cameraPos;
	}
	void SetupDescriptorSetLayout()
	{
		VkDescriptorSetLayout descriptorSetLayout;

		VkDescriptorSetLayoutBinding uniformSetLayoutBinding{};
		uniformSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uniformSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		uniformSetLayoutBinding.binding = 0;
		uniformSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding albedoModelextureSamplerSetLayoutBinding{};
		albedoModelextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoModelextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		albedoModelextureSamplerSetLayoutBinding.binding = 1;
		albedoModelextureSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding positionTextureSamplerSetLayoutBinding{};
		positionTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		positionTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		positionTextureSamplerSetLayoutBinding.binding = 2;
		positionTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding normalTextureSamplerSetLayoutBinding{};
		normalTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		normalTextureSamplerSetLayoutBinding.binding = 3;
		normalTextureSamplerSetLayoutBinding.descriptorCount = 1;

		//
		VkDescriptorSetLayoutBinding albedoTextureSamplerSetLayoutBinding{};
		albedoTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		albedoTextureSamplerSetLayoutBinding.binding = 4;
		albedoTextureSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding ssboKernalSetLayoutBinding{};
		ssboKernalSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ssboKernalSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		ssboKernalSetLayoutBinding.binding = 5;
		ssboKernalSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding ssboBlurSetLayoutBinding{};
		ssboBlurSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ssboBlurSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		ssboBlurSetLayoutBinding.binding = 6;
		ssboBlurSetLayoutBinding.descriptorCount = 1;
		//
		VkDescriptorSetLayoutBinding ssboTextureSamplerSetLayoutBinding{};
		ssboTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ssboTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		ssboTextureSamplerSetLayoutBinding.binding = 7;
		ssboTextureSamplerSetLayoutBinding.descriptorCount = 1;
		//
		VkDescriptorSetLayoutBinding ssboBlurTextureSamplerSetLayoutBinding{};
		ssboBlurTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ssboBlurTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		ssboBlurTextureSamplerSetLayoutBinding.binding = 8;
		ssboBlurTextureSamplerSetLayoutBinding.descriptorCount = 1;
		//
		VkDescriptorSetLayoutBinding lightDataSamplerSetLayoutBinding{};
		lightDataSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		lightDataSamplerSetLayoutBinding.binding = 9;
		lightDataSamplerSetLayoutBinding.descriptorCount = 1;

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.push_back(uniformSetLayoutBinding);
		setLayoutBindings.push_back(albedoModelextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(positionTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(normalTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(albedoTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(ssboKernalSetLayoutBinding);
		setLayoutBindings.push_back(ssboBlurSetLayoutBinding);
		setLayoutBindings.push_back(ssboBlurTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(ssboTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(lightDataSamplerSetLayoutBinding);

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

		for (int i = 0; i < GAME_OBJECT_COUNT; ++i)
		{
			descriptorSetLayouts.emplace_back(descriptorSetLayout);
		}

		VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo{};
		pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pPipelineLayoutCreateInfo.setLayoutCount = GAME_OBJECT_COUNT;
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
		rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
		std::string vertAddress = fileName + "src\\AmbientOcclusion\\Shaders\\Default.vert.spv";
		std::string fragAddress = fileName + "src\\AmbientOcclusion\\Shaders\\Default.frag.spv";
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

		//VkResult res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline);
		/*if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}*/
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);
		
		//SSAOGeometry Shader

		vertAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOGeometry.vert.spv";
		 fragAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOGeometry.frag.spv";

		 vertShaderStage.module = resourceLoader->LoadShader(vertAddress);
		 fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		 shaderStages[0] = vertShaderStage;
		 shaderStages[1] = fragShaderStage;
		 pipelineCreateInfo.pStages = shaderStages.data();

		 std::array<VkPipelineColorBlendAttachmentState, 3> blendAttachmentStates;

		 VkPipelineColorBlendAttachmentState pipelineColorBlendAttachmentState{};
		 pipelineColorBlendAttachmentState.colorWriteMask = 0xf;
		 pipelineColorBlendAttachmentState.blendEnable = VK_FALSE;
		 blendAttachmentStates[0] = pipelineColorBlendAttachmentState;
		 blendAttachmentStates[1] = pipelineColorBlendAttachmentState;
		 blendAttachmentStates[2] = pipelineColorBlendAttachmentState;

		 colorBlendState.attachmentCount = static_cast<uint32_t>(blendAttachmentStates.size());
		 colorBlendState.pAttachments = blendAttachmentStates.data();
		 pipelineCreateInfo.pColorBlendState = &colorBlendState;

		 pipelineCreateInfo.layout = pipelineLayout;
		 pipelineCreateInfo.renderPass = offScreenFrameBuf.renderPass;

		 VkResult res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelineSSAOGeometry);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);


		///////////////
		VkPipelineVertexInputStateCreateInfo emptyInputState{};
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineCreateInfo.pVertexInputState = &emptyInputState;

		vertAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOPass.vert.spv";
		fragAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOPass.frag.spv";

		vertShaderStage.module = resourceLoader->LoadShader(vertAddress);
		fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		shaderStages[0] = vertShaderStage;
		shaderStages[1] = fragShaderStage;
		pipelineCreateInfo.pStages = shaderStages.data();

		

		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = blendAttachmentStates.data();
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = ssboFrameBuf.renderPass;

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelineSSAOPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);

		///////////////////////////
		//SSAO blur pass
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineCreateInfo.pVertexInputState = &emptyInputState;

		vertAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOPass.vert.spv";
		fragAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOBlurPass.frag.spv";

		vertShaderStage.module = resourceLoader->LoadShader(vertAddress);
		fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		shaderStages[0] = vertShaderStage;
		shaderStages[1] = fragShaderStage;
		pipelineCreateInfo.pStages = shaderStages.data();



		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = blendAttachmentStates.data();
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = ssboBlurFrameBuf.renderPass;

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelineSSAOBlurPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);

		///////////////////////////
		//SSAO final pass
		emptyInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		pipelineCreateInfo.pVertexInputState = &emptyInputState;

		vertAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOPass.vert.spv";
		fragAddress = fileName + "src\\AmbientOcclusion\\Shaders\\SSAOFinalPass.frag.spv";

		vertShaderStage.module = resourceLoader->LoadShader(vertAddress);
		fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		shaderStages[0] = vertShaderStage;
		shaderStages[1] = fragShaderStage;
		pipelineCreateInfo.pStages = shaderStages.data();



		colorBlendState.attachmentCount = 1;
		colorBlendState.pAttachments = blendAttachmentStates.data();
		pipelineCreateInfo.pColorBlendState = &colorBlendState;

		pipelineCreateInfo.layout = pipelineLayout;
		pipelineCreateInfo.renderPass = renderPass;

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline);
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
		uniformDescriptorPoolSize.descriptorCount = 2;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize albedoModelSamplerDescriptorPoolSize{};
		albedoModelSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoModelSamplerDescriptorPoolSize.descriptorCount = 2;


		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize positionSamplerDescriptorPoolSize{};
		positionSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		positionSamplerDescriptorPoolSize.descriptorCount = 2;


		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize normalSamplerDescriptorPoolSize{};
		normalSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalSamplerDescriptorPoolSize.descriptorCount = 2;


		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize albedoSamplerDescriptorPoolSize{};
		albedoSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSamplerDescriptorPoolSize.descriptorCount = 2;



		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize ssboKernalSamplerDescriptorPoolSize{};
		ssboKernalSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ssboKernalSamplerDescriptorPoolSize.descriptorCount = 2;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize ssboBlurSamplerDescriptorPoolSize{};
		ssboBlurSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ssboBlurSamplerDescriptorPoolSize.descriptorCount = 2;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize ssboBlurTextureSamplerDescriptorPoolSize{};
		ssboBlurTextureSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ssboBlurTextureSamplerDescriptorPoolSize.descriptorCount = 2;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize ssboTextureSamplerDescriptorPoolSize{};
		ssboTextureSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ssboTextureSamplerDescriptorPoolSize.descriptorCount = 2;


		//Descriptor pool for Light Buffer
		VkDescriptorPoolSize lightDataSamplerDescriptorPoolSize{};
		lightDataSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataSamplerDescriptorPoolSize.descriptorCount = 2;

		// Example uses tw0 ubo and one combined image sampler
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.push_back(uniformDescriptorPoolSize);
		poolSizes.push_back(albedoModelSamplerDescriptorPoolSize);

		poolSizes.push_back(positionSamplerDescriptorPoolSize);
		poolSizes.push_back(normalSamplerDescriptorPoolSize);
		poolSizes.push_back(albedoSamplerDescriptorPoolSize);
		poolSizes.push_back(ssboKernalSamplerDescriptorPoolSize);
		poolSizes.push_back(ssboBlurSamplerDescriptorPoolSize);
		poolSizes.push_back(ssboBlurTextureSamplerDescriptorPoolSize);
		poolSizes.push_back(ssboTextureSamplerDescriptorPoolSize);
		poolSizes.push_back(lightDataSamplerDescriptorPoolSize);

		// Create Descriptor pool info
		VkDescriptorPoolCreateInfo descriptorPoolInfo{};
		descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptorPoolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		descriptorPoolInfo.pPoolSizes = poolSizes.data();
		descriptorPoolInfo.maxSets = GAME_OBJECT_COUNT;



		VkResult res = vkCreateDescriptorPool(vulkanDevice->logicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Descriptor Pool");
		}
	}
	void SetupDescriptorSet()
	{
		descriptorSets.resize(GAME_OBJECT_COUNT);

		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.pSetLayouts = descriptorSetLayouts.data();
		allocInfo.descriptorSetCount = GAME_OBJECT_COUNT;
		VkResult res = vkAllocateDescriptorSets(vulkanDevice->logicalDevice, &allocInfo, descriptorSets.data());
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Allocate Descriptor Sets");
		}

		//  Image descriptors for the GBuffer color attachments
		VkDescriptorImageInfo positionDescriptorImageInfo{};
		positionDescriptorImageInfo.sampler = colorSamplerPass1;
		positionDescriptorImageInfo.imageView = offScreenFrameBuf.position.view;
		positionDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo normalDescriptorImageInfo{};
		normalDescriptorImageInfo.sampler = colorSamplerPass1;
		normalDescriptorImageInfo.imageView = offScreenFrameBuf.normal.view;
		normalDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo albedoDescriptorImageInfo{};
		albedoDescriptorImageInfo.sampler = colorSamplerPass1;
		albedoDescriptorImageInfo.imageView = offScreenFrameBuf.albedo.view;
		albedoDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo ssboDescriptorImageInfo{};
		ssboDescriptorImageInfo.sampler = colorSamplerSSBOPass;
		ssboDescriptorImageInfo.imageView = ssboFrameBuf.ssboPass.view;
		ssboDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkDescriptorImageInfo ssboBlurDescriptorImageInfo{};
		ssboBlurDescriptorImageInfo.sampler = colorSamplerSSBOBlurPass;
		ssboBlurDescriptorImageInfo.imageView = ssboBlurFrameBuf.ssboPass.view;
		ssboBlurDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		for (int i = 0; i < GAME_OBJECT_COUNT; ++i)
		{

			// Binding 0 : Vertex shader uniform buffer
			VkWriteDescriptorSet uniforBufferWriteDescriptorSet{};
			uniforBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			uniforBufferWriteDescriptorSet.dstSet = descriptorSets[i];
			uniforBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			uniforBufferWriteDescriptorSet.dstBinding = 0;
			uniforBufferWriteDescriptorSet.pBufferInfo = &(vertBuffer.descriptor);
			uniforBufferWriteDescriptorSet.descriptorCount = 1;

			//  Binding 1 : Albedo map 
			VkWriteDescriptorSet albedoModelSamplerWriteDescriptorSet{};
			albedoModelSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			albedoModelSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			albedoModelSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			albedoModelSamplerWriteDescriptorSet.dstBinding = 1;
			albedoModelSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.albedoTexture.descriptor);
			albedoModelSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 2 : position map 
			VkWriteDescriptorSet positionSamplerWriteDescriptorSet{};
			positionSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			positionSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			positionSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			positionSamplerWriteDescriptorSet.dstBinding = 2;
			positionSamplerWriteDescriptorSet.pImageInfo = &(positionDescriptorImageInfo);
			positionSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 3 : normal map 
			VkWriteDescriptorSet normalSamplerWriteDescriptorSet{};
			normalSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			normalSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			normalSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			normalSamplerWriteDescriptorSet.dstBinding = 3;
			normalSamplerWriteDescriptorSet.pImageInfo = &(normalDescriptorImageInfo);
			normalSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 4 : Albedo attachment map 
			VkWriteDescriptorSet albedoSamplerWriteDescriptorSet{};
			albedoSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			albedoSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			albedoSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			albedoSamplerWriteDescriptorSet.dstBinding = 4;
			albedoSamplerWriteDescriptorSet.pImageInfo = &(albedoDescriptorImageInfo);
			albedoSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 5 : ssboKernal Buffer 
			VkWriteDescriptorSet ssboKernalSamplerWriteDescriptorSet{};
			ssboKernalSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ssboKernalSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			ssboKernalSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ssboKernalSamplerWriteDescriptorSet.dstBinding =5;
			ssboKernalSamplerWriteDescriptorSet.pBufferInfo = &(kernalBuffer.descriptor);
			ssboKernalSamplerWriteDescriptorSet.descriptorCount = 1;

			
			//  Binding 6 : ssboNoiseBlur 
			VkWriteDescriptorSet ssboBlurSamplerWriteDescriptorSet{};
			ssboBlurSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ssboBlurSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			ssboBlurSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ssboBlurSamplerWriteDescriptorSet.dstBinding = 6;
			ssboBlurSamplerWriteDescriptorSet.pBufferInfo = &(kernalNoiseBuffer.descriptor);
			ssboBlurSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 7 : ssboBlurTexture
			VkWriteDescriptorSet ssboBlurTextureSamplerWriteDescriptorSet{};
			ssboBlurTextureSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ssboBlurTextureSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			ssboBlurTextureSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			ssboBlurTextureSamplerWriteDescriptorSet.dstBinding = 7;
			ssboBlurTextureSamplerWriteDescriptorSet.pImageInfo = &(ssboBlurDescriptorImageInfo);
			ssboBlurTextureSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 8 : ssboTexture
			VkWriteDescriptorSet ssboTextureSamplerWriteDescriptorSet{};
			ssboTextureSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ssboTextureSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			ssboTextureSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			ssboTextureSamplerWriteDescriptorSet.dstBinding = 8;
			ssboTextureSamplerWriteDescriptorSet.pImageInfo = &(ssboDescriptorImageInfo);
			ssboTextureSamplerWriteDescriptorSet.descriptorCount = 1;


			//  Binding 9 : light ddata
			VkWriteDescriptorSet lightDataSamplerWriteDescriptorSet{};
			lightDataSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			lightDataSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			lightDataSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			lightDataSamplerWriteDescriptorSet.dstBinding = 9;
			lightDataSamplerWriteDescriptorSet.pBufferInfo = &(lightBuffer.descriptor);
			lightDataSamplerWriteDescriptorSet.descriptorCount = 1;

			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(albedoModelSamplerWriteDescriptorSet);

			writeDescriptorSets.push_back(positionSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(normalSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(albedoSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(ssboKernalSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(ssboBlurSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(ssboBlurTextureSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(ssboTextureSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(lightDataSamplerWriteDescriptorSet);

			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
			writeDescriptorSets.clear();
		}




	}


	void DrawModels(VkCommandBuffer  drawCmdBufferCurrent)
	{


		for (int i = 0; i < GAME_OBJECT_COUNT; ++i)
		{

			vkCmdBindDescriptorSets(drawCmdBufferCurrent, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, NULL);
			UpdateModelBuffer(scene.gameObjectList[i], drawCmdBufferCurrent);
			VkDeviceSize offsets[1] = { 0 };
			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(drawCmdBufferCurrent, 0, 1, &scene.gameObjectList[i].model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(drawCmdBufferCurrent, scene.gameObjectList[i].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(drawCmdBufferCurrent, scene.gameObjectList[i].model->indexCount, 1, 0, 0, 0);

		}

	}
	

	void UpdateModelBuffer(GameObject _gameObject, VkCommandBuffer  drawCmdBufferCurrent)
	{
		perModelData.color = _gameObject.gameObjectDetails.color;
		glm::mat4 model = glm::mat4(1);
		
		model = glm::translate(model, glm::vec3(_gameObject.transform.positionPerAxis.x, _gameObject.transform.positionPerAxis.y, _gameObject.transform.positionPerAxis.z));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.z, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::scale(model, _gameObject.transform.scalePerAxis);
		
		perModelData.modelMat = model;
		vkCmdPushConstants(drawCmdBufferCurrent, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
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
			float tempHeight = float((-1.0f)*height);
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


			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
			VkDeviceSize offsets[1] = { 0 };
			vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[0], 0, NULL);
			vkCmdPushConstants(drawCmdBuffers[i], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
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
	void CreateAttachment(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment *attachment)
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
			aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
		}

		assert(aspectMask > 0);


		VkImageCreateInfo image{};
		image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image.imageType = VK_IMAGE_TYPE_2D;
		image.format = format;
		image.extent.width = offScreenFrameBuf.width;
		image.extent.height = offScreenFrameBuf.height;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
		image.samples = VK_SAMPLE_COUNT_1_BIT;
		image.tiling = VK_IMAGE_TILING_OPTIMAL;
		image.usage = usage | VK_IMAGE_USAGE_SAMPLED_BIT;

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
		imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageView.format = format;
		imageView.subresourceRange = {};
		imageView.subresourceRange.aspectMask = aspectMask;
		imageView.subresourceRange.baseMipLevel = 0;
		imageView.subresourceRange.levelCount = 1;
		imageView.subresourceRange.baseArrayLayer = 0;
		imageView.subresourceRange.layerCount = 1;
		imageView.image = attachment->image;
		res = vkCreateImageView(vulkanDevice->logicalDevice, &imageView, nullptr, &attachment->view);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image View");
		}
	}
	void PrepareGBuffer()
	{
		offScreenFrameBuf.width = width;
		offScreenFrameBuf.height = height;

		// (World space) Positions
		CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.position);

		// (World space) Normals
		CreateAttachment(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.normal);

		// Albedo (color)
		CreateAttachment(VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.albedo);

		
		// Depth Format
		CreateAttachment(depthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, &offScreenFrameBuf.depth);

		// Set up separate renderpass with references to the color and depth attachments
		std::array<VkAttachmentDescription, 4> attachmentDescs = {};

		// Init attachment properties
		for (uint32_t i = 0; i < 4; ++i)
		{
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			if (i == 3)
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
		attachmentDescs[3].format = offScreenFrameBuf.depth.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
		colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkAttachmentReference depthReference = {};
		depthReference.attachment = 3;
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

		std::array<VkImageView, 4> attachments;
		attachments[0] = offScreenFrameBuf.position.view;
		attachments[1] = offScreenFrameBuf.normal.view;
		attachments[2] = offScreenFrameBuf.albedo.view;
		attachments[3] = offScreenFrameBuf.depth.view;

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
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &colorSamplerPass1);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
	}
	void PrepareSSBOBuffer()
	{
		ssboFrameBuf.width = width;
		ssboFrameBuf.height = height;

		// (World space) Positions
		CreateAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &ssboFrameBuf.ssboPass);

		
		// Set up separate renderpass with references to the color and depth attachments
		std::array<VkAttachmentDescription, 1> attachmentDescs = {};

		// Init attachment properties
		for (uint32_t i = 0; i < 1; ++i)
		{
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			
				attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
				attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			
		}

		// Formats
		attachmentDescs[0].format = ssboFrameBuf.ssboPass.format;
		

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
	


		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount =  static_cast<uint32_t>(colorReferences.size());
		//subpass.pDepthStencilAttachment = &depthReference;

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

		VkResult res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &ssboFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 1> attachments;
		attachments[0] = ssboFrameBuf.ssboPass.view;


		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = ssboFrameBuf.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = ssboFrameBuf.width;
		fbufCreateInfo.height = ssboFrameBuf.height;
		fbufCreateInfo.layers = 1;
		res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &ssboFrameBuf.frameBuffer);
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
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &colorSamplerSSBOPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
	}
	void PrepareSSBOBlurBuffer()
	{
		ssboBlurFrameBuf.width = width;
		ssboBlurFrameBuf.height = height;

		// (World space) Positions
		CreateAttachment(VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &ssboBlurFrameBuf.ssboPass);


		// Set up separate renderpass with references to the color and depth attachments
		std::array<VkAttachmentDescription, 1> attachmentDescs = {};

		// Init attachment properties
		for (uint32_t i = 0; i < 1; ++i)
		{
			attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
			attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

			attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			attachmentDescs[i].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		}

		// Formats
		attachmentDescs[0].format = ssboBlurFrameBuf.ssboPass.format;


		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });



		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
		//subpass.pDepthStencilAttachment = &depthReference;

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

		VkResult res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &ssboBlurFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 1> attachments;
		attachments[0] = ssboBlurFrameBuf.ssboPass.view;


		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = ssboBlurFrameBuf.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = ssboBlurFrameBuf.width;
		fbufCreateInfo.height = ssboBlurFrameBuf.height;
		fbufCreateInfo.layers = 1;
		res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &ssboBlurFrameBuf.frameBuffer);
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
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &colorSamplerSSBOBlurPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
	}
	float lerp(float a, float b, float f)
	{
		return a + f * (b - a);
	}
	void GenerateKernal()
	{
		
		std::uniform_real_distribution<GLfloat> randomFloatsTemp(0.0, 1.0);
		randomFloats = randomFloatsTemp;
		for (unsigned int i = 0; i < 64; ++i)
		{
			glm::vec4 sample(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, randomFloats(generator),1.0);
			sample = glm::normalize(sample);
			sample *= randomFloats(generator);
			float scale = float(i) / 64.0;

			// scale samples s.t. they're more aligned to center of kernel
			scale = lerp(0.1f, 1.0f, scale * scale);
			sample *= scale;
			ssaoKernel.push_back(sample);
		}

		// Vertex shader uniform buffer block
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&kernalBuffer, sizeof(glm::vec4)*ssaoKernel.size());
		// Map persistent
		kernalBuffer.Map();
		memcpy(kernalBuffer.mapped, ssaoKernel.data(), sizeof(glm::vec4)*ssaoKernel.size());
		//kernalBuffer.UnMap();
	}
	void GenerateNoiseBuffer()
	{
		
		std::vector<glm::vec4> ssaoNoise;
		for (unsigned int i = 0; i < 16; i++)
		{
			glm::vec4 noise(randomFloats(generator) * 2.0 - 1.0, randomFloats(generator) * 2.0 - 1.0, 0.0f,1.0); // rotate around z-axis (in tangent space)
			ssaoNoise.push_back(noise);
		}
		////
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&kernalNoiseBuffer, sizeof(glm::vec4)*ssaoNoise.size());
		// Map persistent
		kernalNoiseBuffer.Map();
		memcpy(kernalNoiseBuffer.mapped, ssaoNoise.data(), sizeof(glm::vec4)*ssaoNoise.size());
		kernalNoiseBuffer.UnMap();


	}
	void BuildCommandBuffersSSAOGeometry()
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkClearValue clearValues[4];
		clearValues[0].color = defaultClearColor;
		clearValues[1].color = defaultClearColor;
		clearValues[2].color = defaultClearColor;
		clearValues[3].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = offScreenFrameBuf.renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 4;
		renderPassBeginInfo.pClearValues = clearValues;

		
			// Set target frame buffer
			renderPassBeginInfo.framebuffer = offScreenFrameBuf.frameBuffer;
			drawCmdBuffersSSAOGeometry = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
			
			vkCmdBeginRenderPass(drawCmdBuffersSSAOGeometry, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			VkViewport viewport{};
			viewport.width = (float)width;
			float tempHeight = float((-1.0f)*height);
			viewport.height = (float)height;
			//viewport.height = -(float)height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;
			//viewport.x = 0;
			//viewport.y = float(height);

			vkCmdSetViewport(drawCmdBuffersSSAOGeometry, 0, 1, &viewport);

			VkRect2D scissor{};
			scissor.extent.width = width;
			scissor.extent.height = height;
			scissor.offset.x = 0;
			scissor.offset.y = 0;
			vkCmdSetScissor(drawCmdBuffersSSAOGeometry, 0, 1, &scissor);


			vkCmdBindPipeline(drawCmdBuffersSSAOGeometry, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineSSAOGeometry);

			DrawModels(drawCmdBuffersSSAOGeometry);


			vkCmdEndRenderPass(drawCmdBuffersSSAOGeometry);

			VkResult res = vkEndCommandBuffer(drawCmdBuffersSSAOGeometry);
			if (res != VK_SUCCESS) {
				throw std::runtime_error("Error: End Command Buffer");
			}
		
	}
	void BuildCommandBuffersSSAOPass()
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkClearValue clearValues[1];
		clearValues[0].color = defaultClearColor;
	

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = ssboFrameBuf.renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;


		// Set target frame buffer
		renderPassBeginInfo.framebuffer = ssboFrameBuf.frameBuffer;
		drawCmdBuffersSSAOPass = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		vkCmdBeginRenderPass(drawCmdBuffersSSAOPass, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.width = (float)width;
		float tempHeight = float((-1.0f)*height);
		viewport.height = -(float)height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = float(height);

		vkCmdSetViewport(drawCmdBuffersSSAOPass, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(drawCmdBuffersSSAOPass, 0, 1, &scissor);


		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindDescriptorSets(drawCmdBuffersSSAOPass, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[0], 0, NULL);

		// Final composition as full screen quad
		vkCmdBindPipeline(drawCmdBuffersSSAOPass, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineSSAOPass);

		vkCmdDraw(drawCmdBuffersSSAOPass, 6, 1, 0, 0);

		vkCmdEndRenderPass(drawCmdBuffersSSAOPass);

		VkResult res = vkEndCommandBuffer(drawCmdBuffersSSAOPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: End Command Buffer");
		}

	}
	void BuildCommandBuffersSSAOBlurPass()
	{
		VkCommandBufferBeginInfo cmdBufInfo{};
		cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VkClearValue clearValues[1];
		clearValues[0].color = defaultClearColor;


		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassBeginInfo.renderPass = ssboBlurFrameBuf.renderPass;
		renderPassBeginInfo.renderArea.offset.x = 0;
		renderPassBeginInfo.renderArea.offset.y = 0;
		renderPassBeginInfo.renderArea.extent.width = width;
		renderPassBeginInfo.renderArea.extent.height = height;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;


		// Set target frame buffer
		renderPassBeginInfo.framebuffer = ssboBlurFrameBuf.frameBuffer;
		drawCmdBuffersSSAOBlurPass = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		vkCmdBeginRenderPass(drawCmdBuffersSSAOBlurPass, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport{};
		viewport.width = (float)width;
		float tempHeight = float((-1.0f)*height);
		viewport.height = -(float)height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = float(height);

		vkCmdSetViewport(drawCmdBuffersSSAOBlurPass, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent.width = width;
		scissor.extent.height = height;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(drawCmdBuffersSSAOBlurPass, 0, 1, &scissor);


		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindDescriptorSets(drawCmdBuffersSSAOBlurPass, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[0], 0, NULL);

		// Final composition as full screen quad
		vkCmdBindPipeline(drawCmdBuffersSSAOBlurPass, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineSSAOBlurPass);

		vkCmdDraw(drawCmdBuffersSSAOBlurPass, 6, 1, 0, 0);

		vkCmdEndRenderPass(drawCmdBuffersSSAOBlurPass);

		VkResult res = vkEndCommandBuffer(drawCmdBuffersSSAOBlurPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: End Command Buffer");
		}

	}
	void Prepare()
	{
		VulkanRenderer::Prepare();
		LoadScene();
		PrepareGBuffer();
		PrepareSSBOBuffer();
		PrepareSSBOBlurBuffer();
		GenerateKernal();
		GenerateNoiseBuffer();
		SetupVertexDescriptions();
		PrepareUniformBuffers();
		SetupDescriptorSetLayout();
		PreparePipelines();
		SetupDescriptorPool();
		SetupDescriptorSet();
		BuildCommandBuffersSSAOGeometry();
		BuildCommandBuffersSSAOPass();
		BuildCommandBuffersSSAOBlurPass();
		BuildCommandBuffers();

		prepared = true;

	}
	virtual void Update()
	{
		UpdateUniformBuffers();
		UpdateLightBuffers();

	}
	void DrawLoopSSAOGeometry()
	{
		// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		//
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
		//submitInfo.pWaitSemaphores = &semaphores.presentComplete;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
		submitInfo.waitSemaphoreCount = 0;												// One wait semaphore																				
		//submitInfo.pSignalSemaphores = &semaphores.renderComplete;						// Semaphore(s) to be signaled when command buffers have completed
		submitInfo.signalSemaphoreCount = 0;											// One signal semaphore
		submitInfo.pCommandBuffers = &drawCmdBuffersSSAOGeometry;					// Command buffers(s) to execute in this batch (submission)
		submitInfo.commandBufferCount = 1;												// One command buffer

		// Submit to the graphics queue passing a wait fence
		vkQueueSubmit(queue.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue.graphicQueue);
	}
	void DrawLoopSSAOPass()
	{
		// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		//
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
		//submitInfo.pWaitSemaphores = &semaphores.presentComplete;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
		submitInfo.waitSemaphoreCount = 0;												// One wait semaphore																				
		//submitInfo.pSignalSemaphores = &semaphores.renderComplete;						// Semaphore(s) to be signaled when command buffers have completed
		submitInfo.signalSemaphoreCount = 0;											// One signal semaphore
		submitInfo.pCommandBuffers = &drawCmdBuffersSSAOPass;					// Command buffers(s) to execute in this batch (submission)
		submitInfo.commandBufferCount = 1;												// One command buffer

		// Submit to the graphics queue passing a wait fence
		vkQueueSubmit(queue.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue.graphicQueue);
	}
	void DrawLoopSSAOBlurPass()
	{
		// Pipeline stage at which the queue submission will wait (via pWaitSemaphores)
		VkPipelineStageFlags waitStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		//
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.pWaitDstStageMask = &waitStageMask;									// Pointer to the list of pipeline stages that the semaphore waits will occur at
		//submitInfo.pWaitSemaphores = &semaphores.presentComplete;							// Semaphore(s) to wait upon before the submitted command buffer starts executing
		submitInfo.waitSemaphoreCount = 0;												// One wait semaphore																				
		//submitInfo.pSignalSemaphores = &semaphores.renderComplete;						// Semaphore(s) to be signaled when command buffers have completed
		submitInfo.signalSemaphoreCount = 0;											// One signal semaphore
		submitInfo.pCommandBuffers = &drawCmdBuffersSSAOBlurPass;					// Command buffers(s) to execute in this batch (submission)
		submitInfo.commandBufferCount = 1;												// One command buffer

		// Submit to the graphics queue passing a wait fence
		vkQueueSubmit(queue.graphicQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(queue.graphicQueue);
	}
	void DrawLoop()
	{
		//
		DrawLoopSSAOGeometry();
		DrawLoopSSAOPass();
		DrawLoopSSAOBlurPass();
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
			gui->Text("Model Count: %i", GAME_OBJECT_COUNT);
		}
		if (gui->Header("Light Data"))
		{
			if (gui->Header("Light Color"))
			{
				if (gui->InputFloat("Light Color X", &light.lightColor.x, 0.1f, 2))
				{

				}
				if (gui->InputFloat("Light Color Y", &light.lightColor.y, 0.1f, 2))
				{

				}
				if (gui->InputFloat("Light Color Z", &light.lightColor.z, 0.1f, 2))
				{

				}
			}
			if (gui->Header("Light Position"))
			{
				if (gui->InputFloat("Light Position X", &light.lightPos.x, 0.1f, 2))
				{

				}
				if (gui->InputFloat("Light Position Y", &light.lightPos.y, 0.1f, 2))
				{

				}
				if (gui->InputFloat("Light Position Z", &light.lightPos.z, 0.1f, 2))
				{

				}
			}
			if (gui->Header("Light Attenuation"))
			{
				if (gui->InputFloat("Light Linear", &light.Linear, 0.001f, 5))
				{

				}
				if (gui->InputFloat("Light Quadratic", &light.Quadratic, 0.001f, 5))
				{

				}
				
			}
		}



	}
};

VULKAN_RENDERER()