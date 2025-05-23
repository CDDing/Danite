#pragma once
#include <Windows.h>
#include <vulkan/vulkan_raii.hpp>
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/matrix_decompose.hpp>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui_stdlib.h"


#include <iostream>
#include <stdexcept>
#include <algorithm>
#include <vector>
#include <cstring>
#include <functional>
#include <cstdlib>
#include <deque>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <fstream>
#include <map>
#include <array>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <string>
#include <utility>

#define MAX_LIGHTS 4

#include "Common.h"
#include "App.h"
