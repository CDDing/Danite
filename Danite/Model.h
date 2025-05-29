#pragma once
#include "tiny_gltf.h"
#include "meshoptimizer.h"
struct Vertex {
	glm::vec3 position;
	float texCoordX;
	glm::vec3 normal;
	float texCoordY;
};
class Model
{
public:
	Model() {};
	void Init(std::string path);

	std::vector<Vertex> vertices;
	PFN_vkCmdDrawMeshTasksEXT vkCmdDrawMeshTasksEXT;
	//per LOD
	std::vector<std::vector<uint32_t>> indices;
	std::vector<std::vector<meshopt_Meshlet>> meshlets;
	std::vector<std::vector<unsigned int>> meshlet_vertices;
	std::vector<std::vector<unsigned char>> meshlet_triangles;

	//Buffers, per LOD
	DDing::Buffer vertexBuffer;
	std::vector<DDing::Buffer> indexBuffers;
	std::vector<DDing::Buffer> meshlet_Buffers;
	std::vector<DDing::Buffer> meshlet_vertices_Buffers;
	std::vector<DDing::Buffer> meshlet_triangles_Buffers;
	void Draw(vk::CommandBuffer commandBuffer);

	uint32_t LOD = 0;
	const uint32_t MAX_LOD = 10;
private:
	void loadFile(std::string path);
	void initBuffer();
		
	void createClusters();
	void createLODs();


};

