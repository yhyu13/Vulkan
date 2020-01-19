#pragma once
#include <iostream>
#include <fstream>
#include "VulkanDevice.h"
#include "assimp/Importer.hpp" 
#include "assimp/scene.h"
#include "assimp/postprocess.h"
#include "assimp/cimport.h"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
class ResourceLoader
{
public:
	VulkanDevice* vulkanDevice;

	ResourceLoader(VulkanDevice* _vulkanDevice);
	~ResourceLoader();

	//Shader Loader
	VkShaderModule LoadShader(std::string fileName);




public:
};
