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
#define GAME_OBJECT_COUNT 11


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
	Texture2D envMapTexture;
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
	int type = 0;
};

struct AllModel
{
	Model modelData;
	Material materialData;
	
};
struct Scene
{
	std::vector<GameObject> gameObjectList;

	GameObject skybox;
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
	TextureCubeMap environmentCubeMap;
	//
	TextureCubeMap irradiansCubeMap;
	//
	TextureCubeMap preFilterCubeMap;
	//
	Texture2D brdfMap;
	//
	std::vector<VkDescriptorSet> descriptorSets;
	//
	LightData lightData;

	//
	glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
	std::vector<glm::mat4> captureViews =
	{
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  -1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  -1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f,  0.0f), glm::vec3(0.0f,  0.0f, 1.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
		glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
	};
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
		glm::mat4 projMatrix;
		glm::mat4 viewMatrix;
		glm::vec3 viewPos;
	} uboVert;

	// Data send to the shader per model
	struct PerModelData
	{
		alignas(16) glm::mat4 modelMat;
		alignas(16) glm::vec4 color;
		alignas(16) glm::mat4 proj;
		alignas(16) glm::mat4 view;
	}perModelData;

	struct {
		VkPipeline skybox;
		VkPipeline pbr;
		VkPipeline envMapRect;
		VkPipeline irradians;
		VkPipeline preFilter;
		VkPipeline brdf;
	} pipelines;
	struct FrameBufferAttachment {
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
		VkFormat format;
	};
	struct FrameBuffer {
		int32_t width, height;
		VkFramebuffer frameBuffer;
		FrameBufferAttachment mapAttachment;
		VkRenderPass renderPass;
		VkSampler colorSampler;
		VkPipelineLayout pipelineLayout;
	} offScreenFrameBuf, irradiansFrameBuf, preFilterFrameBuf, brdfFrameBuf;

	
	

	UnitTest() : VulkanRenderer()
	{
		appName = "IBL";
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
		vkDestroyPipeline(vulkanDevice->logicalDevice, pipelines.pbr, nullptr);
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
		float posY = 1.5f;
		float posZ = 0.0f;
		AllModel allModel;
		//Load Model Globe
		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("RustAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("RustNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("RustAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("RustMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("RustRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("TitaniumAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("TitaniumNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("RustAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("TitaniumMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("TitaniumRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);

		// Skybox
		allModel.modelData.LoadModel("cube.obj", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("Studio.hdr", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		
		allModelList.push_back(allModel);


		allModel.modelData.LoadModel("Box.obj", vertexLayout, 0.05f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("RustNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("RustMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("RustRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Block.obj", vertexLayout, 0.5f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("PanelAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("PanelNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("PanelHeight.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("PanelAO.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("PanelMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("PanelRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Sneaker.obj", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("SneakerAlbedo.jpg", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("SneakerNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("PanelHeight.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("SneakerAO.jpg", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("SneakerMetal.jpg", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("SneakerRough.jpg", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);

	

		//
		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("CopperAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("CopperNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("CopperAO.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("CopperMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("CopperRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);

		//
		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("OxyCopperAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("OxyCopperNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("OxyCopperMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("OxyCopperRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);

		//
		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("RustCoatAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("RustCoatNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("RustCoatMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("RustCoatRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);

		//
		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("GoldAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("GoldNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("GoldMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("GoldRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);
		
		//
		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("OrnateAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.normalTexture.LoadFromFile("OrnateNormal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.heightTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.aoTexture.LoadFromFile("OrnateAO.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.metalTexture.LoadFromFile("OrnateMetal.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.roughTexture.LoadFromFile("OrnateRough.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModel.materialData.envMapTexture.LoadFromFile("WhiteImage.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);

		allModelList.push_back(allModel);
		// Load Object 1
		GameObject gameObj;
		gameObj.model = &(allModelList[0].modelData);
		gameObj.material = &(allModelList[0].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(1.0, 1.0, 1.0);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
		gameObj.type = 0;
		scene.gameObjectList.emplace_back(gameObj);
		

		posX = 0.0f;
		posY = 3.0f;
		posZ = 2.0f;

		// Load Object 2

		gameObj.model = &(allModelList[1].modelData);
		gameObj.material = &(allModelList[1].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(4.0, 1.0, 1.0);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
		gameObj.type = 1;
		scene.gameObjectList.emplace_back(gameObj);

		posX = 0.0f;
		posY = 500.0f;
		posZ = 0.0f;

		// Load Object 3
	
		gameObj.model = &(allModelList[2].modelData);
		gameObj.material = &(allModelList[2].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(posX, posY, posZ);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(5.0f, 5.0f, 5.0f);
		gameObj.type = 2;
		scene.skybox = gameObj;
		

		posX = 0.0f;
		posY = 2.0f;
		posZ = -2.0f;
		for (int i = 0; i < MAX_LIGHT_COUNT; ++i)
		{
			GameObject gameObj;
			gameObj.model = &(allModelList[3].modelData);
			gameObj.material = &(allModelList[3].materialData);
			gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

			gameObj.transform.positionPerAxis = glm::vec3(posX, posY, posZ);
			gameObj.transform.rotateAngelPerAxis = glm::vec3(0.0f, 0.0f, 0.0f);
			gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
			gameObj.type = 3;
			scene.gameObjectList.emplace_back(gameObj);

			LightInstanceDetails lightDetails;
			lightDetails.position = glm::vec3(posX, posY - 0.25, posZ);
			lightDetails.color = glm::vec3(10.0f, 1.0f, 1.0f);

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


		gameObj.model = &(allModelList[4].modelData);
		gameObj.material = &(allModelList[4].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

		gameObj.transform.positionPerAxis = glm::vec3(0.3f, -0.055f, 0.30f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(0.0f, glm::radians(00.0f), 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(50.0f, 5.0f, 40.0f);
		gameObj.type = 4;
		scene.gameObjectList.emplace_back(gameObj);

		// Lead Sneakers

		gameObj.model = &(allModelList[5].modelData);
		gameObj.material = &(allModelList[5].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);

		gameObj.transform.positionPerAxis = glm::vec3(0.0f, 1.0f, -5.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(00.0f), 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(0.1f, 0.1f, 0.1f);
		gameObj.type = 5;
		scene.gameObjectList.emplace_back(gameObj);

		// copper sphere
		gameObj.model = &(allModelList[6].modelData);
		gameObj.material = &(allModelList[6].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(8.0f, 1.0f, 1.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
		gameObj.type = 6;
		scene.gameObjectList.emplace_back(gameObj);

		// copper oxdised sphere
		gameObj.model = &(allModelList[7].modelData);
		gameObj.material = &(allModelList[7].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(12.0f, 1.0f, 1.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
		gameObj.type = 7;
		scene.gameObjectList.emplace_back(gameObj);

		// copper oxdised sphere
		gameObj.model = &(allModelList[8].modelData);
		gameObj.material = &(allModelList[8].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(16.0f, 1.0f, 1.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
		gameObj.type = 8;
		scene.gameObjectList.emplace_back(gameObj);

		// Gold sphere
		gameObj.model = &(allModelList[9].modelData);
		gameObj.material = &(allModelList[9].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(20.0f, 1.0f,1.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
		gameObj.type = 9;
		scene.gameObjectList.emplace_back(gameObj);


		// ORnate sphere
		gameObj.model = &(allModelList[10].modelData);
		gameObj.material = &(allModelList[10].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(24.0f, 1.0f, 1.0f);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(90.0f), glm::radians(0.0f), glm::radians(0.0f));
		gameObj.transform.scalePerAxis = glm::vec3(1.0f, 1.0f, 1.0f);
		gameObj.type = 10;
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
		uboVert.projMatrix = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 10000.0f);
		uboVert.projViewMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 10000.0f) * viewMatrix;
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

		VkDescriptorSetLayoutBinding envMapSamplerSetLayoutBinding{};
		envMapSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		envMapSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		envMapSamplerSetLayoutBinding.binding = 9;
		envMapSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding envCubeMapSamplerSetLayoutBinding{};
		envCubeMapSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		envCubeMapSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		envCubeMapSamplerSetLayoutBinding.binding = 10;
		envCubeMapSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding irradiansMapSamplerSetLayoutBinding{};
		irradiansMapSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		irradiansMapSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		irradiansMapSamplerSetLayoutBinding.binding = 16;
		irradiansMapSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding preFilterMapSamplerSetLayoutBinding{};
		preFilterMapSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		preFilterMapSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		preFilterMapSamplerSetLayoutBinding.binding = 22;
		preFilterMapSamplerSetLayoutBinding.descriptorCount = 1;

		VkDescriptorSetLayoutBinding brdfMapSamplerSetLayoutBinding{};
		brdfMapSamplerSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		brdfMapSamplerSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		brdfMapSamplerSetLayoutBinding.binding = 28;
		brdfMapSamplerSetLayoutBinding.descriptorCount = 1;

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
		setLayoutBindings.push_back(envMapSamplerSetLayoutBinding);
		setLayoutBindings.push_back(envCubeMapSamplerSetLayoutBinding);
		setLayoutBindings.push_back(irradiansMapSamplerSetLayoutBinding);
		setLayoutBindings.push_back(preFilterMapSamplerSetLayoutBinding);
		setLayoutBindings.push_back(brdfMapSamplerSetLayoutBinding);

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

		for (int i = 0; i < GAME_OBJECT_COUNT; ++i)
		{
			descriptorSetLayouts.emplace_back(descriptorSetLayout);
		}

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
		std::string vertAddress = fileName + "src\\IBL\\Shaders\\PBR.vert.spv";
		std::string fragAddress = fileName + "src\\IBL\\Shaders\\PBR.frag.spv";
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

		VkResult res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.pbr);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);
		//////////////////////////////////////////////////////////////////////////////////////////////

		 vertAddress = fileName + "src\\IBL\\Shaders\\Skybox.vert.spv";
		 fragAddress = fileName + "src\\IBL\\Shaders\\Skybox.frag.spv";
		//Vertex pipeline shader stage

		vertShaderStage.module = resourceLoader->LoadShader(vertAddress);
		assert(vertShaderStage.module != VK_NULL_HANDLE);
		shaderStages[0] = vertShaderStage;

		//fragment pipeline shader stage
		fragShaderStage.module = resourceLoader->LoadShader(fragAddress);
		assert(fragShaderStage.module != VK_NULL_HANDLE);
		shaderStages[1] = fragShaderStage;

		 res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.skybox);
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
		VkDescriptorPoolSize albedoSamplerDescriptorPoolSize{};
		albedoSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		albedoSamplerDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize normalSamplerDescriptorPoolSize{};
		normalSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalSamplerDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize heightSamplerDescriptorPoolSize{};
		heightSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		heightSamplerDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize aoSamplerDescriptorPoolSize{};
		aoSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		aoSamplerDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize metalSamplerDescriptorPoolSize{};
		metalSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		metalSamplerDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize roughSamplerDescriptorPoolSize{};
		roughSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		roughSamplerDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for uniform buffer
		VkDescriptorPoolSize lightDataDescriptorPoolSize{};
		lightDataDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		lightDataDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for uniform buffer
		VkDescriptorPoolSize instanceLightsDescriptorPoolSize{};
		instanceLightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		instanceLightsDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize envMapSamplerDescriptorPoolSize{};
		envMapSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		envMapSamplerDescriptorPoolSize.descriptorCount = 10;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize envCubeMapSamplerDescriptorPoolSize{};
		envCubeMapSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		envCubeMapSamplerDescriptorPoolSize.descriptorCount = 30;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize irradiansMapSamplerDescriptorPoolSize{};
		irradiansMapSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		irradiansMapSamplerDescriptorPoolSize.descriptorCount = 30;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize preFilterMapSamplerDescriptorPoolSize{};
		preFilterMapSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		preFilterMapSamplerDescriptorPoolSize.descriptorCount = 30;

		//Descriptor pool for Image Sampler
		VkDescriptorPoolSize brdfMapSamplerDescriptorPoolSize{};
		brdfMapSamplerDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		brdfMapSamplerDescriptorPoolSize.descriptorCount = 30;


		// Example uses tw0 ubo and one combined image sampler
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.push_back(uniformDescriptorPoolSize);
		poolSizes.push_back(albedoSamplerDescriptorPoolSize);
		poolSizes.push_back(normalSamplerDescriptorPoolSize);
		poolSizes.push_back(heightSamplerDescriptorPoolSize);
		poolSizes.push_back(aoSamplerDescriptorPoolSize);
		poolSizes.push_back(metalSamplerDescriptorPoolSize);
		poolSizes.push_back(roughSamplerDescriptorPoolSize);
		poolSizes.push_back(lightDataDescriptorPoolSize);
		poolSizes.push_back(instanceLightsDescriptorPoolSize);
		poolSizes.push_back(envMapSamplerDescriptorPoolSize);
		poolSizes.push_back(envCubeMapSamplerDescriptorPoolSize);
		poolSizes.push_back(irradiansMapSamplerDescriptorPoolSize);
		poolSizes.push_back(preFilterMapSamplerDescriptorPoolSize);
		poolSizes.push_back(brdfMapSamplerDescriptorPoolSize);

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
			VkWriteDescriptorSet albedoSamplerWriteDescriptorSet{};
			albedoSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			albedoSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			albedoSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			albedoSamplerWriteDescriptorSet.dstBinding = 1;
			albedoSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.albedoTexture.descriptor);
			albedoSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 2 : Normal map Model 1
			VkWriteDescriptorSet normalSamplerWriteDescriptorSet{};
			normalSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			normalSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			normalSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			normalSamplerWriteDescriptorSet.dstBinding = 2;
			normalSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.normalTexture.descriptor);
			normalSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 3 : Height map Model 1
			VkWriteDescriptorSet heightSamplerWriteDescriptorSet{};
			heightSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			heightSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			heightSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			heightSamplerWriteDescriptorSet.dstBinding = 3;
			heightSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.heightTexture.descriptor);
			heightSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 4 : AO map Model 1
			VkWriteDescriptorSet aoSamplerWriteDescriptorSet{};
			aoSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			aoSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			aoSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			aoSamplerWriteDescriptorSet.dstBinding = 4;
			aoSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.aoTexture.descriptor);
			aoSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 5 : Metal map Model 1
			VkWriteDescriptorSet metalSamplerWriteDescriptorSet{};
			metalSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			metalSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			metalSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			metalSamplerWriteDescriptorSet.dstBinding = 5;
			metalSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.metalTexture.descriptor);
			metalSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 6 : Rough map Model 1
			VkWriteDescriptorSet roughSamplerWriteDescriptorSet{};
			roughSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			roughSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			roughSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			roughSamplerWriteDescriptorSet.dstBinding = 6;
			roughSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.roughTexture.descriptor);
			roughSamplerWriteDescriptorSet.descriptorCount = 1;


			// Binding 7 : Light data buffer
			VkWriteDescriptorSet lightDataBufferWriteDescriptorSet{};
			lightDataBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			lightDataBufferWriteDescriptorSet.dstSet = descriptorSets[i];
			lightDataBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			lightDataBufferWriteDescriptorSet.dstBinding = 7;
			lightDataBufferWriteDescriptorSet.pBufferInfo = &(lightDetailsBuffer.descriptor);
			lightDataBufferWriteDescriptorSet.descriptorCount = 1;

			// Binding 8 : Light Instance
			VkWriteDescriptorSet instancLightBufferWriteDescriptorSet{};
			instancLightBufferWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			instancLightBufferWriteDescriptorSet.dstSet = descriptorSets[i];
			instancLightBufferWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			instancLightBufferWriteDescriptorSet.dstBinding = 8;
			instancLightBufferWriteDescriptorSet.pBufferInfo = &(instanceLightBuffer.descriptor);
			instancLightBufferWriteDescriptorSet.descriptorCount = 1;


			//  Binding 9 : Env Map
			VkWriteDescriptorSet envMapSamplerWriteDescriptorSet{};
			envMapSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			envMapSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			envMapSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			envMapSamplerWriteDescriptorSet.dstBinding = 9;
			envMapSamplerWriteDescriptorSet.pImageInfo = &(allModelList[i].materialData.envMapTexture.descriptor);
			envMapSamplerWriteDescriptorSet.descriptorCount = 1;

			//  Binding 10 : Env Map
			VkWriteDescriptorSet envCubeMapSamplerWriteDescriptorSet{};
			envCubeMapSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			envCubeMapSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			envCubeMapSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			envCubeMapSamplerWriteDescriptorSet.dstBinding = 10;
			envCubeMapSamplerWriteDescriptorSet.pImageInfo = &(environmentCubeMap.descriptor);
			envCubeMapSamplerWriteDescriptorSet.descriptorCount = 1;
			//  Binding 16 : Env Map
			VkWriteDescriptorSet irradiansMapSamplerWriteDescriptorSet{};
			irradiansMapSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			irradiansMapSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			irradiansMapSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			irradiansMapSamplerWriteDescriptorSet.dstBinding = 16;
			irradiansMapSamplerWriteDescriptorSet.pImageInfo = &(irradiansCubeMap.descriptor);
			irradiansMapSamplerWriteDescriptorSet.descriptorCount = 1;

			VkWriteDescriptorSet preFilterMapSamplerWriteDescriptorSet{};
			preFilterMapSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			preFilterMapSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			preFilterMapSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			preFilterMapSamplerWriteDescriptorSet.dstBinding = 22;
			preFilterMapSamplerWriteDescriptorSet.pImageInfo = &(preFilterCubeMap.descriptor);
			preFilterMapSamplerWriteDescriptorSet.descriptorCount = 1;


			VkWriteDescriptorSet brdfMapSamplerWriteDescriptorSet{};
			brdfMapSamplerWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			brdfMapSamplerWriteDescriptorSet.dstSet = descriptorSets[i];
			brdfMapSamplerWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			brdfMapSamplerWriteDescriptorSet.dstBinding = 28;
			brdfMapSamplerWriteDescriptorSet.pImageInfo = &(brdfMap.descriptor);
			brdfMapSamplerWriteDescriptorSet.descriptorCount = 1;


			
			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(albedoSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(normalSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(heightSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(aoSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(metalSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(roughSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(lightDataBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(instancLightBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(envMapSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(envCubeMapSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(irradiansMapSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(preFilterMapSamplerWriteDescriptorSet);
			writeDescriptorSets.push_back(brdfMapSamplerWriteDescriptorSet);

			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);

			writeDescriptorSets.clear();
		}
		

	}

	void DrawModels(int commandBufferCount)
	{


		//Draw all models 
		for (int i = 0; i < scene.gameObjectList.size(); ++i)
		{
			vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferCount], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[scene.gameObjectList[i].type], 0, NULL);


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

	void DrawSkybox(int commandBufferCount)
	{
		
		vkCmdBindDescriptorSets(drawCmdBuffers[commandBufferCount], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[scene.skybox.type], 0, NULL);


		UpdateModelBuffer(scene.skybox, commandBufferCount);
		VkDeviceSize offsets[1] = { 0 };

			// Bind mesh vertex buffer
		vkCmdBindVertexBuffers(drawCmdBuffers[commandBufferCount], 0, 1, &scene.skybox.model->vertices.buffer, offsets);
			// Bind mesh index buffer
		vkCmdBindIndexBuffer(drawCmdBuffers[commandBufferCount], scene.skybox.model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
		vkCmdDrawIndexed(drawCmdBuffers[commandBufferCount], scene.skybox.model->indexCount, 1, 0, 0, 0);
		


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


			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.skybox);
			DrawSkybox(i);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.pbr);
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
		image.extent.width = 512;
		image.extent.height = 512;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
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
	void CreateAttachment1(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment *attachment)
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
		image.extent.width = 32;
		image.extent.height = 32;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
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
	void CreateAttachment2(VkFormat format, VkImageUsageFlagBits usage, FrameBufferAttachment *attachment)
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
		image.extent.width = 128;
		image.extent.height = 128;
		image.extent.depth = 1;
		image.mipLevels = 1;
		image.arrayLayers = 1;
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
	void BuildEquirectangularToCubemap()
	{
		const VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
		const int32_t dim = 512;
		const uint32_t numMips = static_cast<uint32_t>(std::floor(std::log2(std::max(dim, dim)))) + 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VkResult res = vkCreateImage(vulkanDevice->logicalDevice, &imageCI, nullptr, &environmentCubeMap.image);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image");
		}
		VkMemoryAllocateInfo memAlloc{};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, environmentCubeMap.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAlloc, nullptr, &environmentCubeMap.deviceMemory);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Allocate Memory");
		}
		res = vkBindImageMemory(vulkanDevice->logicalDevice, environmentCubeMap.image, environmentCubeMap.deviceMemory, 0);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Bind Image Memory");
		}
		// Image view
		VkImageViewCreateInfo viewCI{};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = environmentCubeMap.image;
		res = vkCreateImageView(vulkanDevice->logicalDevice, &viewCI, nullptr, &environmentCubeMap.view);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Create Image View");
		}
		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		res = vkCreateSampler(vulkanDevice->logicalDevice, &samplerCI, nullptr, &environmentCubeMap.sampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
		environmentCubeMap.descriptor.imageView = environmentCubeMap.view;
		environmentCubeMap.descriptor.sampler = environmentCubeMap.sampler;
		environmentCubeMap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		environmentCubeMap.vulkanDevice = vulkanDevice;
//////////////////////////////////////////////////////////////////////
		offScreenFrameBuf.width = 512;
		offScreenFrameBuf.height = 512;

		// (World space) Positions
		CreateAttachment(VK_FORMAT_R32G32B32A32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &offScreenFrameBuf.mapAttachment);
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
		attachmentDescs[0].format = offScreenFrameBuf.mapAttachment.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());


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

		 res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &offScreenFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 1> attachments;
		attachments[0] = offScreenFrameBuf.mapAttachment.view;

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
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &offScreenFrameBuf.colorSampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}

////////////////////////////////////////////////////////////////////////////////////
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
		std::string vertAddress = fileName + "src\\IBL\\Shaders\\EquiToCube.vert.spv";
		std::string fragAddress = fileName + "src\\IBL\\Shaders\\EquiToCube.frag.spv";
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
		//pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.envMapRect);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);
		

		
	}

	void RenderEnvToCube()
	{
		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = offScreenFrameBuf.renderPass;
		renderPassBeginInfo.framebuffer = offScreenFrameBuf.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = 512;
		renderPassBeginInfo.renderArea.extent.height = 512;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		VkCommandBuffer cmdBuf = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport{};
		viewport.width = (float)512;
		viewport.height = -(float)512;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = float(512);


		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent.width = 512;
		scissor.extent.height = 512;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		int mipLevel = static_cast<int>(std::floor(std::log2(std::max(512, 512)))) + 1;

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = mipLevel;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		 vulkanHelper->SetImageLayout(
			cmdBuf,
			environmentCubeMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		 
		 for (uint32_t f = 0; f < 6; f++) 
		 {
			 perModelData.proj = captureProjection;
			 perModelData.view = captureViews[f];
			 vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
			 // Render scene from cube face's point of view
			 vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			 vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.envMapRect);
			 vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[scene.skybox.type], 0, NULL);

			 VkDeviceSize offsets[1] = { 0 };

			 // Bind mesh vertex buffer
			 vkCmdBindVertexBuffers(cmdBuf, 0, 1, &scene.skybox.model->vertices.buffer, offsets);
			 // Bind mesh index buffer
			 vkCmdBindIndexBuffer(cmdBuf, scene.skybox.model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			 // Render mesh vertex buffer using it's indices
			 vkCmdDrawIndexed(cmdBuf, scene.skybox.model->indexCount, 1, 0, 0, 0);

			 vkCmdEndRenderPass(cmdBuf);

			 vulkanHelper->SetImageLayout(
				 cmdBuf,
				 offScreenFrameBuf.mapAttachment.image,
				 VK_IMAGE_ASPECT_COLOR_BIT,
				 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			 // Copy region for transfer from framebuffer to cube face
			 VkImageCopy copyRegion = {};

			 copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			 copyRegion.srcSubresource.baseArrayLayer = 0;
			 copyRegion.srcSubresource.mipLevel = 0;
			 copyRegion.srcSubresource.layerCount = 1;
			 copyRegion.srcOffset = { 0, 0, 0 };

			 copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			 copyRegion.dstSubresource.baseArrayLayer = f;
			 copyRegion.dstSubresource.mipLevel = 0;
			 copyRegion.dstSubresource.layerCount = 1;
			 copyRegion.dstOffset = { 0, 0, 0 };

			 copyRegion.extent.width = static_cast<uint32_t>(512);
			 copyRegion.extent.height = static_cast<uint32_t>(512);
			 copyRegion.extent.depth = 1;

			 vkCmdCopyImage(
				 cmdBuf,
				 offScreenFrameBuf.mapAttachment.image,
				 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				 environmentCubeMap.image,
				 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				 1,
				 &copyRegion);

			 for (int k = 0; k < mipLevel-1; ++k)
			 {

				 VkImageBlit blitRegion = {};
				 blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				 blitRegion.srcSubresource.baseArrayLayer = 0;
				 blitRegion.srcSubresource.layerCount = 1;
				 blitRegion.srcSubresource.mipLevel = 0;
				 blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				 blitRegion.dstSubresource.baseArrayLayer = f;
				 blitRegion.dstSubresource.layerCount = 1;
				 blitRegion.dstSubresource.mipLevel = k + 1;
				 blitRegion.srcOffsets[0].x = 0.0f;
				 blitRegion.srcOffsets[0].y = 0.0f;
				 blitRegion.srcOffsets[0].z = 0.0f;

				 blitRegion.srcOffsets[1].x = 512.0f;
				 blitRegion.srcOffsets[1].y = 512.0f;
				 blitRegion.srcOffsets[1].z = 1.0f;

				 blitRegion.dstOffsets[0].x = 0.0f;
				 blitRegion.dstOffsets[0].y = 0.0f;
				 blitRegion.dstOffsets[0].z = 0.0f;

				 blitRegion.dstOffsets[1].x = 512.0f / pow(2.0, k + 1);
				 blitRegion.dstOffsets[1].y = 512.0f / pow(2.0, k + 1);
				 blitRegion.dstOffsets[1].z = 1.0f;



				 vkCmdBlitImage(
					 cmdBuf,
					 offScreenFrameBuf.mapAttachment.image,
					 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					 environmentCubeMap.image,
					 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					 1,
					 &blitRegion,
					 VK_FILTER_LINEAR);
			 }


			 // Transform framebuffer color attachment back 
			 vulkanHelper->SetImageLayout(
				 cmdBuf,
				 offScreenFrameBuf.mapAttachment.image,
				 VK_IMAGE_ASPECT_COLOR_BIT,
				 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		 }

		 vulkanHelper->SetImageLayout(
			 cmdBuf,
			 environmentCubeMap.image,
			 VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			 VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			 subresourceRange);

		 vulkanDevice->FlushCommandBuffer(cmdBuf, queue.graphicQueue);

	}
	void BuildIrradiansMap()
	{
		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const int32_t dim = 32;
		const uint32_t numMips = 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VkResult res = vkCreateImage(vulkanDevice->logicalDevice, &imageCI, nullptr, &irradiansCubeMap.image);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image");
		}
		VkMemoryAllocateInfo memAlloc{};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, irradiansCubeMap.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAlloc, nullptr, &irradiansCubeMap.deviceMemory);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Allocate Memory");
		}
		res = vkBindImageMemory(vulkanDevice->logicalDevice, irradiansCubeMap.image, irradiansCubeMap.deviceMemory, 0);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Bind Image Memory");
		}
		// Image view
		VkImageViewCreateInfo viewCI{};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = irradiansCubeMap.image;
		res = vkCreateImageView(vulkanDevice->logicalDevice, &viewCI, nullptr, &irradiansCubeMap.view);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Create Image View");
		}
		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		res = vkCreateSampler(vulkanDevice->logicalDevice, &samplerCI, nullptr, &irradiansCubeMap.sampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
		irradiansCubeMap.descriptor.imageView = irradiansCubeMap.view;
		irradiansCubeMap.descriptor.sampler = irradiansCubeMap.sampler;
		irradiansCubeMap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		irradiansCubeMap.vulkanDevice = vulkanDevice;
		//////////////////////////////////////////////////////////////////////
		irradiansFrameBuf.width = 32;
		irradiansFrameBuf.height = 32;

		// (World space) Positions
		CreateAttachment1(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &irradiansFrameBuf.mapAttachment);
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
		attachmentDescs[0].format = irradiansFrameBuf.mapAttachment.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());


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

		res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &irradiansFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 1> attachments;
		attachments[0] = irradiansFrameBuf.mapAttachment.view;

		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = irradiansFrameBuf.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = irradiansFrameBuf.width;
		fbufCreateInfo.height = irradiansFrameBuf.height;
		fbufCreateInfo.layers = 1;
		res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &irradiansFrameBuf.frameBuffer);
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
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &irradiansFrameBuf.colorSampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}

		////////////////////////////////////////////////////////////////////////////////////
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
		std::string vertAddress = fileName + "src\\IBL\\Shaders\\IrrMap.vert.spv";
		std::string fragAddress = fileName + "src\\IBL\\Shaders\\IrrMap.frag.spv";
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
		pipelineCreateInfo.renderPass = irradiansFrameBuf.renderPass;
		pipelineCreateInfo.flags = 0;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;


		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		//pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.irradians);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);

	}
	void RenderIrradianceMap()
	{
		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.2f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = irradiansFrameBuf.renderPass;
		renderPassBeginInfo.framebuffer = irradiansFrameBuf.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = 32;
		renderPassBeginInfo.renderArea.extent.height = 32;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		VkCommandBuffer cmdBuf = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport{};
		viewport.width = (float)32;
		viewport.height = -(float)32;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = float(32);


		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent.width = 32;
		scissor.extent.height = 32;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vulkanHelper->SetImageLayout(
			cmdBuf,
			irradiansCubeMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t f = 0; f < 6; f++)
		{
			perModelData.proj = captureProjection;
			perModelData.view = captureViews[f];
			vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
			// Render scene from cube face's point of view
			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.irradians);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[scene.skybox.type], 0, NULL);

			VkDeviceSize offsets[1] = { 0 };

			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &scene.skybox.model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(cmdBuf, scene.skybox.model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(cmdBuf, scene.skybox.model->indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(cmdBuf);

			vulkanHelper->SetImageLayout(
				cmdBuf,
				irradiansFrameBuf.mapAttachment.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			// Copy region for transfer from framebuffer to cube face
			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = f;
			copyRegion.dstSubresource.mipLevel = 0;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(32);
			copyRegion.extent.height = static_cast<uint32_t>(32);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(
				cmdBuf,
				irradiansFrameBuf.mapAttachment.image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				irradiansCubeMap.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion);

			// Transform framebuffer color attachment back 
			vulkanHelper->SetImageLayout(
				cmdBuf,
				irradiansFrameBuf.mapAttachment.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}

		vulkanHelper->SetImageLayout(
			cmdBuf,
			irradiansCubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->FlushCommandBuffer(cmdBuf, queue.graphicQueue);

	}
	void BuildPreFilterMap()
	{
		const VkFormat format = VK_FORMAT_R16G16B16A16_SFLOAT;
		const int32_t dim = 128;
		const uint32_t numMips = 6;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 6;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VkResult res = vkCreateImage(vulkanDevice->logicalDevice, &imageCI, nullptr, &preFilterCubeMap.image);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image");
		}
		VkMemoryAllocateInfo memAlloc{};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, preFilterCubeMap.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAlloc, nullptr, &preFilterCubeMap.deviceMemory);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Allocate Memory");
		}
		res = vkBindImageMemory(vulkanDevice->logicalDevice, preFilterCubeMap.image, preFilterCubeMap.deviceMemory, 0);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Bind Image Memory");
		}
		// Image view
		VkImageViewCreateInfo viewCI{};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 6;
		viewCI.image = preFilterCubeMap.image;
		res = vkCreateImageView(vulkanDevice->logicalDevice, &viewCI, nullptr, &preFilterCubeMap.view);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Create Image View");
		}
		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		res = vkCreateSampler(vulkanDevice->logicalDevice, &samplerCI, nullptr, &preFilterCubeMap.sampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
		preFilterCubeMap.descriptor.imageView = preFilterCubeMap.view;
		preFilterCubeMap.descriptor.sampler = preFilterCubeMap.sampler;
		preFilterCubeMap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		preFilterCubeMap.vulkanDevice = vulkanDevice;
		//////////////////////////////////////////////////////////////////////
		preFilterFrameBuf.width = 128;
		preFilterFrameBuf.height = 128;

		// (World space) Positions
		CreateAttachment2(VK_FORMAT_R16G16B16A16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &preFilterFrameBuf.mapAttachment);
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
		attachmentDescs[0].format = preFilterFrameBuf.mapAttachment.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());


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

		res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &preFilterFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 1> attachments;
		attachments[0] = preFilterFrameBuf.mapAttachment.view;

		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = preFilterFrameBuf.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = preFilterFrameBuf.width;
		fbufCreateInfo.height = preFilterFrameBuf.height;
		fbufCreateInfo.layers = 1;
		res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &preFilterFrameBuf.frameBuffer);
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
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &preFilterFrameBuf.colorSampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}

		////////////////////////////////////////////////////////////////////////////////////
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
		std::string vertAddress = fileName + "src\\IBL\\Shaders\\PreFilter.vert.spv";
		std::string fragAddress = fileName + "src\\IBL\\Shaders\\PreFilter.frag.spv";
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
		pipelineCreateInfo.renderPass = preFilterFrameBuf.renderPass;
		pipelineCreateInfo.flags = 0;
		pipelineCreateInfo.basePipelineIndex = -1;
		pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;


		pipelineCreateInfo.pVertexInputState = &vertices.inputState;
		pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
		pipelineCreateInfo.pRasterizationState = &rasterizationState;
		pipelineCreateInfo.pColorBlendState = &colorBlendState;
		pipelineCreateInfo.pMultisampleState = &multisampleState;
		pipelineCreateInfo.pViewportState = &viewportState;
		//pipelineCreateInfo.pDepthStencilState = &depthStencilState;
		pipelineCreateInfo.pDynamicState = &dynamicState;
		pipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
		pipelineCreateInfo.pStages = shaderStages.data();

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.preFilter);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);

	}
	void RenderPreFilterMap()
	{
		VkClearValue clearValues[1];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = preFilterFrameBuf.renderPass;
		renderPassBeginInfo.framebuffer = preFilterFrameBuf.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = 128;
		renderPassBeginInfo.renderArea.extent.height = 128;
		renderPassBeginInfo.clearValueCount = 1;
		renderPassBeginInfo.pClearValues = clearValues;

		VkCommandBuffer cmdBuf = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport{};
		viewport.width = (float)128;
		viewport.height = -(float)128;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = float(128);


		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent.width = 128;
		scissor.extent.height = 128;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 6;
		subresourceRange.layerCount = 6;

		// Change image layout for all cubemap faces to transfer destination
		vulkanHelper->SetImageLayout(
			cmdBuf,
			preFilterCubeMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		for (uint32_t f = 0; f < 6; f++)
		{
			perModelData.proj = captureProjection;
			perModelData.view = captureViews[f];
			vkCmdPushConstants(cmdBuf, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
			// Render scene from cube face's point of view
			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.preFilter);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[scene.skybox.type], 0, NULL);

			VkDeviceSize offsets[1] = { 0 };

			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &scene.skybox.model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(cmdBuf, scene.skybox.model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			vkCmdDrawIndexed(cmdBuf, scene.skybox.model->indexCount, 1, 0, 0, 0);

			vkCmdEndRenderPass(cmdBuf);

			vulkanHelper->SetImageLayout(
				cmdBuf,
				preFilterFrameBuf.mapAttachment.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			// Copy region for transfer from framebuffer to cube face
			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = f;
			copyRegion.dstSubresource.mipLevel = 0;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(128);
			copyRegion.extent.height = static_cast<uint32_t>(128);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(
				cmdBuf,
				preFilterFrameBuf.mapAttachment.image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				preFilterCubeMap.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion);

			for (int k = 0; k < 5; ++k)
			{
				
				VkImageBlit blitRegion = {};
				blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blitRegion.srcSubresource.baseArrayLayer = 0;
				blitRegion.srcSubresource.layerCount = 1;
				blitRegion.srcSubresource.mipLevel = 0;
				blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				blitRegion.dstSubresource.baseArrayLayer = f;
				blitRegion.dstSubresource.layerCount = 1;
				blitRegion.dstSubresource.mipLevel = k+1;
				blitRegion.srcOffsets[0].x = 0.0f;
				blitRegion.srcOffsets[0].y = 0.0f;
				blitRegion.srcOffsets[0].z = 0.0f;

				blitRegion.srcOffsets[1].x = 128.0f;
				blitRegion.srcOffsets[1].y = 128.0f;
				blitRegion.srcOffsets[1].z = 1.0f;

				blitRegion.dstOffsets[0].x = 0.0f;
				blitRegion.dstOffsets[0].y = 0.0f;
				blitRegion.dstOffsets[0].z = 0.0f;

				blitRegion.dstOffsets[1].x = 128.0f / pow(2.0,k+1);
				blitRegion.dstOffsets[1].y = 128.0f / pow(2.0, k+1);
				blitRegion.dstOffsets[1].z = 1.0f;



					vkCmdBlitImage(
					cmdBuf,
					preFilterFrameBuf.mapAttachment.image,
					VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
					preFilterCubeMap.image,
					VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
					1,
					&blitRegion,
					VK_FILTER_LINEAR);
			}
			// Transform framebuffer color attachment back 
			vulkanHelper->SetImageLayout(
				cmdBuf,
				preFilterFrameBuf.mapAttachment.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		}

		vulkanHelper->SetImageLayout(
			cmdBuf,
			preFilterCubeMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->FlushCommandBuffer(cmdBuf, queue.graphicQueue);

	}
	void BuildBrdfMap()
	{
		const VkFormat format = VK_FORMAT_R16G16_SFLOAT;
		const int32_t dim = 512;
		const uint32_t numMips = 1;

		// Pre-filtered cube map
		// Image
		VkImageCreateInfo imageCI{};
		imageCI.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCI.imageType = VK_IMAGE_TYPE_2D;
		imageCI.format = format;
		imageCI.extent.width = dim;
		imageCI.extent.height = dim;
		imageCI.extent.depth = 1;
		imageCI.mipLevels = numMips;
		imageCI.arrayLayers = 1;
		imageCI.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCI.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCI.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		//imageCI.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
		VkResult res = vkCreateImage(vulkanDevice->logicalDevice, &imageCI, nullptr, &brdfMap.image);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Image");
		}
		VkMemoryAllocateInfo memAlloc{};
		memAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		VkMemoryRequirements memReqs;
		vkGetImageMemoryRequirements(vulkanDevice->logicalDevice, brdfMap.image, &memReqs);
		memAlloc.allocationSize = memReqs.size;
		memAlloc.memoryTypeIndex = vulkanDevice->GetMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		res = vkAllocateMemory(vulkanDevice->logicalDevice, &memAlloc, nullptr, &brdfMap.deviceMemory);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Allocate Memory");
		}
		res = vkBindImageMemory(vulkanDevice->logicalDevice, brdfMap.image, brdfMap.deviceMemory, 0);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Bind Image Memory");
		}
		// Image view
		VkImageViewCreateInfo viewCI{};
		viewCI.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewCI.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewCI.format = format;
		viewCI.subresourceRange = {};
		viewCI.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewCI.subresourceRange.levelCount = numMips;
		viewCI.subresourceRange.layerCount = 1;
		viewCI.image = brdfMap.image;
		res = vkCreateImageView(vulkanDevice->logicalDevice, &viewCI, nullptr, &brdfMap.view);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error:Create Image View");
		}
		// Sampler
		VkSamplerCreateInfo samplerCI{};
		samplerCI.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerCI.maxAnisotropy = 1.0f;
		samplerCI.magFilter = VK_FILTER_LINEAR;
		samplerCI.minFilter = VK_FILTER_LINEAR;
		samplerCI.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerCI.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCI.minLod = 0.0f;
		samplerCI.maxLod = static_cast<float>(numMips);
		samplerCI.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		res = vkCreateSampler(vulkanDevice->logicalDevice, &samplerCI, nullptr, &brdfMap.sampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}
		brdfMap.descriptor.imageView = brdfMap.view;
		brdfMap.descriptor.sampler = brdfMap.sampler;
		brdfMap.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		brdfMap.vulkanDevice = vulkanDevice;
		//////////////////////////////////////////////////////////////////////
		brdfFrameBuf.width = 512;
		brdfFrameBuf.height = 512;

		// (World space) Positions
		CreateAttachment(VK_FORMAT_R16G16_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &brdfFrameBuf.mapAttachment);
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
		attachmentDescs[0].format = brdfFrameBuf.mapAttachment.format;

		std::vector<VkAttachmentReference> colorReferences;
		colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

		VkSubpassDescription subpass = {};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.pColorAttachments = colorReferences.data();
		subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());


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

		res = vkCreateRenderPass(vulkanDevice->logicalDevice, &renderPassInfo, nullptr, &brdfFrameBuf.renderPass);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Render Pass");
		}

		std::array<VkImageView, 1> attachments;
		attachments[0] = brdfFrameBuf.mapAttachment.view;

		VkFramebufferCreateInfo fbufCreateInfo = {};
		fbufCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fbufCreateInfo.pNext = NULL;
		fbufCreateInfo.renderPass = brdfFrameBuf.renderPass;
		fbufCreateInfo.pAttachments = attachments.data();
		fbufCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		fbufCreateInfo.width = brdfFrameBuf.width;
		fbufCreateInfo.height = brdfFrameBuf.height;
		fbufCreateInfo.layers = 1;
		res = vkCreateFramebuffer(vulkanDevice->logicalDevice, &fbufCreateInfo, nullptr, &brdfFrameBuf.frameBuffer);
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
		res = vkCreateSampler(vulkanDevice->logicalDevice, &sampler, nullptr, &brdfFrameBuf.colorSampler);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Sampler");
		}

		////////////////////////////////////////////////////////////////////////////////////
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
		std::string vertAddress = fileName + "src\\IBL\\Shaders\\Brdf.vert.spv";
		std::string fragAddress = fileName + "src\\IBL\\Shaders\\Brdf.frag.spv";
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
		pipelineCreateInfo.renderPass = brdfFrameBuf.renderPass;
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

		res = vkCreateGraphicsPipelines(vulkanDevice->logicalDevice, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.brdf);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Graphics Pipelines");
		}
		// Shader modules are no longer needed once the graphics pipeline has been created
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[0].module, nullptr);
		vkDestroyShaderModule(vulkanDevice->logicalDevice, shaderStages[1].module, nullptr);

	}
	void RenderBrdfMap()
	{
		VkClearValue clearValues[2];
		clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
		clearValues[1].depthStencil = { 1.0f, 0 };

		VkRenderPassBeginInfo renderPassBeginInfo{};
		renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		// Reuse render pass from example pass
		renderPassBeginInfo.renderPass = brdfFrameBuf.renderPass;
		renderPassBeginInfo.framebuffer = brdfFrameBuf.frameBuffer;
		renderPassBeginInfo.renderArea.extent.width = 512;
		renderPassBeginInfo.renderArea.extent.height = 512;
		renderPassBeginInfo.clearValueCount = 2;
		renderPassBeginInfo.pClearValues = clearValues;

		VkCommandBuffer cmdBuf = vulkanDevice->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkViewport viewport{};
		viewport.width = (float)512;
		viewport.height = -(float)512;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		viewport.x = 0;
		viewport.y = float(512);


		vkCmdSetViewport(cmdBuf, 0, 1, &viewport);

		VkRect2D scissor{};
		scissor.extent.width = 512;
		scissor.extent.height = 512;
		scissor.offset.x = 0;
		scissor.offset.y = 0;
		vkCmdSetScissor(cmdBuf, 0, 1, &scissor);

		VkImageSubresourceRange subresourceRange = {};
		subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresourceRange.baseMipLevel = 0;
		subresourceRange.levelCount = 1;
		subresourceRange.layerCount = 1;

		// Change image layout for all cubemap faces to transfer destination
		vulkanHelper->SetImageLayout(
			cmdBuf,
			brdfMap.image,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			subresourceRange);

		
			
			// Render scene from cube face's point of view
			vkCmdBeginRenderPass(cmdBuf, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.brdf);
			vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[scene.skybox.type], 0, NULL);

			VkDeviceSize offsets[1] = { 0 };

			// Bind mesh vertex buffer
			vkCmdBindVertexBuffers(cmdBuf, 0, 1, &scene.skybox.model->vertices.buffer, offsets);
			// Bind mesh index buffer
			vkCmdBindIndexBuffer(cmdBuf, scene.skybox.model->indices.buffer, 0, VK_INDEX_TYPE_UINT32);
			// Render mesh vertex buffer using it's indices
			//vkCmdDrawIndexed(cmdBuf, scene.skybox.model->indexCount, 1, 0, 0, 0);
			vkCmdDraw(cmdBuf, 6, 1, 0, 0);
			vkCmdEndRenderPass(cmdBuf);

			vulkanHelper->SetImageLayout(
				cmdBuf,
				brdfFrameBuf.mapAttachment.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

			// Copy region for transfer from framebuffer to cube face
			VkImageCopy copyRegion = {};

			copyRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.srcSubresource.baseArrayLayer = 0;
			copyRegion.srcSubresource.mipLevel = 0;
			copyRegion.srcSubresource.layerCount = 1;
			copyRegion.srcOffset = { 0, 0, 0 };

			copyRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			copyRegion.dstSubresource.baseArrayLayer = 0;
			copyRegion.dstSubresource.mipLevel = 0;
			copyRegion.dstSubresource.layerCount = 1;
			copyRegion.dstOffset = { 0, 0, 0 };

			copyRegion.extent.width = static_cast<uint32_t>(512);
			copyRegion.extent.height = static_cast<uint32_t>(512);
			copyRegion.extent.depth = 1;

			vkCmdCopyImage(
				cmdBuf,
				brdfFrameBuf.mapAttachment.image,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				brdfMap.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
				1,
				&copyRegion);

		
			// Transform framebuffer color attachment back 
			vulkanHelper->SetImageLayout(
				cmdBuf,
				brdfFrameBuf.mapAttachment.image,
				VK_IMAGE_ASPECT_COLOR_BIT,
				VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
				VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
		

		vulkanHelper->SetImageLayout(
			cmdBuf,
			brdfMap.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			subresourceRange);

		vulkanDevice->FlushCommandBuffer(cmdBuf, queue.graphicQueue);

	}
	void Prepare()
	{
		VulkanRenderer::Prepare();
		LoadScene();
		
		SetupVertexDescriptions();
		
		PrepareUniformBuffers();
		SetupDescriptorSetLayout();
		PreparePipelines();
		BuildEquirectangularToCubemap();
		BuildIrradiansMap();
		BuildPreFilterMap();
		BuildBrdfMap();
		SetupDescriptorPool();
		SetupDescriptorSet();
		RenderEnvToCube();
		RenderIrradianceMap();
		RenderPreFilterMap();
		RenderBrdfMap();
		BuildCommandBuffers();
		prepared = true;

	}
	virtual void Update()
	{
		uboVert.viewPos = camera->position;
		uboVert.viewPos= -uboVert.viewPos;
		UpdateUniformBuffers();

	}
	void DrawLoop()
	{
		//RenderBrdfMap();
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