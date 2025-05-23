#version 460
#extension GL_EXT_buffer_reference : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require

layout(set = 0, binding = 0) uniform GlobalBuffer {
    mat4 view;
    mat4 projection;
    mat4 transform;
    vec3 cameraPosition;
    float time;
} ubo;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec2 outUV;
layout(location = 2) out vec3 outNormal;

void main() {
    // Predefined triangle in clip space

    gl_Position = ubo.projection * ubo.view * ubo.transform * vec4(inPosition,1.0);
    
    outWorldPos = (ubo.transform * vec4(inPosition,1.0)).xyz;
    outUV = inUV;
    outNormal = (inverse(mat3(ubo.transform))) * normalize(inNormal);
}
