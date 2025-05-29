#pragma once
#include "Context.h"
#include "RenderManager.h"
#include "InputManager.h"
class App
{
private:
	GLFWwindow* window;
public:
	App();
	~App();

	void Init();
	void Run();
	void Update();
	void Render();

	DDing::Context context;
	Model model;
	InputManager input;
	RenderManager render;
private:
	GLFWwindow* initWindow();
};
extern std::unique_ptr<App> app;