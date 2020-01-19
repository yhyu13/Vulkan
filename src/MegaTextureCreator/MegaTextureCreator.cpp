
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <windows.h>
#include "vulkan/vulkan.h"
#include <unordered_map>
#include "stb\stb_image.h"
#include "gli\gli.hpp"
using namespace std;

int megaTexWidth = 8192;
int megaTexHeight = 8192;
int xTextureCount = megaTexWidth / 8192;
int smallTextureWidth = 8192;
void main()
{
	std::vector <unsigned char> temp;

	std::vector<std::string> textureNames;
	for (int i = 0; i < 4; ++i)
	{
		textureNames.push_back("Mars.jpg");
	}
	char buffer[MAX_PATH];
	GetModuleFileName(NULL, buffer, MAX_PATH);
	std::string::size_type pos = std::string(buffer).find_last_of("\\/");

	std::string fileName = std::string(buffer).substr(0, pos);
	std::size_t found = fileName.find("PC Visual Studio");
	if (found != std::string::npos)
	{
		fileName.erase(fileName.begin() + found, fileName.end());
	}
	std::string tempFileAddress = fileName + "Resources\\Textures\\MegaTexture\\temp.txt";
	FILE* tempData = fopen(tempFileAddress.c_str(), "wb");
	std::string address = fileName + "Resources\\Textures\\MegaTexture\\MegaTexture.png";
	FILE* tempReadData = fopen(tempFileAddress.c_str(), "rb+");
	

	std::vector<unsigned char> imageData;

	int texWidth, texHeight, texChannels, byteCount = 0;
	unsigned char* pixels = stbi_load(address.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
	if (pixels == nullptr)
		throw(std::string("Failed to load texture"));
	

	imageData.resize(texWidth*texHeight * 4);
	
	memcpy(&imageData[0], &pixels[0], imageData.size());
	//fseek(tempReadData, 0, SEEK_SET);
	//int result = fread(imageData.data(), sizeof(char), 8192 * 8192 * 4, tempReadData);

	/*if (!result)
	{
		throw std::runtime_error("Error: fread");
	}*/
	
	for (int m = 0; m < 8; ++m)
	{ //int m = 0;
		int loopSize = pow(2, 14 - m)/ 128;
		int imageWidth = pow(2, 14 - m);
		int externalOffset = 0;
		for (int i = 0; i < loopSize; ++i)
		{
			externalOffset = i*128 * (imageWidth * 4);
			for (int j = 0; j < loopSize; ++j)
			{
				//uintptr_t stagingBufferLocation = (uintptr_t)imageData.data();
				
				for (int k = 0; k < 128; ++k)
				{
					int internalOffset = k * (imageWidth * 4);
					//uintptr_t tempSB = stagingBufferLocation + externalOffset + internalOffset;
					//fseek(tempData, 0, SEEK_END);
					fwrite((void*)(imageData.data() + externalOffset + internalOffset), 512, 1, tempData);
					//fwrite(imageData.data() +offset+ k * (imageWidth * 4), sizeof(char), 512, tempData);
					
				}
				externalOffset += 512;
				//fwrite(imageData.data() , sizeof(char), 65536, tempData);
				
			}
			
		}
		
		//edit vector data
		temp.clear();
		for (int f = 0; f < imageData.size(); f+=(imageWidth * 4*2))
		{
			for (int g = f; g < f+(imageWidth * 4); g += 8)
			{
				temp.push_back(imageData[g]);
				temp.push_back(imageData[g + 1]);
				temp.push_back(imageData[g + 2]);
				temp.push_back(imageData[g + 3]); 
			}
			/*if (f != 0 && f % (imageWidth * 4) == 0)
			{
				f += (imageWidth * 4);
			}*/
			
		}
		imageData.clear();
		imageData = temp;
	}


	//for (int i = 8; i < 15; ++i)
	//{
	//	int imageWidth = pow(2, 14 - i);
	//	int size = imageWidth * imageWidth * 4;
	//	fwrite((void*)imageData.data() , imageData.size(), 1, tempData);
	//	//edit vector data
	//	temp.clear();
	//	for (int f = 0; f < imageData.size(); f+=(imageWidth * 4*2))
	//	{

	//		for (int g = f; g < f+(imageWidth * 4); g += 8)
	//		{
	//			temp.push_back(imageData[g]);
	//			temp.push_back(imageData[g + 1]);
	//			temp.push_back(imageData[g + 2]);
	//			temp.push_back(imageData[g + 3]); 
	//		}
	//		
	//	}
	//	imageData.clear();
	//	imageData = temp;
	//}

	//fwrite(imageData.data(), sizeof(char), imageData.size(),tempData);
	
	 /*gli::texture2d tex2D(gli::load(address.c_str()));
	 uint32_t width = static_cast<uint32_t>(tex2D[0].extent().x);
	 uint32_t height = static_cast<uint32_t>(tex2D[0].extent().y);
	 uint32_t mipLevels = static_cast<uint32_t>(tex2D.levels());
	 imageData.resize(tex2D.size());
	
	 memcpy(imageData.data(), tex2D.data(), tex2D.size());
	
	 std::unordered_map<int,  std::vector<unsigned char>> imageMipDataPerPage;
	 
	 for (int i = 0; i < (int)mipLevels; i++)
	 {

	 }*/

	
	//stbi_image_free(pixels);
	fclose(tempData);
	std::cout << "Reached";
	getchar();

}