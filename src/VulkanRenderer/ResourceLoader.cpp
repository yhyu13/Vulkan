#include "ResourceLoader.h"

ResourceLoader::ResourceLoader(VulkanDevice* _vulkanDevice)
{
	vulkanDevice = _vulkanDevice;
}

ResourceLoader::~ResourceLoader()
{
}

VkShaderModule ResourceLoader::LoadShader(std::string fileName)
{

	size_t shaderSize;
	char* shaderCode = NULL;


	std::ifstream is(fileName, std::ios::binary | std::ios::in | std::ios::ate);

	if (is.is_open())
	{
		shaderSize = is.tellg();
		is.seekg(0, std::ios::beg);
		// Copy file contents into a buffer
		shaderCode = new char[shaderSize];
		is.read(shaderCode, shaderSize);
		is.close();
		assert(shaderSize > 0);
	}

	if (shaderCode)
	{
		// Create a new shader module that will be used for pipeline creation
		VkShaderModuleCreateInfo moduleCreateInfo{};
		moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleCreateInfo.codeSize = shaderSize;
		moduleCreateInfo.pCode = (uint32_t*)shaderCode;

		VkShaderModule shaderModule;
		
		VkResult res = vkCreateShaderModule(vulkanDevice->logicalDevice, &moduleCreateInfo, NULL, &shaderModule);
		if (res != VK_SUCCESS) {
			throw std::runtime_error("Error: Create Shader Module");
		}
		delete[] shaderCode;

		return shaderModule;
	}
	else
	{
		return VK_NULL_HANDLE;
	}

}



