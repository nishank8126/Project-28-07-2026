#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;

layout(push_constant) uniform PushConstants {
    mat4 model;
    mat4 view;
    mat4 projection;
    vec4 lightDir;    // xyz = light direction, w = surface alpha
    vec3 cameraPos;
    vec4 material; // x=ambient, y=diffuse, z=specular, w=shininess
    vec4 shadingParams; // x=mode, y=depthMin, z=depthMax, w=edlStrength
} pc;

layout(location = 0) out vec3 fragWorldPos;
layout(location = 1) out vec3 fragNormal;
layout(location = 2) out vec3 fragColor;
layout(location = 3) out vec3 fragLightDir;
layout(location = 4) out vec3 fragCameraPos;
layout(location = 5) out vec4 fragMaterial;
layout(location = 6) out float fragDepth;
layout(location = 7) out vec4 fragShadingParams;
layout(location = 8) out float fragAlpha;

void main() {
    vec4 worldPos = pc.model * vec4(inPosition, 1.0);
    gl_Position = pc.projection * pc.view * worldPos;

    fragWorldPos = worldPos.xyz;
    fragNormal = normalize(mat3(pc.model) * inNormal);
    fragColor = inColor;
    fragLightDir = normalize(pc.lightDir.xyz);
    fragCameraPos = pc.cameraPos;
    fragMaterial = pc.material;
    fragDepth = gl_Position.z / gl_Position.w;
    fragShadingParams = pc.shadingParams;
    fragAlpha = pc.lightDir.w;
}
