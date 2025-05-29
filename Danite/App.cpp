#include "pch.h"
#include "App.h"
std::unique_ptr<App> app = std::make_unique<App>();
App::App()
    : window(initWindow()), context(window)
{

}

App::~App()
{
}


void App::Init()
{
    model.Init("../Resources/dragon.gltf");
    input.Init();
    render.Init();


    
}

void App::Run()
{
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        Update();
        Render();
    }
    context.logical.waitIdle();

    glfwDestroyWindow(window);

    glfwTerminate();
}

void App::Update()
{
    input.Update();
    render.camera.Update();
}

void App::Render()
{
    render.DrawFrame();
}

GLFWwindow* App::initWindow()
{
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Don't create OpenGL context
    GLFWwindow* initializedWindow = glfwCreateWindow(1600, 900, "Vulkan Window", nullptr, nullptr);
    if (!initializedWindow) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window");
    }

    return initializedWindow;
}
