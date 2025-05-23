#pragma once
#include "tiny_gltf.h"
struct Vertex {
	glm::vec3 position;
	glm::vec3 normal;
	glm::vec2 texCoord;
};
class Model
{
public:
	Model() {};
	void Init(std::string path);

	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	DDing::Buffer vertexBuffer;
	DDing::Buffer indexBuffer;

	void Draw(vk::CommandBuffer commandBuffer);

	uint32_t LOD = 0;
private:
	void loadFile(std::string path);
		
	void createClusters();
	void createLODs();


};

