#pragma once
namespace DDing {
	class Camera;
}
class InputManager
{
public:
	~InputManager();

	void Update();
	void Init();
	void DrawImGui(vk::CommandBuffer commandBuffer, uint32_t imageIndex);
private:
	void createImGuiResources();

	vk::raii::RenderPass renderPass = nullptr;
	vk::raii::Pipeline pipeline = nullptr;
	vk::raii::PipelineLayout pipelineLayout = nullptr;
	vk::raii::PipelineCache pipelineCache = nullptr;
	vk::raii::DescriptorPool descriptorPool = nullptr;
	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	std::vector<vk::raii::Framebuffer> swapChainFrameBuffers;

	friend class DDing::Camera;

	//for keyboard
	static void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
	static bool keyPressed[256];

	//for mouse
	static void CursorPosCallback(GLFWwindow* window, double xpos, double ypos);
	static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

	static double mouseX, mouseY;
	static bool mouseButtons[3]; // 0: Left, 1: Right, 2: Middle
};

