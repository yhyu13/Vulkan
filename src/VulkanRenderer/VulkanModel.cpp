#include "VulkanModel.h"

Model::Model()
{
}

Model::~Model()
{
}

void Model::Destroy()
{
	assert(device);
	vkDestroyBuffer(device, vertices.buffer, nullptr);
	vkFreeMemory(device, vertices.memory, nullptr);
	if (indices.buffer != VK_NULL_HANDLE)
	{
		vkDestroyBuffer(device, indices.buffer, nullptr);
		vkFreeMemory(device, indices.memory, nullptr);
	}
	if (animation)
	{
		delete animation;
	}
}

bool Model::LoadModelFromFile(const std::string & filename, VertexLayout layout, ModelCreateInfo * createInfo, VulkanDevice * device, VkQueue copyQueue)
{

	this->device = device->logicalDevice;

	Assimp::Importer Importer;
	const aiScene* pScene;

	// Load file
	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");

	std::string fileName = std::string(buffer).substr(0, pos);
	std::size_t found = fileName.find("PC Visual Studio");
	if (found != std::string::npos)
	{
		fileName.erase(fileName.begin() + found, fileName.end());
	}

	fileName = fileName + "Resources\\Models\\" + filename;
			
	//Importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
	pScene = Importer.ReadFile(fileName.c_str(), defaultFlags);
	//pScene = aiImportFile(fileName.c_str(), defaultFlags);
	if (!pScene)
	{
		std::string error = Importer.GetErrorString();
		throw std::runtime_error("Error: File not found");
	}


	if (pScene)
	{
		parts.clear();
		parts.resize(pScene->mNumMeshes);

		glm::vec3 scale(1.0f);
		glm::vec2 uvscale(1.0f);
		glm::vec3 center(0.0f);
		if (createInfo)
		{
			scale = createInfo->scale;
			uvscale = createInfo->uvscale;
			center = createInfo->center;
		}

		std::vector<float> vertexBuffer;
		std::vector<uint32_t> indexBuffer;

		animationEnabled = false;
		
		if (pScene->HasAnimations())
		{
			animationEnabled = true;
			animation = new Animation();
			if (enableIK)
			{
				animation->boneNameIK = boneNameIK;
				animation->boneNameIKFind = boneNameIK;
			}
			//GetOrphanedScene()
			//animation->scene = const_cast<aiScene*>(pScene);
			animation->scene = Importer.GetOrphanedScene();
			animation->SetAnimation(0);

			uint32_t vertexCount(0);
			for (uint32_t m = 0; m < pScene->mNumMeshes; m++) 
			{
				vertexCount += pScene->mMeshes[m]->mNumVertices;
			}

			animation->m_global_inverse_transform = pScene->mRootNode->mTransformation;

			if (pScene->mAnimations[0]->mTicksPerSecond != 0.0)
			{
				animation->ticks_per_second = pScene->mAnimations[0]->mTicksPerSecond;
			}
			else
			{
				animation->ticks_per_second = 25.0f;
			}
			animation->ProcessNode(pScene->mRootNode, pScene);
			uint32_t vertBase(0);
		

		}
		vertexCount = 0;
		indexCount = 0;
		uint32_t vertexBase(0);
		// Load meshes
		for (unsigned int i = 0; i < pScene->mNumMeshes; i++)
		{
			const aiMesh* paiMesh = pScene->mMeshes[i];

			parts[i] = {};
			parts[i].vertexBase = vertexCount;
			parts[i].indexBase = indexCount;

			vertexCount += pScene->mMeshes[i]->mNumVertices;

			aiColor3D pColor(0.f, 0.f, 0.f);
			pScene->mMaterials[paiMesh->mMaterialIndex]->Get(AI_MATKEY_COLOR_DIFFUSE, pColor);

			const aiVector3D Zero3D(0.0f, 0.0f, 0.0f);

			for (unsigned int j = 0; j < paiMesh->mNumVertices; j++)
			{
				const aiVector3D* pPos = &(paiMesh->mVertices[j]);
				const aiVector3D* pNormal = &(paiMesh->mNormals[j]);
				const aiVector3D* pTexCoord = (paiMesh->HasTextureCoords(0)) ? &(paiMesh->mTextureCoords[0][j]) : &Zero3D;
				const aiVector3D* pTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mTangents[j]) : &Zero3D;
				const aiVector3D* pBiTangent = (paiMesh->HasTangentsAndBitangents()) ? &(paiMesh->mBitangents[j]) : &Zero3D;
			
				
				for (auto& component : layout.components)
				{
					switch (component) {
					case VERTEX_COMPONENT_POSITION:
						vertexBuffer.push_back(pPos->x * scale.x);
						vertexBuffer.push_back(pPos->y * scale.y);
						vertexBuffer.push_back(pPos->z * scale.z);
						break;
					case VERTEX_COMPONENT_NORMAL:
						vertexBuffer.push_back(pNormal->x);
						vertexBuffer.push_back(pNormal->y);
						vertexBuffer.push_back(pNormal->z);
						break;
					case VERTEX_COMPONENT_UV:
						vertexBuffer.push_back(pTexCoord->x );
						vertexBuffer.push_back(pTexCoord->y );
						break;
					case VERTEX_COMPONENT_COLOR:
						vertexBuffer.push_back(pColor.r);
						vertexBuffer.push_back(pColor.g);
						vertexBuffer.push_back(pColor.b);
						break;
					case VERTEX_COMPONENT_TANGENT:
						vertexBuffer.push_back(pTangent->x);
						vertexBuffer.push_back(pTangent->y);
						vertexBuffer.push_back(pTangent->z);
						break;
					case VERTEX_COMPONENT_BITANGENT:
						vertexBuffer.push_back(pBiTangent->x);
						vertexBuffer.push_back(pBiTangent->y);
						vertexBuffer.push_back(pBiTangent->z);
						break;
					case VERTEX_COMPONENT_BONE_WEIGHT:
						if (animationEnabled)
						{
							/*glm::vec4 temp;
							temp.x = animation->bones[vertexBase + j].weights[0];
							temp.y = animation->bones[vertexBase + j].weights[1];
							temp.z = animation->bones[vertexBase + j].weights[2];
							temp.w = animation->bones[vertexBase + j].weights[3];

							glm::normalize(temp);
							vertexBuffer.push_back(temp.x);
							vertexBuffer.push_back(temp.y);
							vertexBuffer.push_back(temp.z);
							vertexBuffer.push_back(temp.w);*/
							// Fetch bone weights and IDs
							for (uint32_t k = 0; k < MAX_BONES_PER_VERTEX; k++)
							{
								
								//vertexBuffer.push_back((float)animation->bones[vertexBase+ j].weights[k]);
								vertexBuffer.push_back(animation->bones_id_weights_for_each_vertex[i][j].weights[k]);
							}
							break;
						}
						for (uint32_t k = 0; k < MAX_BONES_PER_VERTEX; k++)
						{
							vertexBuffer.push_back(0.0f);
						}
						
						break;

					case VERTEX_COMPONENT_BONE_IDS:
						if (animationEnabled)
						{
							
							for (uint32_t k = 0; k < MAX_BONES_PER_VERTEX; k++)
							{
								
								vertexBuffer.push_back(float( animation->bones_id_weights_for_each_vertex[i][j].ids[k]));
								//vertexBuffer.push_back((float)animation->bones[vertexBase + j].IDs[k]);
								
							}
							break;
						}
						for (uint32_t k = 0; k < MAX_BONES_PER_VERTEX; k++)
						{
							vertexBuffer.push_back(0.0f);
					
						}
						
						break;
						// Dummy components for padding
					case VERTEX_COMPONENT_DUMMY_FLOAT:
						vertexBuffer.push_back(0.0f);
						break;
					case VERTEX_COMPONENT_DUMMY_VEC4:
						vertexBuffer.push_back(0.0f);
						vertexBuffer.push_back(0.0f);
						vertexBuffer.push_back(0.0f);
						vertexBuffer.push_back(0.0f);
						break;
					};
				}

			
			}

			

		
			parts[i].vertexCount = paiMesh->mNumVertices;

			vertexBase += parts[i].vertexCount;

			uint32_t indexBase = static_cast<uint32_t>(indexBuffer.size());
			
			for (unsigned int j = 0; j < paiMesh->mNumFaces; j++)
			{
				const aiFace& Face = paiMesh->mFaces[j];
				if (Face.mNumIndices != 3)
					continue;
				indexBuffer.push_back(indexBase + Face.mIndices[0]);
				indexBuffer.push_back(indexBase + Face.mIndices[1]);
				indexBuffer.push_back(indexBase + Face.mIndices[2]);
				parts[i].indexCount += 3;
				indexCount += 3;
			}
			
			
		}


		uint32_t vBufferSize = static_cast<uint32_t>(vertexBuffer.size()) * sizeof(float);
		uint32_t iBufferSize = static_cast<uint32_t>(indexBuffer.size()) * sizeof(uint32_t);

		// Use staging buffer to move vertex and index buffer to device local memory
		// Create staging buffers
		Buffer vertexStaging, indexStaging;

		// Vertex buffer
		device->CreateBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&vertexStaging,
			vBufferSize,
			vertexBuffer.data());

		// Index buffer
		device->CreateBuffer(
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&indexStaging,
			iBufferSize,
			indexBuffer.data());

		// Create device local target buffers
		// Vertex buffer
		device->CreateBuffer(
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | createInfo->memoryPropertyFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&vertices,
			vBufferSize);

		// Index buffer
		device->CreateBuffer(
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | createInfo->memoryPropertyFlags,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&indices,
			iBufferSize);

		// Copy from staging buffers
		VkCommandBuffer copyCmd = device->CreateCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, true);

		VkBufferCopy copyRegion{};

		copyRegion.size = vertices.size;
		vkCmdCopyBuffer(copyCmd, vertexStaging.buffer, vertices.buffer, 1, &copyRegion);

		copyRegion.size = indices.size;
		vkCmdCopyBuffer(copyCmd, indexStaging.buffer, indices.buffer, 1, &copyRegion);

		device->FlushCommandBuffer(copyCmd, copyQueue);

		// Destroy staging resources
		vkDestroyBuffer(device->logicalDevice, vertexStaging.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, vertexStaging.memory, nullptr);
		vkDestroyBuffer(device->logicalDevice, indexStaging.buffer, nullptr);
		vkFreeMemory(device->logicalDevice, indexStaging.memory, nullptr);
		
		


		return true;
	}
	else
	{
		printf("Error parsing '%s': '%s'\n", filename.c_str(), Importer.GetErrorString());

		return false;
	}
}

bool Model::LoadModel(const char * filename, VertexLayout layout, float scale, VulkanDevice * device, VkQueue copyQueue)
{
	ModelCreateInfo modelCreateInfo(scale, 1.0f, 0.0f);
	return LoadModelFromFile(filename, layout, &modelCreateInfo, device, copyQueue);
}

void Animation::ProcessNode(aiNode * node, const aiScene * scene)
{
	//Mesh mesh;
	bones_id_weights_for_each_vertex.resize(scene->mNumMeshes);
	for (int i = 0; i < scene->mNumMeshes; i++)
	{
		aiMesh* ai_mesh = scene->mMeshes[i];
		 ProcessMesh(ai_mesh, scene, i);
		//meshes.push_back(mesh); //accumulate all meshes in one vector
	}
}

void Animation::ProcessMesh(aiMesh * mesh, const aiScene * scene, int meshCount)
{
	
	bones_id_weights_for_each_vertex[meshCount].resize(mesh->mNumVertices);

	// load bones
	for (int i = 0; i < mesh->mNumBones; i++)
	{
		int bone_index = 0;
		std::string bone_name(mesh->mBones[i]->mName.data);

		
		
		if (m_bone_mapping.find(bone_name) == m_bone_mapping.end())
		{
			// Allocate an index for a new bone
			bone_index = m_num_bones;
			m_num_bones++;
			BoneMatrix bi;
			m_bone_matrices.push_back(bi);
			
			//aiMatrix4x4 temp = mesh->mBones[i]->mOffsetMatrix.Transpose().Inverse();;
			m_bone_matrices[bone_index].offset_matrix = mesh->mBones[i]->mOffsetMatrix;
			
			m_bone_mapping[bone_name] = bone_index;

			
		}
		else
		{
			bone_index = m_bone_mapping[bone_name];
		}

		for (int j = 0; j < mesh->mBones[i]->mNumWeights; j++)
		{
			int vertex_id =   mesh->mBones[i]->mWeights[j].mVertexId; 
			float weight = mesh->mBones[i]->mWeights[j].mWeight;
			bones_id_weights_for_each_vertex[meshCount][vertex_id].AddBoneData(bone_index, weight); 

			
		}
	}
	
}

void Animation::BoneTransform(double time_in_sec, std::vector<aiMatrix4x4>& transforms)
{
	aiMatrix4x4 identity_matrix; // = mat4(1.0f);
	
	double time_in_ticks = time_in_sec * ticks_per_second;
	float animation_time = fmod(time_in_ticks, scene->mAnimations[0]->mDuration); 

	tempCount.clear();
	tempCount.resize(53);

	
	ReadNodeHierar(animation_time, scene->mRootNode, identity_matrix);
	

	transforms.resize(m_num_bones);

	for (int i = 0; i < m_num_bones; i++)
	{
		transforms[i] = m_bone_matrices[i].final_world_transform;
	}
	/////////////////////////////////////////
	if (enableIK)
	{
		if (ikCurrentSteps == 0)
		{
			ReadNodeHierar(animation_time, scene->mRootNode, identity_matrix);
			for (int i = 0; i < boneIKList.size(); ++i)
			{
				//initailRotation = calcInterpolatedRotation(animation_time, boneIKList[i].node_anim);

				//aiMatrix4x4 rotate_matr = aiMatrix4x4(rotate_quat.GetMatrix());
				//boneIKList[i].rotation = rotate_matr;
				//boneIKList[i].position = calcInterpolatedPosition(animation_time, boneIKList[i].node_anim);
				//boneIKList[i].initailRotation  = calcInterpolatedRotation(animation_time, boneIKList[i].node_anim);
			}
			//aiVector3D translate_vector = calcInterpolatedPosition(animation_time, boneIKList[boneIKList.size() - 2].node_anim);
			endEffectorPosition = aiVector3D(boneIKList[boneIKList.size() - 1].globalTransform.a4, boneIKList[boneIKList.size() - 1].globalTransform.b4, boneIKList[boneIKList.size() - 1].globalTransform.c4);
			initialEndEffectorPosition = endEffectorPosition;
			//initialEndEffectorPosition.Normalize();
		}
		else
		{
			temp_angle += 0.1f;// glm::radians(tarPos.x);
			//aiMatrix4x4 rotate, newRotate;
			//rotate.IsIdentity();
			//newRotate.IsIdentity();
			//////aiVector3D pos_temp = aiVector3D(boneIKList[boneIKList.size() - 2].globalTransform.a4, boneIKList[boneIKList.size() - 2].globalTransform.b4, boneIKList[boneIKList.size() - 2].globalTransform.c4);
			/////pos_temp.Normalize();
			//////pos_temp *= 5.0f;
			/////aiVector3D pos = aiVector3D(boneIKList[boneIKList.size() - 1].globalTransform.a4, boneIKList[boneIKList.size() - 1].globalTransform.b4, boneIKList[boneIKList.size() - 1].globalTransform.c4);
			/////pos.Normalize();
			//////pos *= 5.0f;
			//aiMatrix4x4 posMat = boneIKList[boneIKList.size() - 2].globalTransform;
			//posMat.Transpose();
			//glm::vec3 pos_bone = glm::vec3(boneIKList[boneIKList.size() - 2].globalTransform.a4, boneIKList[boneIKList.size() - 2].globalTransform.b4, boneIKList[boneIKList.size() - 2].globalTransform.c4);
			//												glm::vec3 pos_1 = glm::vec3( 14.3278198,  43.2889786, 83.3064194);
			//												glm::vec3 pos_2 = tarPos+ glm::vec3(14.3278198,  43.2889786, 83.3064194);
			//glm::vec3 B = pos_1 - pos_bone;
			//glm::vec3 C = pos_2 - pos_bone;
			//glm::vec3 A = glm::normalize(glm::cross(B, C));
			//glm::vec3 com = glm::vec3(0.0f);
			//com.x = std::fabs(B.x - C.x);
			//com.y = std::fabs(B.y - C.y);
			//com.z = std::fabs(B.z - C.z);
			//if (((com.x < 0.10f) && (com.y < 0.10f) && (com.z < 0.10f)))
			//{
			//	
			//}
			//else
			//{
			//	glm::vec3 da = glm::normalize(B);
			//	glm::vec3 db = glm::normalize(C);
			//	float angle = glm::acos(glm::dot(da, db));
			//	aiVector3D axis =  aiVector3D(A.x, A.y, A.z);
			//	//aiMatrix4x4 inverseGlobal = boneIKList[0].globalTransform;
			//	//inverseGlobal.Transpose().Inverse();
			//	//aiVector3D A_dash = inverseGlobal * axis;
			//	//aiVector3D axis = aiVector3D(1.0, 0.0, 0.0);
			//	axis.Normalize();
			//	rotate.Rotation(temp_angle, axis, newRotate);
			//	
			//	boneIKList[0].rotation = newRotate;
			//	ReadNodeHierar(animation_time, scene->mRootNode, identity_matrix);
			//}
			//
			ikTotalSteps = 1;
			if (ikCurrentSteps > ikTotalSteps)
			{
				//return;
				//ikCurrentSteps = 0;
			}
			
			aiVector3D currentEndEffectorPosition = aiVector3D(boneIKList[boneIKList.size() - 1].globalTransform.a4, boneIKList[boneIKList.size() - 1].globalTransform.b4, boneIKList[boneIKList.size() - 1].globalTransform.c4);
			endEffectorPosition = ((1.0f - ((float)ikCurrentSteps / (float)ikTotalSteps))*initialEndEffectorPosition) + (((float)ikCurrentSteps / (float)ikTotalSteps) * (aiVector3D(tarPos.x, tarPos.y, tarPos.z)+ initialEndEffectorPosition));

			glm::vec3 compare = glm::vec3(0.0f);
			compare.x = std::fabs(endEffectorPosition.x - currentEndEffectorPosition.x);
			compare.y = std::fabs(endEffectorPosition.y - currentEndEffectorPosition.y);
			compare.z = std::fabs(endEffectorPosition.z - currentEndEffectorPosition.z);

			

			//if (((compare.x > 0.001f) || (compare.y > 0.001f) || (compare.z > 0.001f)))
			{
				
				for (int i =  0;i< boneIKList.size() -1; ++i)
				{
					aiVector3D origin = aiVector3D(boneIKList[i].globalTransform.a4, boneIKList[i].globalTransform.b4, boneIKList[i].globalTransform.c4);
					aiVector3D B = currentEndEffectorPosition - origin;
					aiVector3D C = endEffectorPosition - origin;
					glm::vec3 com = glm::vec3(0.0f);
					com.x = std::fabs(B.x - C.x);
					com.y = std::fabs(B.y - C.y);
					com.z = std::fabs(B.z - C.z);
					if (((com.x < 0.10f) && (com.y < 0.10f) && (com.z < 0.10f)))
					{
						continue;
					}
					glm::vec3 _B, _C, _A;
					_B.x = B.x;
					_B.y = B.y;
					_B.z = B.z;

					_C.x = C.x;
					_C.y = C.y;
					_C.z = C.z;
					_A = glm::normalize(glm::cross(_B, _C));
					glm::vec3 da = glm::normalize(_B);
					glm::vec3 db = glm::normalize(_C);
					float angle =  glm::acos(glm::dot(da, db));
					if (glm::degrees(angle) > 0.001)
					{
						aiVector3D A;
						A.x = _A.x;
						A.y = _A.y;
						A.z = _A.z;

						aiMatrix4x4 rotate, newRotate;
						rotate.IsIdentity();
						newRotate.IsIdentity();
						aiMatrix4x4 inverseGlobal = boneIKList[i].globalTransform;
						inverseGlobal.Transpose();
						aiVector3D A_dash = inverseGlobal * A;
						A_dash.Normalize();
						rotate.Rotation(angle, A_dash, newRotate);
						boneIKList[i].rotation *= newRotate;
						//ReadNodeHierar(animation_time, scene->mRootNode, identity_matrix);
						ReadNodeHierar(animation_time, boneIKList[i].aiNode, boneIKList[i].parentTransform);
						currentEndEffectorPosition = aiVector3D(boneIKList[boneIKList.size() - 1].globalTransform.a4, boneIKList[boneIKList.size() - 1].globalTransform.b4, boneIKList[boneIKList.size() - 1].globalTransform.c4);

					}
					
					
				}
				
				compare.x = std::fabs(endEffectorPosition.x - currentEndEffectorPosition.x);
				compare.y = std::fabs(endEffectorPosition.y - currentEndEffectorPosition.y);
				compare.z = std::fabs(endEffectorPosition.z - currentEndEffectorPosition.z);
				

			}
			
			




			
		}
		
		ikCurrentSteps = 1;
	}
	
}

void Animation::ReadNodeHierar(float p_animation_time, const aiNode* p_node, const aiMatrix4x4 parent_transform)
{
	std::string node_name(p_node->mName.data);


	const aiAnimation* animation = scene->mAnimations[0];
	aiMatrix4x4 node_transform = p_node->mTransformation;

	const aiNodeAnim* node_anim = findNodeAnim(animation, node_name); // ����� ������� �� ����� ����

	bool found = false;
	int foundCount = -1;
	if (node_anim)
	{
		Quaternions rotate_quat;
		aiVector3D translate_vector;
		aiMatrix4x4 rotate_matr;
		aiMatrix4x4 rotate_matrIK;
		rotate_matr.IsIdentity();
		rotate_matrIK.IsIdentity();
		aiMatrix4x4 translate_matr;
		
		if (enableIK)
		{
			
			for (int i = 0; i < boneNameIK.size() ; ++i)
			{
	
				if (boneNameIK[i]  == node_name)
				{
					//rotate_quat = calcInterpolatedRotation(p_animation_time, node_anim);
					//rotate_matr = aiMatrix4x4(rotate_quat.GetMatrix());
					if(i != boneNameIK.size()-1)
					rotate_matrIK = boneIKList[i].rotation;
					
					found = true;
					foundCount = i;
					break;
				}
				
					
			}

			
				
	
			int key_temp = 0;
			//scaling
			//aiVector3D scaling_vector = node_anim->mScalingKeys[key_temp].mValue;
			aiVector3D scaling_vector = calcInterpolatedScaling(p_animation_time, node_anim);
			aiMatrix4x4 scaling_matr;
			aiMatrix4x4::Scaling(scaling_vector, scaling_matr);
			translate_vector = calcInterpolatedPosition(p_animation_time, node_anim);
			aiMatrix4x4::Translation(translate_vector, translate_matr);
			rotate_quat = calcInterpolatedRotation(p_animation_time, node_anim);
			rotate_matr = aiMatrix4x4(rotate_quat.GetMatrix());
			
			/*if (boneNameIK[boneNameIK.size() - 1] == node_name)
			{
				found = true;

				foundCount = boneNameIK.size() - 1;




			}*/
			if (found )
			{
				int bone_index;
				bone_index = m_bone_mapping[node_name];
				boneIKList[foundCount].localTransform = translate_matr * rotate_matr * scaling_matr * rotate_matrIK;
				boneIKList[foundCount].globalTransform =  parent_transform * boneIKList[foundCount].localTransform;
				boneIKList[foundCount].parentTransform = parent_transform;
				//rotate_quat = calcInterpolatedRotation(p_animation_time, node_anim);
				//rotate_matr = aiMatrix4x4(rotate_quat.GetMatrix());
			}
			

			
			node_transform = translate_matr * rotate_matr * scaling_matr *rotate_matrIK;
			
		}
		else
		{
			int key_temp = 0;
			//scaling
			//aiVector3D scaling_vector = node_anim->mScalingKeys[key_temp].mValue;
			aiVector3D scaling_vector = calcInterpolatedScaling(p_animation_time, node_anim);
			aiMatrix4x4 scaling_matr;
			aiMatrix4x4::Scaling(scaling_vector, scaling_matr);

			//rotation
			//aiQuaternion rotate_quat = node_anim->mRotationKeys[key_temp].mValue;

			//aiQuaternion rotate_quat = calcInterpolatedRotation(p_animation_time, node_anim);
			Quaternions rotate_quat = calcInterpolatedRotation(p_animation_time, node_anim);

			aiMatrix4x4 rotate_matr = aiMatrix4x4(rotate_quat.GetMatrix());

			//rotate_matr.Inverse().Transpose();
			//translation
			//aiVector3D translate_vector = node_anim->mPositionKeys[key_temp].mValue;
			aiVector3D translate_vector = calcInterpolatedPosition(p_animation_time, node_anim);
			aiMatrix4x4 translate_matr;
			aiMatrix4x4::Translation(translate_vector, translate_matr);


			node_transform = translate_matr * rotate_matr * scaling_matr;
		}
		
		
			
	}

	aiMatrix4x4 global_transform = parent_transform *  node_transform;

	int bone_index;
	if (m_bone_mapping.find(node_name) != m_bone_mapping.end()) 
	{
		 bone_index = m_bone_mapping[node_name];
	
		m_bone_matrices[bone_index].final_world_transform = m_global_inverse_transform * global_transform * m_bone_matrices[bone_index].offset_matrix;
		m_bone_matrices[bone_index].finalBoneTransform = m_global_inverse_transform * global_transform;
		m_bone_matrices[bone_index].index = bone_index;
	
		for (int i = 0; i < p_node->mNumChildren; i++)
		{
			std::string nodeName(p_node->mChildren[i]->mName.data);
			if (m_bone_mapping.find(nodeName) != m_bone_mapping.end())
			{
				int boneIndex = m_bone_mapping[nodeName];
				tempCount[bone_index].push_back(boneIndex);
			}
			else
			{
				tempCount[bone_index].push_back(bone_index);
			}
		}
	}
	
		
	
	
	
		
	
	

	for (int i = 0; i < p_node->mNumChildren; i++)
	{
		
		ReadNodeHierar(p_animation_time, p_node->mChildren[i], global_transform);
	
	}
}

const aiNodeAnim * Animation::findNodeAnim(const aiAnimation * p_animation, const std::string p_node_name)
{
	
	for (int i = 0; i < p_animation->mNumChannels; i++)
	{
		const aiNodeAnim* node_anim = p_animation->mChannels[i]; 
		
		if (std::string(node_anim->mNodeName.data) == p_node_name)
		{
			return node_anim;
		}
	}

	return nullptr;
}

aiVector3D Animation::calcInterpolatedScaling(float p_animation_time, const aiNodeAnim * p_node_anim)
{
	if (p_node_anim->mNumScalingKeys == 1) 
	{
		return p_node_anim->mScalingKeys[0].mValue;
	}

	int scaling_index = findScaling(p_animation_time, p_node_anim); 
	int next_scaling_index = scaling_index + 1;
	assert(next_scaling_index < p_node_anim->mNumScalingKeys);
	float delta_time = (float)(p_node_anim->mScalingKeys[next_scaling_index].mTime - p_node_anim->mScalingKeys[scaling_index].mTime);
	float  factor = (p_animation_time - (float)p_node_anim->mScalingKeys[scaling_index].mTime) / delta_time;
	assert(factor >= 0.0f && factor <= 1.0f);
	aiVector3D start = p_node_anim->mScalingKeys[scaling_index].mValue;
	aiVector3D end = p_node_anim->mScalingKeys[next_scaling_index].mValue;
	aiVector3D delta = end - start;

	return start + factor * delta;
}

int Animation::findScaling(float p_animation_time, const aiNodeAnim * p_node_anim)
{
	
	for (int i = 0; i < p_node_anim->mNumScalingKeys - 1; i++) 
	{
		if (p_animation_time < (float)p_node_anim->mScalingKeys[i + 1].mTime) 
		{
			return i;
		}
	}

	assert(0);
	return 0;
}

Quaternions Animation::calcInterpolatedRotation(float p_animation_time, const aiNodeAnim * p_node_anim)
{
	if (p_node_anim->mNumRotationKeys == 1)
	{
		Quaternions result;
		result.x = p_node_anim->mRotationKeys[0].mValue.x;
		result.y = p_node_anim->mRotationKeys[0].mValue.y;
		result.z = p_node_anim->mRotationKeys[0].mValue.z;
		result.w = p_node_anim->mRotationKeys[0].mValue.w;
		return result;
	}

	int rotation_index = findRotation(p_animation_time, p_node_anim); 
	int next_rotation_index = rotation_index + 1;
	assert(next_rotation_index < p_node_anim->mNumRotationKeys);
	
	float delta_time = (float)(p_node_anim->mRotationKeys[next_rotation_index].mTime - p_node_anim->mRotationKeys[rotation_index].mTime);
	
	float factor = (p_animation_time - (float)p_node_anim->mRotationKeys[rotation_index].mTime) / delta_time;



	assert(factor >= 0.0f && factor <= 1.0f);
	Quaternions start_quat;
	start_quat.x = p_node_anim->mRotationKeys[rotation_index].mValue.x;
	start_quat.y = p_node_anim->mRotationKeys[rotation_index].mValue.y;
	start_quat.z = p_node_anim->mRotationKeys[rotation_index].mValue.z;
	start_quat.w = p_node_anim->mRotationKeys[rotation_index].mValue.w;

	Quaternions end_quat;
	end_quat.x = p_node_anim->mRotationKeys[next_rotation_index].mValue.x;
	end_quat.y = p_node_anim->mRotationKeys[next_rotation_index].mValue.y;
	end_quat.z = p_node_anim->mRotationKeys[next_rotation_index].mValue.z;
	end_quat.w = p_node_anim->mRotationKeys[next_rotation_index].mValue.w;

	if (enableSlerp)
	{
		return start_quat.Slerp(end_quat, factor);
	}
	else
	{
		return start_quat.nLerp(end_quat, factor);
	}

}

int Animation::findRotation(float p_animation_time, const aiNodeAnim * p_node_anim)
{
	
	for (int i = 0; i < p_node_anim->mNumRotationKeys - 1; i++) 
	{
		if (p_animation_time < (float)p_node_anim->mRotationKeys[i + 1].mTime) 
		{
			return i; 
		}
	}

	assert(0);
	return 0;
}

aiVector3D Animation::calcInterpolatedPosition(float p_animation_time, const aiNodeAnim * p_node_anim)
{
	if (p_node_anim->mNumPositionKeys == 1) 
	{
		return p_node_anim->mPositionKeys[0].mValue;
	}

	int position_index = findPosition(p_animation_time, p_node_anim); 
	int next_position_index = position_index + 1;
	assert(next_position_index < p_node_anim->mNumPositionKeys);
	
	float delta_time = (float)(p_node_anim->mPositionKeys[next_position_index].mTime - p_node_anim->mPositionKeys[position_index].mTime);
	
	float factor = (p_animation_time - (float)p_node_anim->mPositionKeys[position_index].mTime) / delta_time;
	assert(factor >= 0.0f && factor <= 1.0f);
	aiVector3D start = p_node_anim->mPositionKeys[position_index].mValue;
	aiVector3D end = p_node_anim->mPositionKeys[next_position_index].mValue;
	aiVector3D delta = end - start;

	return start + factor * delta;
}

int Animation::findPosition(float p_animation_time, const aiNodeAnim * p_node_anim)
{
	
	for (int i = 0; i < p_node_anim->mNumPositionKeys - 1; i++) 
	{
		if (p_animation_time < (float)p_node_anim->mPositionKeys[i + 1].mTime) 
		{
			return i; 
		}
	}

	assert(0);
	return 0;
}



glm::mat4 Animation::aiToGlm(aiMatrix4x4 ai_matr)
{
	glm::mat4 result;
	result[0].x = ai_matr.a1; result[0].y = ai_matr.b1; result[0].z = ai_matr.c1; result[0].w = ai_matr.d1;
	result[1].x = ai_matr.a2; result[1].y = ai_matr.b2; result[1].z = ai_matr.c2; result[1].w = ai_matr.d2;
	result[2].x = ai_matr.a3; result[2].y = ai_matr.b3; result[2].z = ai_matr.c3; result[2].w = ai_matr.d3;
	result[3].x = ai_matr.a4; result[3].y = ai_matr.b4; result[3].z = ai_matr.c4; result[3].w = ai_matr.d4;

	

	return result;
}
void Animation::TraverseChild(const aiNode* mRootNode)
{
	std::string node_name(mRootNode->mName.data);
	const aiAnimation* animation = scene->mAnimations[0];
	if (ikCurrentSteps == 0)
	{
		for (int i = 0; i < boneNameIKFind.size(); ++i)
		{
			//std::size_t found = boneNameIK[i].compare(node_name);
			if (boneNameIKFind[i] == node_name )
			{
				BoneIk boneIK;

				boneIK.node_anim = findNodeAnim(animation, node_name);
				boneIK.aiNode = mRootNode;
				boneIKList.push_back(boneIK);
				boneNameIKFind.erase(boneNameIKFind.begin() + i);
				
			}
		}
	}
	
	for (int i = 0; i < mRootNode->mNumChildren; i++)
	{

		TraverseChild(mRootNode->mChildren[i]);

	}
}
void Animation::FindIKChildren()
{
	TraverseChild( scene->mRootNode);
}

void Animation::SetAnimation(uint32_t animationIndex)
{
	if (animationIndex < scene->mNumAnimations)
	{
		pAnimation = scene->mAnimations[animationIndex];
	}
	else
	{
		throw std::runtime_error("Error: Anaimtion Fail");
	}
}

Animation::Animation()
{
}
Animation::~Animation()
{
	
}

Quaternions Animation::nlerp(Quaternions a, Quaternions b, float blend)
{

	a.Normalize();
	b.Normalize();

	Quaternions result;
	float dot_product = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
	float one_minus_blend = 1.0f - blend;

	if (dot_product < 0.0f)
	{
		result.x = a.x * one_minus_blend + blend * -b.x;
		result.y = a.y * one_minus_blend + blend * -b.y;
		result.z = a.z * one_minus_blend + blend * -b.z;
		result.w = a.w * one_minus_blend + blend * -b.w;
	}
	else
	{
		result.x = a.x * one_minus_blend + blend * b.x;
		result.y = a.y * one_minus_blend + blend * b.y;
		result.z = a.z * one_minus_blend + blend * b.z;
		result.w = a.w * one_minus_blend + blend * b.w;
	}

	return result.Normalized();
}
