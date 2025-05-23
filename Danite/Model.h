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
	std::vector<std::vector<uint32_t>> indices;
	DDing::Buffer vertexBuffer;
	std::vector<DDing::Buffer> indexBuffer;

	void Draw(vk::CommandBuffer commandBuffer);

	uint32_t LOD = 0;
	const uint32_t MAX_LOD = 10;
private:
	void loadFile(std::string path);
	void initBuffer();
		
	void createClusters();
	void createLODs();


};

