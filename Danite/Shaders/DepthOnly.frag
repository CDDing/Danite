#version 450

layout(location = 0) in vec3 inWorldPos;
layout(location = 0) out float outDistance;
layout(set = 0, binding = 0) uniform GlobalBuffer {
    mat4 view;
    mat4 projection;
    vec3 cameraPosition;
    float time;
} ubo;

void main() {

    outDistance = (1,0,0,0);
}
