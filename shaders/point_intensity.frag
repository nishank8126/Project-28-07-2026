#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inIntensity;
layout(location = 3) in float inClassification;
layout(location = 4) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    vec4 cameraPosition;
    vec4 lightDirection;
    float pointScale;
    float pointSize;
    uint visualizationMode;
    float intensityMin;
    float intensityMax;
    float elevationMin;
    float elevationMax;
};

layout(location = 0) out vec4 vertColor;
layout(location = 1) out float vertDepth;

void main() {
    vec4 pos = vec4(inPosition, 1.0);
    vec4 viewPos = viewProjection * pos;
    gl_Position = viewPos;

    float i = clamp((inIntensity - intensityMin) / (intensityMax - intensityMin), 0.0, 1.0);
    vertColor = vec4(vec3(i), 1.0);
    vertDepth = viewPos.w;
    float dist = length(pos.xyz - cameraPosition.xyz);
    gl_PointSize = max(1.0, pointScale * pointSize / max(1.0, dist));
}
