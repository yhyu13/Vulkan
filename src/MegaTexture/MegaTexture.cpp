#include <vulkan/vulkan.h>
#include "VulkanRenderer.h"
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "VulkanModel.h"
#include <cstring>


#define GAME_OBJECT_COUNT 16
#define FEED_BACK_SIZE 64*64
#define PAGE_TILE_SIZE 128

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
	int index;
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
	Buffer vertBuffer;
	//
	Buffer feedBackBuffer;
	//
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	//
	VkPipelineLayout pipelineLayout;
	//
	VkPipeline pipeline;
	//
	SparseTexture sparseTexture;
	//
	std::vector<VkDescriptorSet> descriptorSets;

	//
	glm::vec3 ballPos = glm::vec3(0.7f, -0.8f, 0.0f);
	//
	float ballradius = 0.20f;
	//
	int FrameTimer =0;
	//
	int pageDeleteTime = 20;
	//
	int perFrameBindPage = 10;

	///
	bool checkBoxDebugDraw = false;
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

	} uboVert;

	struct PagesActive {
		int miplevel;
		int pageNumber;
		int deviceMemoryLocation;
		VkDeviceMemory memory;

	} pagesActive;
	struct Pages{
		UINT miplevel;
		UINT pageNumber;
		UINT pageAlive;
		UINT dummy;

	} pages;
	

	std::vector<Pages> feedBackBuffer_Vec;
	std::deque<Pages> requestPageQueue;
	std::unordered_map<int, std::unordered_map<int, int>> requestPageCheck;
	std::vector<PagesActive> activePageQueue;
	std::unordered_map<int, std::unordered_map<int, int>> activePageCheck;
	std::unordered_map<int, std::unordered_map<int, int>> timerPageCheck;
	// Data send to the shader per model
	struct PerModelData
	{
		alignas(16) glm::mat4 modelMat;
		alignas(16) glm::vec4 color;
		alignas(16) int modelIndex;
		alignas(16) int debugDraw;
	}perModelData;

	
	UnitTest() : VulkanRenderer()
	{
		appName = "Mega Texture";
		perModelData.debugDraw = 0;
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
		feedBackBuffer.Destroy();
	}

	void LoadScene()
	{
		//Load Megatexture
		sparseTexture.LoadFromFile("temp.txt", VK_FORMAT_R8G8B8A8_UNORM, 16384, 16384, 1, 1024, vulkanDevice, vulkanHelper, queue.graphicQueue);

		float posX = 0.0f;
		float posY = 0.0f;
		float posZ = 0.0f;
		AllModel allModel;
		//Load Model Globe
		allModel.modelData.LoadModel("Sphere.OBJ", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("SofaAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		
		
		
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);
		allModelList.push_back(allModel);

		AllModel allModelCube;
		allModelCube.modelData.LoadModel("Crate.OBJ", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModelCube.materialData.albedoTexture.LoadFromFile("SofaAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModelList.push_back(allModelCube);
		allModelList.push_back(allModelCube);
		allModelList.push_back(allModelCube);
		allModelList.push_back(allModelCube);
		allModelList.push_back(allModelCube);
		allModelList.push_back(allModelCube);


		AllModel allModelTerrain;
		allModelTerrain.modelData.LoadModel("terrain.obj", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModelTerrain.materialData.albedoTexture.LoadFromFile("SofaAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModelList.push_back(allModelTerrain);

		GameObject gameObj;
		
		

		//AllModel allModelCube;
		////Load Model Globe
	
		//
		////
		//allModelList.push_back(allModelCube);
		gameObj.model = &(allModelList[9].modelData);
		gameObj.material = &(allModelList[9].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, 0.0, 600.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(600.00f, 600.00f, 1.00f);
		gameObj.index = 8;
		scene.gameObjectList.emplace_back(gameObj);

		//
		gameObj.model = &(allModelList[10].modelData);
		gameObj.material = &(allModelList[10].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, 600.0, 0.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), glm::radians(90.0f), 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(600.00f, 1.00f, 600.00f);
		gameObj.index = 9;
		scene.gameObjectList.emplace_back(gameObj);
		
		//
		gameObj.model = &(allModelList[11].modelData);
		gameObj.material = &(allModelList[11].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, 0.0, -600.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(600.00f, 600.00f, 1.00f);
		gameObj.index = 10;
		scene.gameObjectList.emplace_back(gameObj);

		//
		gameObj.model = &(allModelList[12].modelData);
		gameObj.material = &(allModelList[12].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(-600.0f, 0.0, 0.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(1.00f, 600.00f, 600.00f);
		gameObj.index = 11;
		scene.gameObjectList.emplace_back(gameObj);

		//
		gameObj.model = &(allModelList[13].modelData);
		gameObj.material = &(allModelList[13].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(600.0f, 0.0, 0.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(1.00f, 600.00f, 600.00f);
		gameObj.index = 12;
		scene.gameObjectList.emplace_back(gameObj);

		//
		gameObj.model = &(allModelList[14].modelData);
		gameObj.material = &(allModelList[14].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, -600.0, 0.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), glm::radians(90.0f), 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(600.00f, 1.00f, 600.00f);
		gameObj.index = 13;
		scene.gameObjectList.emplace_back(gameObj);

		//planets
		gameObj.model = &(allModelList[0].modelData);
		gameObj.material = &(allModelList[0].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, 0.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 0;
		scene.gameObjectList.emplace_back(gameObj);

		//
		//allModel.modelData.LoadModel("Sphere.OBJ", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		//allModel.materialData.albedoTexture.LoadFromFile("SofaAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		gameObj.model = &(allModelList[1].modelData);
		gameObj.material = &(allModelList[1].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(100.0f, 0.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 1;
		scene.gameObjectList.emplace_back(gameObj);

		//

		gameObj.model = &(allModelList[2].modelData);
		gameObj.material = &(allModelList[2].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(200.0f, 0.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 2;
		scene.gameObjectList.emplace_back(gameObj);

		//

		gameObj.model = &(allModelList[3].modelData);
		gameObj.material = &(allModelList[3].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(300.0f, 0.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 3;
		scene.gameObjectList.emplace_back(gameObj);

		//

		gameObj.model = &(allModelList[4].modelData);
		gameObj.material = &(allModelList[4].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(-100.0f, 0.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 4;
		scene.gameObjectList.emplace_back(gameObj);


		//

		gameObj.model = &(allModelList[5].modelData);
		gameObj.material = &(allModelList[5].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(-200.0f, 0.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 5;
		scene.gameObjectList.emplace_back(gameObj);

		//

		gameObj.model = &(allModelList[6].modelData);
		gameObj.material = &(allModelList[6].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(-300.0f, 0.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 6;
		scene.gameObjectList.emplace_back(gameObj);


		//

		gameObj.model = &(allModelList[7].modelData);
		gameObj.material = &(allModelList[7].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, 200.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 7;
		scene.gameObjectList.emplace_back(gameObj);


		gameObj.model = &(allModelList[8].modelData);
		gameObj.material = &(allModelList[8].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, -200.0, -130.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(30.00f, 30.00f, 30.00f);
		gameObj.index = 14;
		scene.gameObjectList.emplace_back(gameObj);

		//terrain model
		gameObj.model = &(allModelList[15].modelData);
		gameObj.material = &(allModelList[15].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(0.0f, -10.0, 0.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(00.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(-10.00f, 10.00f, 10.00f);
		gameObj.index = 15;
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
	}
	void PrepareFeedBackBuffers()
	{
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&feedBackBuffer, sizeof(Pages)*FEED_BACK_SIZE);
		// Map persistent
		feedBackBuffer.Map();
	}
	void UpdateUniformBuffers()
	{
		glm::mat4 viewMatrix = camera->view;
		uboVert.projViewMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 10000.0f) * viewMatrix;
		memcpy(vertBuffer.mapped, &uboVert, sizeof(uboVert));
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

		VkDescriptorSetLayoutBinding sparseTextureSamplerSetLayoutBinding{};
		sparseTextureSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sparseTextureSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sparseTextureSamplerSetLayoutBinding.binding = 2;
		sparseTextureSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding feedBackSamplerSetLayoutBinding{};
		feedBackSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		feedBackSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		feedBackSamplerSetLayoutBinding.binding = 3;
		feedBackSamplerSetLayoutBinding.descriptorCount = 1;

		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.push_back(uniformSetLayoutBinding);
		setLayoutBindings.push_back(albedoModelextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(sparseTextureSamplerSetLayoutBinding);
		setLayoutBindings.push_back(feedBackSamplerSetLayoutBinding);

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
		pPipelineLayoutCreateInfo.setLayoutCount = descriptorSetLayouts.size();
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
		std::string vertAddress = fileName + "src\\MegaTexture\\Shaders\\MegaTexture.vert.spv";
		std::string fragAddress = fileName + "src\\MegaTexture\\Shaders\\MegaTexture.frag.spv";
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

		VkResult res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipeline);
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

		//Descriptor pool for Sparse Texture
		VkDescriptorPoolSize sparseTextureSamplerDescriptorPoolSize{};
		sparseTextureSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sparseTextureSamplerDescriptorPoolSize.descriptorCount = 2;
		
		//Descriptor pool for FeedBack Buffer
		VkDescriptorPoolSize feedBackSamplerDescriptorPoolSize{};
		feedBackSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		feedBackSamplerDescriptorPoolSize.descriptorCount = 3;

		

		// Example uses tw0 ubo and one combined image sampler
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.push_back(uniformDescriptorPoolSize);
		poolSizes.push_back(albedoModelSamplerDescriptorPoolSize);
		poolSizes.push_back(sparseTextureSamplerDescriptorPoolSize);
		poolSizes.push_back(feedBackSamplerDescriptorPoolSize);

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

			//  Binding 1 : Albedo map Model 1
			VkWriteDescriptorSet albedoModelSamplerWriteDescriptorSet{};
			albedoModelSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			albedoModelSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			albedoModelSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			albedoModelSamplerWriteDescriptorSet.dstBinding = 1;
			albedoModelSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.albedoTexture.descriptor);
			albedoModelSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 2 : sparse Texture
			VkWriteDescriptorSet sparseTextureSamplerWriteDescriptorSet{};
			sparseTextureSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			sparseTextureSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			sparseTextureSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			sparseTextureSamplerWriteDescriptorSet.dstBinding = 2;
			sparseTextureSamplerWriteDescriptorSet.pImageInfo = &(sparseTexture.descriptor);
			sparseTextureSamplerWriteDescriptorSet.descriptorCount = 1;


			//  Binding 3 : FeedBack Buffer
			VkWriteDescriptorSet feedBackSamplerWriteDescriptorSet{};
			feedBackSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			feedBackSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			feedBackSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			feedBackSamplerWriteDescriptorSet.dstBinding = 3;
			feedBackSamplerWriteDescriptorSet.pBufferInfo = &(feedBackBuffer.descriptor);
			feedBackSamplerWriteDescriptorSet.descriptorCount = 1;

			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(albedoModelSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(sparseTextureSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(feedBackSamplerWriteDescriptorSet);

			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
			writeDescriptorSets.clear();
		}




	}

	void DrawModels(int commandBufferCount)
	{


		for (int i = 0; i < GAME_OBJECT_COUNT; ++i)
		{

			vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferCount], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[i], 0, NULL);
			UpdateModelBuffer(scene.gameObjectList[i], commandBufferCount);
			VkDeviceSize offsets[1] = { 0 };
			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(drawCmdBuffers[commandBufferCount], 0, 1, &scene.gameObjectList[i].model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(drawCmdBuffers[commandBufferCount], scene.gameObjectList[i].model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(drawCmdBuffers[commandBufferCount], scene.gameObjectList[i].model->indexCount, 1, 0, 0, 0);

		}

	}

	void UpdateModelBuffer(GameObject _gameObject, int commandBufferCount)
	{
		perModelData.color = _gameObject.gameObjectDetails.color;
		glm::mat4 model = glm::mat4(1);
		model = glm::translate(model, glm::vec3(_gameObject.transform.positionPerAxis.x, _gameObject.transform.positionPerAxis.y, _gameObject.transform.positionPerAxis.z));
		
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.z, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.x, glm::vec3(1.0f, 0.0f, 0.0f));
		
		model = glm::scale(model, _gameObject.transform.scalePerAxis);
		perModelData.modelMat = model; 
		perModelData.modelIndex = _gameObject.index;
		if (checkBoxDebugDraw)
		{
			perModelData.debugDraw = 1;
		}
		else
		{
			perModelData.debugDraw = 0;
		}
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
		PrepareFeedBackBuffers();
		SetupDescriptorSetLayout();
		PreparePipelines();
		SetupDescriptorPool();
		SetupDescriptorSet();
		BuildCommandBuffers();
		prepared = true;

	}
	//todo
	bool CheckPageValid(Pages page, int xOffset, int yOffset)
	{
		if (page.miplevel < 0)
		{
			return false;
		}

		else if (page.miplevel >= 8)
		{
			return false;
		}

		if (xOffset < 0)
		{
			return false;
		}
		else if (xOffset >= sqrt(sparseTexture.mipAndPages[page.miplevel]))
		{
			return false;
		}

		if (yOffset < 0)
		{
			return false;
		}
		else if (yOffset >= sqrt(sparseTexture.mipAndPages[page.miplevel]))
		{
			return false;
		}

		return true;
	}
	void AddRequestAndParents(Pages page)
	{
		// todo
		int loopCount = 8 - page.miplevel;
		int pageCountWidth = pow(2,(sparseTexture.mipLevels-1 - page.miplevel)) / PAGE_TILE_SIZE;

		int pageCountY = page.pageNumber / pageCountWidth;
		int pageCountX = page.pageNumber % pageCountWidth;//page.pageNumber - (pageCountY * pageCountWidth);


		

		for (int i = 0; i < loopCount; ++i)
		{
			int xpos = pageCountX >> i;
			int ypos = pageCountY >> i;
			int mipLevel = page.miplevel;
			mipLevel += i;
			//todo
			if (mipLevel > 6)
			{
				break;
			}
			int pageWidth = pow(2, (sparseTexture.mipLevels - 1 - mipLevel)) / PAGE_TILE_SIZE;
			
			if (!CheckPageValid(page, xpos, ypos))
			{
				return;
			}
			
			int pageNumber = ypos * pageWidth + xpos;

			if (pageNumber > sparseTexture.mipAndPages[mipLevel] )
			{
				int k = 0;
			}
			if ( timerPageCheck[mipLevel][pageNumber] > 0)
			{
				timerPageCheck[mipLevel][pageNumber] = FrameTimer;
				continue;
			}
			if (requestPageCheck[mipLevel][pageNumber] == 1)
			{
				continue;
			}
			if (activePageCheck[mipLevel][pageNumber] == 1)
			{
				continue;
			}
			if (requestPageCheck[mipLevel][pageNumber] != 1)
			{
				requestPageCheck[mipLevel][pageNumber] = 1;
				Pages page;
				page.miplevel = mipLevel;
				page.pageNumber = pageNumber;
				requestPageQueue.emplace_back(page);
			}

			
		}
	}
	void ReadFeedBackBuffer()
	{

		feedBackBuffer_Vec.clear();
		feedBackBuffer_Vec.resize(FEED_BACK_SIZE);
		memcpy(feedBackBuffer_Vec.data(), feedBackBuffer.mapped, FEED_BACK_SIZE * sizeof(Pages));
		
		std::unordered_map<int, std::unordered_map<int,int>> feedBackReadCheck_Vec;
		for (int i = 0; i < feedBackBuffer_Vec.size(); ++i)
		{
			//todo
			if (feedBackBuffer_Vec[i].pageAlive == 0 || feedBackBuffer_Vec[i].pageAlive>6)
			{
				continue;
			}
			if (feedBackReadCheck_Vec[feedBackBuffer_Vec[i].miplevel][feedBackBuffer_Vec[i].pageNumber] == 1)
			{
				continue;
			}
			else
			{
				feedBackReadCheck_Vec[feedBackBuffer_Vec[i].miplevel][feedBackBuffer_Vec[i].pageNumber] = 1;
			}
			/*if (activePageCheck[feedBackBuffer_Vec[i].miplevel][feedBackBuffer_Vec[i].pageNumber] == 1)
			{
				timerPageCheck[feedBackBuffer_Vec[i].miplevel][feedBackBuffer_Vec[i].pageNumber] = FrameTimer;
				continue;
			}*/

			
			AddRequestAndParents(feedBackBuffer_Vec[i]);
		}

	}
	void BindPages()
	{
		int totalPagesBinded = 0;
		int totalPagesUnBinded = 0;
		std::deque <Pages> ::iterator it;
		std::vector<VkSparseImageMemoryBind> currentImageBind;
		std::vector < VkBufferImageCopy> imageCopy;
		for (it = requestPageQueue.begin(); it != requestPageQueue.end(); ++it)
		{
			if ((*it).miplevel != 1)
			{
				//continue;
			}
			if (sparseTexture.memoryDeviceList.size() == 0)
			{
				break;
			}
			if (totalPagesBinded  >= perFrameBindPage)
			{
				break;
			}
			if (activePageCheck[(*it).miplevel][(*it).pageNumber] == 1)
			{
				//timerPageCheck[(*it).miplevel][(*it).pageNumber] = FrameTimer;
				continue;
			}
			
			activePageCheck[(*it).miplevel][(*it).pageNumber] = 1;
			PagesActive pageActive;
			pageActive.miplevel = (*it).miplevel;
			pageActive.pageNumber = (*it).pageNumber;
			pageActive.deviceMemoryLocation = sparseTexture.memoryCount[0];
			
			pageActive.memory = sparseTexture.memoryDeviceList[0];
			activePageQueue.emplace_back(pageActive);
			//
			sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber].memory = pageActive.memory;
			currentImageBind.push_back(sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber]);
			//
			VkImageSubresourceLayers  subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.mipLevel = pageActive.miplevel;
			subresourceRange.layerCount = 1;
			subresourceRange.baseArrayLayer = 0;
			//
			VkBufferImageCopy imageCopyInst;
			imageCopyInst.bufferOffset = pageActive.deviceMemoryLocation * 65536;
			imageCopyInst.bufferRowLength = 0;
			imageCopyInst.bufferImageHeight = 0;
			imageCopyInst.imageOffset = sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber].offset;
			imageCopyInst.imageExtent = sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber].extent;
			imageCopyInst.imageSubresource = subresourceRange;
			imageCopy.push_back(imageCopyInst);
			//todo
			//fread
			char * stagingBufferLocation = (char *)sparseTexture.stagingBuffer.mapped;
			char * tempSB = stagingBufferLocation + pageActive.deviceMemoryLocation * 65536;
			int currentReadLocation = 65536 * sparseTexture.readByteOffset[pageActive.miplevel] + pageActive.pageNumber * 65536;
			//fseek(sparseTexture.textureFile, 65536*1000, SEEK_SET);
			fseek(sparseTexture.textureFile, currentReadLocation , SEEK_SET);
			int result = fread((void*)tempSB,   sizeof(char), 65536,sparseTexture.textureFile);
			
			if (!result )
			{
				throw std::runtime_error("Error: fread");
			}
			//
			sparseTexture.memoryCount.pop_front();
			sparseTexture.memoryDeviceList.pop_front();
			totalPagesBinded += 1;
			timerPageCheck[(*it).miplevel][(*it).pageNumber] = FrameTimer;
			requestPageCheck[(*it).miplevel][(*it).pageNumber] = 0;
			
		}
		for (int i = 0; i < totalPagesBinded; ++i)
		{
			requestPageQueue.pop_front();
		}
		std::vector <PagesActive> ::iterator itr;
		std::vector <PagesActive> deleteActivePage;
		for (itr = activePageQueue.begin(); itr != activePageQueue.end(); ++itr)
		{
			
			if (FrameTimer - timerPageCheck[(*itr).miplevel][(*itr).pageNumber] > pageDeleteTime)
			{
				timerPageCheck[(*itr).miplevel][(*itr).pageNumber] = 0;
				activePageCheck[(*itr).miplevel][(*itr).pageNumber] = 0;
				sparseTexture.memoryCount.emplace_back((*itr).deviceMemoryLocation);
				sparseTexture.memoryDeviceList.emplace_back((*itr).memory);
				totalPagesUnBinded += 1;
				sparseTexture.imageBinds[(*itr).miplevel][(*itr).pageNumber].memory = VK_NULL_HANDLE;
				currentImageBind.push_back(sparseTexture.imageBinds[(*itr).miplevel][(*itr).pageNumber]);
				//if (itr != activePageQueue.begin()) {
				//	

				//	activePageQueue.erase(itr--);
				//}
				//else
				//{
				//	//activePageQueue.erase(itr);
				//	//activePageQueue.erase(itr);
				//	//itr = activePageQueue.begin();
				//}
				
			}
			else
			{
				deleteActivePage.push_back(*itr);
			}
		
		}
		activePageQueue.clear();
		activePageQueue = deleteActivePage;
		if (currentImageBind.size() > 0)
		{
			VkSparseImageMemoryBindInfo imageBindInfo;					// Sparse image memory bind info 
			VkSparseImageOpaqueMemoryBindInfo mipTailBindInfo;

			VkBindSparseInfo bindSparseInfo{};
			bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
			// Image memory binds
			imageBindInfo.image = sparseTexture.image;
			imageBindInfo.bindCount = static_cast<uint32_t>(currentImageBind.size());
			imageBindInfo.pBinds = currentImageBind.data();
			bindSparseInfo.imageBindCount = (imageBindInfo.bindCount > 0) ? 1 : 0;
			bindSparseInfo.pImageBinds = &imageBindInfo;

			// Opaque image memory binds (mip tail)
			mipTailBindInfo.image = sparseTexture.image;
			mipTailBindInfo.bindCount = 0;// static_cast<uint32_t>(opaqueMemoryBinds.size());
			//mipTailBindInfo.pBinds = &sparseTexture.mipTailBind;
			bindSparseInfo.imageOpaqueBindCount = (mipTailBindInfo.bindCount > 0) ? 1 : 0;
			bindSparseInfo.pImageOpaqueBinds = &mipTailBindInfo;
			
			vkQueueBindSparse(queue.graphicQueue, 1, &bindSparseInfo, VK_NULL_HANDLE);
			//todo: use sparse bind semaphore
			vkQueueWaitIdle(queue.graphicQueue);

			if (imageCopy.size() > 0)
			{
				//
				VkCommandBuffer Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

				VkImageSubresourceRange subresourceRange = {};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.levelCount = sparseTexture.mipLevels ;
				subresourceRange.layerCount = 1;

				vulkanHelper->SetImageLayout(
					Cmd,
					sparseTexture.image,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					subresourceRange);

				vulkanDevice->FlushCommandBuffer(Cmd, queue.graphicQueue);
				Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
				
				vkCmdCopyBufferToImage(
					Cmd,
					sparseTexture.stagingBuffer.buffer,
					sparseTexture.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					imageCopy.size(),
					imageCopy.data());

				vulkanDevice->FlushCommandBuffer(Cmd, queue.graphicQueue);
				Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);
				
				vulkanHelper->SetImageLayout(
					Cmd,
					sparseTexture.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					subresourceRange);

				vulkanDevice->FlushCommandBuffer(Cmd, queue.graphicQueue);
			}
			
			
		}
		

	
	}
	void BindInitialPages()
	{
		//int totalPagesBinded = 0;
		//int totalPagesUnBinded = 0;
		//std::deque <Pages> ::iterator it;
		std::vector<VkSparseImageMemoryBind> currentImageBind;
		std::vector < VkBufferImageCopy> imageCopy;
		//for (it = requestPageQueue.begin(); it != requestPageQueue.end(); ++it)
		{
			//if ((*it).miplevel != 1)
			//{
			//	//continue;
			//}
			//if (sparseTexture.memoryDeviceList.size() == 0)
			//{
			//	break;
			//}
			//if (totalPagesBinded >= perFrameBindPage)
			//{
			//	break;
			//}
			//if (activePageCheck[(*it).miplevel][(*it).pageNumber] == 1)
			//{
			//	continue;
			//}
			activePageCheck[7][0] = 1;
			PagesActive pageActive;
			pageActive.miplevel = 7;
			pageActive.pageNumber = 0;
			pageActive.deviceMemoryLocation = sparseTexture.memoryCount[0];

			pageActive.memory = sparseTexture.memoryDeviceList[0];
			activePageQueue.emplace_back(pageActive);
			//
			sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber].memory = pageActive.memory;
			currentImageBind.push_back(sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber]);
			//
			VkImageSubresourceLayers  subresourceRange = {};
			subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			subresourceRange.mipLevel = pageActive.miplevel;
			subresourceRange.layerCount = 1;
			subresourceRange.baseArrayLayer = 0;
			//
			VkBufferImageCopy imageCopyInst;
			imageCopyInst.bufferOffset = pageActive.deviceMemoryLocation * 65536;
			imageCopyInst.bufferRowLength = 0;
			imageCopyInst.bufferImageHeight = 0;
			imageCopyInst.imageOffset = sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber].offset;
			imageCopyInst.imageExtent = sparseTexture.imageBinds[pageActive.miplevel][pageActive.pageNumber].extent;
			imageCopyInst.imageSubresource = subresourceRange;
			imageCopy.push_back(imageCopyInst);
			//todo
			//fread
			char * stagingBufferLocation = (char *)sparseTexture.stagingBuffer.mapped;
			char * tempSB = stagingBufferLocation + pageActive.deviceMemoryLocation * 65536;
			int currentReadLocation = 65536 * sparseTexture.readByteOffset[pageActive.miplevel] + pageActive.pageNumber * 65536;
			//fseek(sparseTexture.textureFile, 65536*1000, SEEK_SET);
			fseek(sparseTexture.textureFile, currentReadLocation, SEEK_SET);
			int result = fread((void*)tempSB, sizeof(char), 65536, sparseTexture.textureFile);

			if (!result)
			{
				throw std::runtime_error("Error: fread");
			}
			//
			sparseTexture.memoryCount.pop_front();
			sparseTexture.memoryDeviceList.pop_front();
			//totalPagesBinded += 1;
			timerPageCheck[7][0] = FrameTimer;
			//requestPageCheck[(*it).miplevel][(*it).pageNumber] = 0;

		}
		/*for (int i = 0; i < totalPagesBinded; ++i)
		{
			requestPageQueue.pop_front();
		}*/
		//std::vector <PagesActive> ::iterator itr;

		//for (itr = activePageQueue.begin(); itr != activePageQueue.end(); ++itr)
		//{

		//	if (FrameTimer - timerPageCheck[(*itr).miplevel][(*itr).pageNumber] > pageDeleteTime)
		//	{
		//		timerPageCheck[(*itr).miplevel][(*itr).pageNumber] = 0;
		//		activePageCheck[(*itr).miplevel][(*itr).pageNumber] = 0;
		//		sparseTexture.memoryCount.emplace_back((*itr).deviceMemoryLocation);
		//		sparseTexture.memoryDeviceList.emplace_back((*itr).memory);
		//		totalPagesUnBinded += 1;
		//		sparseTexture.imageBinds[(*itr).miplevel][(*itr).pageNumber].memory = VK_NULL_HANDLE;
		//		currentImageBind.push_back(sparseTexture.imageBinds[(*itr).miplevel][(*itr).pageNumber]);
		//		if (itr != activePageQueue.begin()) {


		//			activePageQueue.erase(itr--);
		//		}
		//		else
		//		{
		//			//activePageQueue.erase(itr);
		//			//activePageQueue.erase(itr);
		//			//itr = activePageQueue.begin();
		//		}

		//	}

		//}

		if (currentImageBind.size() > 0)
		{
			VkSparseImageMemoryBindInfo imageBindInfo;					// Sparse image memory bind info 
			VkSparseImageOpaqueMemoryBindInfo mipTailBindInfo;

			VkBindSparseInfo bindSparseInfo{};
			bindSparseInfo.sType = VK_STRUCTURE_TYPE_BIND_SPARSE_INFO;
			// Image memory binds
			imageBindInfo.image = sparseTexture.image;
			imageBindInfo.bindCount = static_cast<uint32_t>(currentImageBind.size());
			imageBindInfo.pBinds = currentImageBind.data();
			bindSparseInfo.imageBindCount = (imageBindInfo.bindCount > 0) ? 1 : 0;
			bindSparseInfo.pImageBinds = &imageBindInfo;

			// Opaque image memory binds (mip tail)
			mipTailBindInfo.image = sparseTexture.image;
			mipTailBindInfo.bindCount = 0;// static_cast<uint32_t>(opaqueMemoryBinds.size());
			//mipTailBindInfo.pBinds = &sparseTexture.mipTailBind;
			bindSparseInfo.imageOpaqueBindCount = (mipTailBindInfo.bindCount > 0) ? 1 : 0;
			bindSparseInfo.pImageOpaqueBinds = &mipTailBindInfo;

			vkQueueBindSparse(queue.graphicQueue, 1, &bindSparseInfo, VK_NULL_HANDLE);
			//todo: use sparse bind semaphore
			vkQueueWaitIdle(queue.graphicQueue);

			if (imageCopy.size() > 0)
			{
				//
				VkCommandBuffer Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

				VkImageSubresourceRange subresourceRange = {};
				subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				subresourceRange.baseMipLevel = 0;
				subresourceRange.levelCount = sparseTexture.mipLevels;
				subresourceRange.layerCount = 1;

				vulkanHelper->SetImageLayout(
					Cmd,
					sparseTexture.image,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					subresourceRange);

				vulkanDevice->FlushCommandBuffer(Cmd, queue.graphicQueue);
				Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

				vkCmdCopyBufferToImage(
					Cmd,
					sparseTexture.stagingBuffer.buffer,
					sparseTexture.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					imageCopy.size(),
					imageCopy.data());

				vulkanDevice->FlushCommandBuffer(Cmd, queue.graphicQueue);
				Cmd = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

				vulkanHelper->SetImageLayout(
					Cmd,
					sparseTexture.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
					subresourceRange);

				vulkanDevice->FlushCommandBuffer(Cmd, queue.graphicQueue);
			}


		}



	}
	virtual void Update()
	{
		UpdateUniformBuffers();
		if (FrameTimer > 0)
		{
			ReadFeedBackBuffer();
			BindPages();
			timerPageCheck[7][0] = FrameTimer;
		}
		else
		{
			BindInitialPages();
		}
		memset(feedBackBuffer.mapped, 0, FEED_BACK_SIZE * sizeof(Pages));
		FrameTimer += 1;
	}
	void DrawLoop()
	{
		//BuildCommandBuffers();
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
		if (vulkanDevice->physicalDeviceFeatures.sparseBinding) {
			enabledFeatures.sparseBinding = VK_TRUE;
		}
		if (vulkanDevice->physicalDeviceFeatures.sparseResidencyImage2D) {
			enabledFeatures.sparseResidencyImage2D = VK_TRUE;
		}
		if (vulkanDevice->physicalDeviceFeatures.shaderResourceResidency) {
			enabledFeatures.shaderResourceResidency = VK_TRUE;
		}
		if (vulkanDevice->physicalDeviceFeatures.shaderResourceMinLod) {
			enabledFeatures.shaderResourceMinLod = VK_TRUE;
		}
		if (vulkanDevice->physicalDeviceFeatures.fragmentStoresAndAtomics) {
			enabledFeatures.fragmentStoresAndAtomics = VK_TRUE;
		}
	}
	virtual void GuiUpdate(GUI * gui)
	{
		if (gui->Header("Scene Details"))
		{
			gui->Text("Model Count: %i", GAME_OBJECT_COUNT);
		}
		
		if (gui->CheckBox("Enable Debug", &checkBoxDebugDraw))
		{
			
			if (checkBoxDebugDraw)
			{
				perModelData.debugDraw = 1;
			}
			else
			{
				perModelData.debugDraw = 0;
			}
				
		}
		if (gui->SliderInt("Page Delete Time", &pageDeleteTime, 0, 100))
		{
		
		}
		
			if (gui->SliderInt("Per Frame Page Bind Count", &perFrameBindPage, 0, 100))
			{

			}
	}
};

VULKAN_RENDERER()