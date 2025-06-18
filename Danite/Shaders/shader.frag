#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_shader_explicit_arithmetic_types_int32 : require
struct Plane {
    vec3 normal;
    float dis;
};
struct Frustum {
    Plane topFace;
    Plane bottomFace;
    Plane rightFace;
    Plane leftFace;
    Plane farFace;
    Plane nearFace;
};
layout(set = 0, binding = 0) uniform GlobalBuffer {
    mat4 view;
    mat4 projection;
    mat4 transform;
    Frustum viewFrustum;
    vec3 cameraPosition;
    uint currentLOD;
    uint totalClusters;
    float time;
} ubo;

struct Vertex {
    vec3 position;
    vec3 normal;
    vec2 uv;
    vec3 color;
};
layout(location = 0) in Vertex inVertex;

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
    vec3 lightPosition = vec3(0,0.5,0);
    vec3 albedo = inVertex.color;
    float metallic = 0.5f, roughness = 0.5f, ao = 1.0f;
    vec3 N = inVertex.normal;
    vec3 V = normalize(ubo.cameraPosition - inVertex.position);
    
    vec3 F0 = vec3(0.04); 
    F0 = mix(F0, albedo, metallic);

    vec3 finalColor = vec3(0.0);
    vec3 L = normalize(lightPosition - inVertex.position);
    float attenuation = 1.0;
    float shadow = 0.0;
    float NdotL;
        

    vec3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);   
    float G   = GeometrySmith(N, V, L, roughness);      
    vec3 F    = fresnelSchlick(max(dot(H, V), 0.0), F0);

        vec3 numerator = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; 
        vec3 specular = numerator / denominator;

        vec3 kS = F;
        vec3 kD = vec3(1.0) - kS;
        kD *= 1.0 - metallic;	  

        NdotL = max(dot(N, L), 0.0);        

        vec3 diffuse = kD * albedo / PI;
        
        vec3 radiance = vec3(1.0) * NdotL * attenuation;

        finalColor += (diffuse + specular) * radiance; 
   
    
    vec3 ambient = vec3(0.03) * albedo * ao;
    
    vec3 color = ambient + finalColor;

    // gamma correct
    color = pow(color, vec3(1.0/2.2)); 

    outColor = vec4(color, 1.0);
}
