#include <vulkan/vulkan.h>
#include "VulkanRenderer.h"
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "VulkanModel.h"
#include <cstring>
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


/* The particle class represents a particle of mass that can move around in 3D space*/
class Particle
{
private:
	bool movable; // can the particle move or not ? used to pin parts of the cloth

	float mass; // the mass of the particle (is always 1 in this example)
	glm::vec3 pos; // the current position of the particle in 3D space
	glm::vec3 old_pos; // the position of the particle in the previous time step, used as part of the verlet numerical integration scheme
	glm::vec3 acceleration; // a vector representing the current acceleration of the particle
	glm::vec3 accumulated_normal; // an accumulated normal (i.e. non normalized), used for OpenGL soft shading
	glm::vec2 UV;
public:
	Particle(glm::vec3 pos, glm::vec2 uv) : pos(pos), old_pos(pos), acceleration(glm::vec3(0, 0, 0)), mass(1), movable(true), accumulated_normal(glm::vec3(0, 0, 0)), UV(uv) {}
	Particle() {}

	void addForce(glm::vec3 f)
	{
		acceleration += f / mass;
	}

	/* This is one of the important methods, where the time is progressed a single step size (TIME_STEPSIZE)
	   The method is called by Cloth.time_step()
	   Given the equation "force = mass * acceleration" the next position is found through verlet integration*/
	void timeStep()
	{
		if (movable)
		{
			glm::vec3 temp = pos;
			pos = pos + (pos - old_pos)*(1.0f - DAMPING) + acceleration * TIME_STEPSIZE2;
			old_pos = temp;
			acceleration = glm::vec3(0.0, 0.0, 0.0); // acceleration is reset since it HAS been translated into a change in position (and implicitely into velocity)	
		}
	}

	glm::vec3& getPos() { return pos; }

	glm::vec2& getUV() { return UV; }

	void resetAcceleration() { acceleration = glm::vec3(0.0, 0.0, 0.0); }

	void offsetPos(const glm::vec3 v) { if (movable) pos += v; }

	void makeUnmovable() { movable = false; }

	void addToNormal(glm::vec3 normal)
	{
		glm::normalize(normal);
		accumulated_normal += normal;
	}

	glm::vec3& getNormal() { return accumulated_normal; } // notice, the normal is not unit length

	void resetNormal() { accumulated_normal = glm::vec3(0.0, 0.0, 0.0); }

};

class Constraint
{
private:
	float rest_distance; // the length between particle p1 and p2 in rest configuration

public:
	Particle *p1, *p2; // the two particles that are connected through this constraint

	Constraint(Particle *p1, Particle *p2) : p1(p1), p2(p2)
	{
		//glm::vec3 vec = p1->getPos() - p2->getPos();
		rest_distance = glm::distance(p1->getPos(), p2->getPos());
	}

	/* This is one of the important methods, where a single constraint between two particles p1 and p2 is solved
	the method is called by Cloth.time_step() many times per frame*/
	void satisfyConstraint()
	{
		glm::vec3 p1_to_p2 = p2->getPos() - p1->getPos(); 
		float current_distance = glm::distance(p1->getPos(), p2->getPos()); 
		glm::vec3 correctionVector = p1_to_p2 * (1 - rest_distance / current_distance); 
		glm::vec3 correctionVectorHalf = correctionVector * 0.5f; 
		p1->offsetPos(correctionVectorHalf); 
		p2->offsetPos(-correctionVectorHalf); 
	}

};
class Cloth
{
public:

	int num_particles_width; // number of particles in "width" direction
	int num_particles_height; // number of particles in "height" direction
	// total number of particles is num_particles_width*num_particles_height

	std::vector<Particle> particles; // all particles that are part of this cloth
	std::vector<Constraint> constraints; // alle constraints between particles as part of this cloth
	std::vector<glm::vec2> UV; 
	Particle* getParticle(int x, int y) { return &particles[y*num_particles_width + x]; }
	void makeConstraint(Particle *p1, Particle *p2) { constraints.push_back(Constraint(p1, p2)); }


	/* A private method used by drawShaded() and addWindForcesForTriangle() to retrieve the
	normal vector of the triangle defined by the position of the particles p1, p2, and p3.
	The magnitude of the normal vector is equal to the area of the parallelogram defined by p1, p2 and p3
	*/
	glm::vec3 calcTriangleNormal(Particle *p1, Particle *p2, Particle *p3)
	{
		glm::vec3 pos1 = p1->getPos();
		glm::vec3 pos2 = p2->getPos();
		glm::vec3 pos3 = p3->getPos();

		glm::vec3 v1 = pos2 - pos1;
		glm::vec3 v2 = pos3 - pos1;

		return  glm::cross(v1,v2);
	}

	/* A private method used by windForce() to calcualte the wind force for a single triangle
	defined by p1,p2,p3*/
	void addWindForcesForTriangle(Particle *p1, Particle *p2, Particle *p3, const glm::vec3 direction)
	{
		glm::vec3 normal = calcTriangleNormal(p1, p2, p3);
		glm::normalize(normal);
		glm::vec3 d = normal;
		glm::vec3 force = normal * (glm::dot( d,direction));
		p1->addForce(force);
		p2->addForce(force);
		p3->addForce(force);
	}


public:

	Cloth() {}
	/* This is a important constructor for the entire system of particles and constraints*/
	Cloth(float width, float height, int num_particles_width, int num_particles_height) : num_particles_width(num_particles_width), num_particles_height(num_particles_height)
	{
		particles.resize(num_particles_width*num_particles_height); //I am essentially using this vector as an array with room for num_particles_width*num_particles_height particles
		UV.resize(num_particles_width*num_particles_height);

		// creating particles in a grid of particles from (0,0,0) to (width,-height,0)
		for (int x = 0; x < num_particles_width; x++)
		{
			for (int y = 0; y < num_particles_height; y++)
			{
				glm::vec3 pos = glm::vec3(width * (x / (float)num_particles_width),
					-height * (y / (float)num_particles_height),
					0.0f);
			
				particles[y*num_particles_width + x] = Particle(pos, glm::vec2(pos.x,-pos.y)); // insert particle in column x at y'th row
				
			}
		}

		// Connecting immediate neighbor particles with constraints (distance 1 and sqrt(2) in the grid)
		for (int x = 0; x < num_particles_width; x++)
		{
			for (int y = 0; y < num_particles_height; y++)
			{
				if (x < num_particles_width - 1) makeConstraint(getParticle(x, y), getParticle(x + 1, y));
				if (y < num_particles_height - 1) makeConstraint(getParticle(x, y), getParticle(x, y + 1));
				if (x < num_particles_width - 1 && y < num_particles_height - 1) makeConstraint(getParticle(x, y), getParticle(x + 1, y + 1));
				if (x < num_particles_width - 1 && y < num_particles_height - 1) makeConstraint(getParticle(x + 1, y), getParticle(x, y + 1));
			}
		}


		// Connecting secondary neighbors with constraints (distance 2 and sqrt(4) in the grid)
		for (int x = 0; x < num_particles_width; x++)
		{
			for (int y = 0; y < num_particles_height; y++)
			{
				if (x < num_particles_width - 2) makeConstraint(getParticle(x, y), getParticle(x + 2, y));
				if (y < num_particles_height - 2) makeConstraint(getParticle(x, y), getParticle(x, y + 2));
				if (x < num_particles_width - 2 && y < num_particles_height - 2) makeConstraint(getParticle(x, y), getParticle(x + 2, y + 2));
				if (x < num_particles_width - 2 && y < num_particles_height - 2) makeConstraint(getParticle(x + 2, y), getParticle(x, y + 2));
			}
		}


		// making the upper left most three and right most three particles unmovable
		for (int i = 0; i < 3; i++)
		{
			getParticle(0 + i, 0)->offsetPos(glm::vec3(0.5, 0.0, 0.0)); // moving the particle a bit towards the center, to make it hang more natural - because I like it ;)
			getParticle(0 + i, 0)->makeUnmovable();

			getParticle(0 + i, 0)->offsetPos(glm::vec3(-0.5, 0.0, 0.0)); // moving the particle a bit towards the center, to make it hang more natural - because I like it ;)
			getParticle(num_particles_width - 1 - i, 0)->makeUnmovable();
		}
	}

	

	/* this is an important methods where the time is progressed one time step for the entire cloth.
	This includes calling satisfyConstraint() for every constraint, and calling timeStep() for all particles
	*/
	void timeStep()
	{
		std::vector<Constraint>::iterator constraint;
		for (int i = 0; i < CONSTRAINT_ITERATIONS; i++) // iterate over all constraints several times
		{
			for (constraint = constraints.begin(); constraint != constraints.end(); constraint++)
			{
				(*constraint).satisfyConstraint(); // satisfy constraint.
			}
		}

		std::vector<Particle>::iterator particle;
		for (particle = particles.begin(); particle != particles.end(); particle++)
		{
			(*particle).timeStep(); // calculate the position of each particle at the next time step.
		}
	}

	/* used to add gravity (or any other arbitrary vector) to all particles*/
	void addForce(const glm::vec3 direction)
	{
		std::vector<Particle>::iterator particle;
		for (particle = particles.begin(); particle != particles.end(); particle++)
		{
			(*particle).addForce(direction); // add the forces to each particle
		}

	}

	/* used to add wind forces to all particles, is added for each triangle since the final force is proportional to the triangle area as seen from the wind direction*/
	void windForce(const glm::vec3 direction)
	{
		for (int y = 0; y < num_particles_height - 1; y++)
		{
			for (int x = 0; x < num_particles_width - 1; x++)
			{
				addWindForcesForTriangle(getParticle(x + 1, y), getParticle(x, y), getParticle(x, y + 1), direction);
				addWindForcesForTriangle(getParticle(x + 1, y + 1), getParticle(x + 1, y), getParticle(x, y + 1), direction);
			}
		}
	}

	/* used to detect and resolve the collision of the cloth with the ball.
	This is based on a very simples scheme where the position of each particle is simply compared to the sphere and corrected.
	This also means that the sphere can "slip through" if the ball is small enough compared to the distance in the grid bewteen particles
	*/
	void ballCollision(const glm::vec3 center, const float radius)
	{
		std::vector<Particle>::iterator particle;
		for (particle = particles.begin(); particle != particles.end(); particle++)
		{
			glm::vec3 v = (*particle).getPos() - center;
			float l = glm::distance((*particle).getPos(), center);
			if (l < radius) // if the particle is inside the ball
			{
				glm::normalize(v);
				(*particle).offsetPos(v*(radius - l)); // project the particle to the surface of the ball
			}
		}
	}

	
};

class UnitTest : public VulkanRenderer
{

public:

	//
	Buffer vertBuffer;
	//
	Buffer clothBuffer;
	//
	Buffer sphereBuffer;
	//
	Buffer sphereIndices;
	//
	std::vector<int> sphere_indices;
	//
	std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
	//
	VkPipelineLayout pipelineLayout;
	//
	VkPipeline pipeline;
	//
	std::vector<VkDescriptorSet> descriptorSets;
	//
	Cloth cloth;
	//
	glm::vec3 ballPos = glm::vec3(0.6f,-0.7f,0.0f);
	//
	float ballradius = 0.20f;
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

	// Data send to the shader per model
	struct PerModelData
	{
		alignas(16) glm::mat4 modelMat;
		alignas(16) glm::vec4 color;
		alignas(16) int textureTypeCount;
	}perModelData;


	UnitTest() : VulkanRenderer()
	{
		appName = "Cloth Simulation";
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
		clothBuffer.Destroy();
	}

	void LoadScene()
	{
		float posX = 0.0f;
		float posY = 0.0f;
		float posZ = 0.0f;
		AllModel allModel;
		//Load Model Globe
		allModel.modelData.LoadModel("Sphere.fbx", vertexLayout, 1.0f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("Football.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModelList.push_back(allModel);

		allModel.modelData.LoadModel("Block.obj", vertexLayout, 0.5f, vulkanDevice, queue.graphicQueue);
		allModel.materialData.albedoTexture.LoadFromFile("RustAlbedo.png", VK_FORMAT_R8G8B8A8_UNORM, vulkanDevice, vulkanHelper, queue.graphicQueue);
		allModelList.push_back(allModel);

		


		
		GameObject gameObj;
		gameObj.model = &(allModelList[0].modelData);
		gameObj.material = &(allModelList[0].materialData);
		gameObj.gameObjectDetails.color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		gameObj.transform.positionPerAxis = glm::vec3(3.0f, 3.0, 3.0);
		gameObj.transform.rotateAngelPerAxis = glm::vec3(glm::radians(0.0f), 0.0f, 0.0f);
		gameObj.transform.scalePerAxis = glm::vec3(0.1700f, 0.1700f, 0.1700f);
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
	}
	void UpdateUniformBuffers()
	{
		glm::mat4 viewMatrix = camera->view;
		uboVert.projViewMat = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 0.1f, 1000.0f) * viewMatrix;
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


		std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
		setLayoutBindings.push_back(uniformSetLayoutBinding);
		setLayoutBindings.push_back(albedoModelextureSamplerSetLayoutBinding);
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
		pPipelineLayoutCreateInfo.setLayoutCount = 2;
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
		std::string vertAddress = fileName + "src\\ClothSimulation\\Shaders\\Default.vert.spv";
		std::string fragAddress = fileName + "src\\ClothSimulation\\Shaders\\Default.frag.spv";
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




		// Example uses tw0 ubo and one combined image sampler
		std::vector<VkDescriptorPoolSize> poolSizes;
		poolSizes.push_back(uniformDescriptorPoolSize);
		poolSizes.push_back(albedoModelSamplerDescriptorPoolSize);



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



			std::vector<VkWriteDescriptorSet> writeDescriptorSets;
			writeDescriptorSets.push_back(uniforBufferWriteDescriptorSet);
			writeDescriptorSets.push_back(albedoModelSamplerWriteDescriptorSet);


			vkUpdateDescriptorSets(vulkanDevice->logicalDevice, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, NULL);
			writeDescriptorSets.clear();
		}
		

		

	}

	void DrawModels(int commandBufferCount)
	{


		for (int i = 0; i < GAME_OBJECT_COUNT-1; ++i)
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
		model = glm::scale(model, _gameObject.transform.scalePerAxis);
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.x, glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.y, glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.z, glm::vec3(0.0f, 0.0f, 1.0f));
		model = glm::translate(model, glm::vec3(_gameObject.transform.positionPerAxis.x, _gameObject.transform.positionPerAxis.y, _gameObject.transform.positionPerAxis.z));
		perModelData.modelMat = model;
		vkCmdPushConstants(drawCmdBuffers[commandBufferCount], pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);
	}
	void UpdateSphere()
	{
		float posX = (3.0f / 0.51f)*ballPos.x;
		float posY = (3.0f / 0.5f)*ballPos.y;
		float posZ = (3.0f / 0.52f)*ballPos.z;
		scene.gameObjectList[0].transform.positionPerAxis = glm::vec3(posX, posY, posZ); //ballPos*3.0f + glm::vec3(2.0f,-2.0f,0.0f);
	}
	void UpdateCloth()
	{

		cloth.addForce(glm::vec3(0, -0.02, 0.002)*TIME_STEPSIZE2); // add gravity each frame, pointing down
		cloth.windForce(glm::vec3(0.0, 0, -1.0)*TIME_STEPSIZE2);
		cloth.timeStep();
		cloth.ballCollision(ballPos, ballradius);
		std::vector<Particle>::iterator particle;
		for (particle = cloth.particles.begin(); particle != cloth.particles.end(); particle++)
		{
			(*particle).resetNormal();
		}
		std::vector<float> vertexInputData;
		//create smooth per particle normals by adding up all the (hard) triangle normals that each particle is part of
		for (int x = 0; x < cloth.num_particles_width-1; x++)
		{
			for (int y = 0; y < cloth.num_particles_height-1; y++)
			{
				glm::vec3 normal = cloth.calcTriangleNormal(cloth.getParticle(x + 1, y), cloth.getParticle(x, y), cloth.getParticle(x, y + 1));
				cloth.getParticle(x + 1, y)->addToNormal(normal);
				cloth.getParticle(x , y)->addToNormal(normal);
				cloth.getParticle(x , y+1)->addToNormal(normal);
				Particle* part = cloth.getParticle(x+1 , y);
				
				vertexInputData.push_back(part->getPos().x);
				vertexInputData.push_back(part->getPos().y);
				vertexInputData.push_back(part->getPos().z);

				vertexInputData.push_back(part->getNormal().x);
				vertexInputData.push_back(part->getNormal().y);
				vertexInputData.push_back(part->getNormal().z);

				vertexInputData.push_back(part->getUV().x);
				vertexInputData.push_back(part->getUV().y);
				////////////////////////////////////////
				part = cloth.getParticle(x , y);

				vertexInputData.push_back(part->getPos().x);
				vertexInputData.push_back(part->getPos().y);
				vertexInputData.push_back(part->getPos().z);

				vertexInputData.push_back(part->getNormal().x);
				vertexInputData.push_back(part->getNormal().y);
				vertexInputData.push_back(part->getNormal().z);

				vertexInputData.push_back(part->getUV().x);
				vertexInputData.push_back(part->getUV().y);
				/////////////////////////////////////
				part = cloth.getParticle(x, y+1);

				vertexInputData.push_back(part->getPos().x);
				vertexInputData.push_back(part->getPos().y);
				vertexInputData.push_back(part->getPos().z);

				vertexInputData.push_back(part->getNormal().x);
				vertexInputData.push_back(part->getNormal().y);
				vertexInputData.push_back(part->getNormal().z);

				vertexInputData.push_back(part->getUV().x);
				vertexInputData.push_back(part->getUV().y);
				//////////////////////////////////////////////////////////////////////

				normal = cloth.calcTriangleNormal(cloth.getParticle(x + 1, y + 1), cloth.getParticle(x + 1, y), cloth.getParticle(x, y + 1));
				cloth.getParticle(x + 1, y + 1)->addToNormal(normal);
				cloth.getParticle(x + 1, y)->addToNormal(normal);
				cloth.getParticle(x, y + 1)->addToNormal(normal);

				part = cloth.getParticle(x + 1, y + 1);

				vertexInputData.push_back(part->getPos().x);
				vertexInputData.push_back(part->getPos().y);
				vertexInputData.push_back(part->getPos().z);

				vertexInputData.push_back(part->getNormal().x);
				vertexInputData.push_back(part->getNormal().y);
				vertexInputData.push_back(part->getNormal().z);

				vertexInputData.push_back(part->getUV().x);
				vertexInputData.push_back(part->getUV().y);
				////////////////////////////////////////
				part = cloth.getParticle(x+1, y);

				vertexInputData.push_back(part->getPos().x);
				vertexInputData.push_back(part->getPos().y);
				vertexInputData.push_back(part->getPos().z);

				vertexInputData.push_back(part->getNormal().x);
				vertexInputData.push_back(part->getNormal().y);
				vertexInputData.push_back(part->getNormal().z);

				vertexInputData.push_back(part->getUV().x);
				vertexInputData.push_back(part->getUV().y);
				/////////////////////////////////////
				part = cloth.getParticle(x, y + 1);

				vertexInputData.push_back(part->getPos().x);
				vertexInputData.push_back(part->getPos().y);
				vertexInputData.push_back(part->getPos().z);

				vertexInputData.push_back(part->getNormal().x);
				vertexInputData.push_back(part->getNormal().y);
				vertexInputData.push_back(part->getNormal().z);

				vertexInputData.push_back(part->getUV().x);
				vertexInputData.push_back(part->getUV().y);

			}
		}
		
		
		memcpy(clothBuffer.mapped, vertexInputData.data(), vertexInputData.size()*sizeof(float));

		
	}
	void DrawCloth(const VkCommandBuffer commandBuffer)
	{

		glm::mat4 model = glm::mat4(1);
		//model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.0f));
		//model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
		
		
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.x, glm::vec3(1.0f, 0.0f, 0.0f));
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.y, glm::vec3(0.0f, 1.0f, 0.0f));
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.z, glm::vec3(0.0f, 0.0f, 1.0f));

		perModelData.modelMat = model;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);

		//// Draw Cloth Model
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[1], 0, NULL);

		VkDeviceSize offsets[1] = { 0 };
		
		

		//// Bind mesh vertex buffer
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &clothBuffer.buffer, offsets);

		vkCmdDraw(commandBuffer, 14405, 1,0,0);
		
	}
	void DrawSphere(const VkCommandBuffer commandBuffer)
	{

		glm::mat4 model = glm::mat4(1);
		model = glm::translate(model, ballPos);
		model = glm::scale(model, glm::vec3(0.9f, 0.9f, 0.9f));


		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.x, glm::vec3(1.0f, 0.0f, 0.0f));
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.y, glm::vec3(0.0f, 1.0f, 0.0f));
		//model = glm::rotate(model, _gameObject.transform.rotateAngelPerAxis.z, glm::vec3(0.0f, 0.0f, 1.0f));

		perModelData.modelMat = model;
		vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(perModelData), &perModelData);

		//// Draw Cloth Model
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSets[0], 0, NULL);

		VkDeviceSize offsets[1] = { 0 };

		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &sphereBuffer.buffer, offsets);
		// Bind mesh index buffer
		vkCmdBindIndexBuffer(commandBuffer, sphereIndices.buffer, 0, VK_INDEX_TYPE_UINT32);
		// Render mesh vertex buffer using it's indices
		vkCmdDrawIndexed(commandBuffer, sphere_indices.size(), 1, 0, 0, 0);
		//vkCmdDraw(commandBuffer, 2500, 1, 0, 0);
		

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
			
			DrawCloth(drawCmdBuffers[i]);
		//	DrawSphere(drawCmdBuffers[i]);
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
	void CreateCloth()
	{
		cloth =  Cloth(1, 1, 50, 50);
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&clothBuffer, 115248 * sizeof(float));
		// Map persistent
		clothBuffer.Map();
	}
	void CreateSphere()
	{

		std::vector<float> vertInput;
		
		float radius = ballradius;
		unsigned int rings = 50;
		unsigned int sectors = 50;
		float const R = 1. / (float)(rings - 1);
		float const S = 1. / (float)(sectors - 1);
		int r, s;

		std::vector<float> sphere_vertices, sphere_normals, sphere_texcoords;
		sphere_vertices.resize(rings * sectors * 3);
		sphere_normals.resize(rings * sectors * 3);
		sphere_texcoords.resize(rings * sectors * 2);
		std::vector<GLfloat>::iterator v = sphere_vertices.begin();
		std::vector<GLfloat>::iterator n = sphere_normals.begin();
		std::vector<GLfloat>::iterator t = sphere_texcoords.begin();
		for (r = 0; r < rings; r++) for (s = 0; s < sectors; s++) {
			float const y = sin(-M_PI_2 + M_PI * r * R);
			float const x = cos(2 * M_PI * s * S) * sin(M_PI * r * R);
			float const z = sin(2 * M_PI * s * S) * sin(M_PI * r * R);

			

			vertInput.push_back( x * radius);
			vertInput.push_back(y * radius);
			vertInput.push_back(z * radius);

			vertInput.push_back(x);
			vertInput.push_back(y);
			vertInput.push_back(z);

			vertInput.push_back(s * S);
			vertInput.push_back(r * R);
		}
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&sphereBuffer, vertInput.size() * sizeof(float));
		// Map persistent
		sphereBuffer.Map();
		memcpy(sphereBuffer.mapped, vertInput.data(), vertInput.size() * sizeof(float));

		
		sphere_indices.resize(rings * sectors * 4);

		std::vector<int>::iterator i = sphere_indices.begin();
		for (r = 0; r < rings; r++) for (s = 0; s < sectors; s++) {
			sphere_indices.push_back( r * sectors + s);
			sphere_indices.push_back(r * sectors + (s + 1));
			sphere_indices.push_back((r + 1) * sectors + (s + 1));
			sphere_indices.push_back((r + 1) * sectors + s);
		}
		vulkanDevice->CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&sphereIndices, sphere_indices.size() * sizeof(int));
		// Map persistent
		sphereIndices.Map();

		memcpy(sphereIndices.mapped, sphere_indices.data(), sphere_indices.size() * sizeof(int));
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
		CreateCloth();
		CreateSphere();
		BuildCommandBuffers();
		prepared = true;

	}
	virtual void Update()
	{
		UpdateUniformBuffers();
		UpdateSphere();
		UpdateCloth();
		
	}
	void DrawLoop()
	{
		BuildCommandBuffers();
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
		if (gui->InputFloat("Ball Pos X = ", &ballPos.x, 0.1f, 3))
		{

		}
		if (gui->InputFloat("Ball Pos Y = ", &ballPos.y, 0.1f, 3))
		{

		}
		if (gui->InputFloat("Ball Pos Z = ", &ballPos.z, 0.01f, 3))
		{

		}
		if (gui->InputFloat("Ball Radius ", &ballradius, 0.1f, 3))
		{

		}
		
	}
};

VULKAN_RENDERER()