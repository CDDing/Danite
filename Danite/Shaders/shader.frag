#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
layout(set = 0, binding = 0) uniform GlobalBuffer {
    mat4 view;
    mat4 projection;
    mat4 transform;
    vec3 cameraPosition;
    float time;
} ubo;

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;

layout(location = 0) out vec4 outColor;


const float PI = 3.14159265359;

int DetermineFaceFromDirection(vec3 dir)
{
    vec3 absDir = abs(dir);
    
    if (absDir.x > absDir.y && absDir.x > absDir.z) {
        return dir.x > 0.0 ? 0 : 1; // 0: +X, 1: -X
    } else if (absDir.y > absDir.z) {
        return dir.y > 0.0 ? 2 : 3; // 2: +Y, 3: -Y
    } else {
        return dir.z > 0.0 ? 4 : 5; // 4: +Z, 5: -Z
    }
}
float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness*roughness;
    float a2 = a*a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;

    float nom   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ----------------------------------------------------------------------------
void main() {
    outColor = vec4(inNormal, 1.0);
}
