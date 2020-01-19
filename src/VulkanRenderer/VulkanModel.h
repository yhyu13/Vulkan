

#pragma once

#include <stdlib.h>
#include <string>
#include <fstream>
#include <vector>

#include "vulkan/vulkan.h"

#include <assimp/Importer.hpp> 
#include <assimp/scene.h>     
#include <assimp/postprocess.h>
#include <assimp/cimport.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "VulkanDevice.h"
#include "VulkanBuffer.h"
#include <array>
#include <map>
#include "Math/Quaternions.h"


// Maximum number of bones per vertex
#define MAX_BONES_PER_VERTEX 4
// Must not be higher than same const in skinning shader
#define MAX_BONES 66


	/** @brief Vertex layout components */
	typedef enum Component {
		VERTEX_COMPONENT_POSITION = 0x0,
		VERTEX_COMPONENT_NORMAL = 0x1,
		VERTEX_COMPONENT_COLOR = 0x2,
		VERTEX_COMPONENT_UV = 0x3,
		VERTEX_COMPONENT_TANGENT = 0x4,
		VERTEX_COMPONENT_BITANGENT = 0x5,
		VERTEX_COMPONENT_BONE_WEIGHT = 0x6,
		VERTEX_COMPONENT_BONE_IDS = 0x7,
		VERTEX_COMPONENT_DUMMY_FLOAT = 0x8,
		VERTEX_COMPONENT_DUMMY_VEC4 = 0x9
	} Component;

	/** @brief Stores vertex layout components for model loading and Vulkan vertex input and atribute bindings  */
	struct VertexLayout {
	public:
		/** @brief Components used to generate vertices from */
		std::vector<Component> components;

		VertexLayout(std::vector<Component> components)
		{
			this->components = std::move(components);
		}

		uint32_t stride()
		{
			uint32_t res = 0;
			for (auto& component : components)
			{
				switch (component)
				{
				case VERTEX_COMPONENT_UV:
					res += 2 * sizeof(float);
					break;
				case VERTEX_COMPONENT_DUMMY_FLOAT:
					res += sizeof(float);
					break;
				case VERTEX_COMPONENT_DUMMY_VEC4:
					res += 4 * sizeof(float);
					break;
				case VERTEX_COMPONENT_BONE_WEIGHT:
					res += 4 * sizeof(float);
					break;
				case VERTEX_COMPONENT_BONE_IDS:
					res += 4 * sizeof(float);
					break;
				default:
					// All components except the ones listed above are made up of 3 floats
					res += 3 * sizeof(float);
				}
			}
			return res;
		}
	};

	// Per-vertex bone IDs and weights
	struct VertexBoneData
	{
		int ids[MAX_BONES_PER_VERTEX];   // we have 4 bone ids for EACH vertex & 4 weights for EACH vertex
		float weights[MAX_BONES_PER_VERTEX];

		VertexBoneData()
		{
			memset(ids, 0, sizeof(ids));    // init all values in array = 0
			memset(weights, 0, sizeof(weights));
		}

		void AddBoneData(int bone_id, float weight) 
		{
			for (int i = 0; i < MAX_BONES_PER_VERTEX; i++)
			{
				if (weights[i] == 0.0)
				{
					ids[i] = bone_id;
					weights[i] = weight;
					return;
				}
			}
		}
	};

	// Stores information on a single bone
	struct BoneInfo
	{
		aiMatrix4x4 offset;
		aiMatrix4x4 finalTransformation;

		BoneInfo()
		{
			offset = aiMatrix4x4();
			finalTransformation = aiMatrix4x4();
		};
	};
	struct BoneMatrix
	{
		aiMatrix4x4 offset_matrix;
		aiMatrix4x4 final_world_transform;
		aiMatrix4x4 finalBoneTransform;
		int index;

	};
	/** @brief Used to parametrize model loading */
	struct ModelCreateInfo {
		glm::vec3 center;
		glm::vec3 scale;
		glm::vec2 uvscale;
		VkMemoryPropertyFlags memoryPropertyFlags = 0;

		ModelCreateInfo() : center(glm::vec3(0.0f)), scale(glm::vec3(1.0f)), uvscale(glm::vec2(1.0f)) {};

		ModelCreateInfo(glm::vec3 scale, glm::vec2 uvscale, glm::vec3 center)
		{
			this->center = center;
			this->scale = scale;
			this->uvscale = uvscale;
		}

		ModelCreateInfo(float scale, float uvscale, float center)
		{
			this->center = glm::vec3(center);
			this->scale = glm::vec3(scale);
			this->uvscale = glm::vec2(uvscale);
		}

	};
	class Animation
	{
	public:
		/////////////////////////////////////////
		float ticks_per_second = 0.0f;
		aiMatrix4x4 m_global_inverse_transform;
		std::vector<std::vector<VertexBoneData>> bones_id_weights_for_each_vertex;
		std::map<std::string, int> m_bone_mapping; // maps a bone name and their index
		int m_num_bones = 0;
		std::vector<BoneMatrix> m_bone_matrices;
		int vertBase = 0;

		std::vector<glm::vec3> bonePos;
		std::vector <std::vector<int>> tempCount;
		bool enableSlerp;
		bool enableIK = false;
		std::vector<std::string> boneNameIK;
		std::vector<std::string> boneNameIKFind;
		aiVector3D endEffectorPosition;
		aiVector3D initialEndEffectorPosition;
		aiVector3D targetPosition;
		bool ikAnimationComplete = false;
		int ikTotalSteps = 0;
		int ikCurrentSteps = 0;
		float temp_angle=0.0;
		glm::vec3 tarPos;
	public:
		struct  BoneIk
		{
			aiMatrix4x4 localTransform;
			aiMatrix4x4 globalTransform;
			aiMatrix4x4 parentTransform;
			aiMatrix4x4 rotation;
			aiVector3D position;
			Quaternions initailRotation = Quaternions(0.0f, 0.0f, 0.0f, 0.0f);
			const aiNodeAnim* node_anim;
			const aiNode* aiNode;
		};

		std::vector<BoneIk> boneIKList;
		Quaternions initailRotation;
	public:
		void ProcessNode(aiNode* node, const aiScene* scene);
		void ProcessMesh(aiMesh* mesh, const aiScene* scene, int meshCount);
		void BoneTransform(double time_in_sec, std::vector<aiMatrix4x4>& transforms);
		void ReadNodeHierar(float p_animation_time, const aiNode* p_node, const aiMatrix4x4 parent_transform);
		const aiNodeAnim * findNodeAnim(const aiAnimation * p_animation, const std::string p_node_name);
		aiVector3D calcInterpolatedScaling(float p_animation_time, const aiNodeAnim* p_node_anim);
		int findScaling(float p_animation_time, const aiNodeAnim* p_node_anim);
		Quaternions calcInterpolatedRotation(float p_animation_time, const aiNodeAnim* p_node_anim);
		int findRotation(float p_animation_time, const aiNodeAnim* p_node_anim);
		aiVector3D calcInterpolatedPosition(float p_animation_time, const aiNodeAnim* p_node_anim);
		int findPosition(float p_animation_time, const aiNodeAnim* p_node_anim);
		
		Quaternions nlerp(Quaternions a, Quaternions b, float blend);
		glm::mat4 aiToGlm(aiMatrix4x4 ai_matr);
		void TraverseChild(const aiNode* mRootNode);
		/////////////////////////////////////////
		//
		const aiScene* scene;
		//
		void FindIKChildren();
		// Currently active animation
		aiAnimation* pAnimation;
		void SetAnimation(uint32_t animationIndex);
		Animation();
		~Animation();

		
	};
	class Model 
	{
	public:
		//////////////////////////
	
		/////////////////
		Model();
		~Model();
		VkDevice device = nullptr;
		Buffer vertices;
		Buffer indices;
		uint32_t indexCount = 0;
		uint32_t vertexCount = 0;
		//
		Animation* animation;
		// 
		bool enableIK = false;
		//
		std::vector<std::string> boneNameIK;
		//
		bool animationEnabled = false;
	
		struct ModelPart {
			uint32_t vertexBase;
			uint32_t vertexCount;
			uint32_t indexBase;
			uint32_t indexCount;
		};
		std::vector<ModelPart> parts;

		static const int defaultFlags = aiProcess_Triangulate | aiProcess_GenSmoothNormals  | aiProcess_CalcTangentSpace | aiProcess_ConvertToLeftHanded | aiProcess_GlobalScale;
		
	

	public:
		void Destroy();
		

		
		bool LoadModelFromFile(const std::string& filename, VertexLayout layout, ModelCreateInfo *createInfo, VulkanDevice *device, VkQueue copyQueue);
		

		
		bool LoadModel(const char* filename, VertexLayout layout, float scale, VulkanDevice *device, VkQueue copyQueue);
		
	};
