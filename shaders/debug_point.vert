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

void main() {
    gl_Position = viewProjection * vec4(inPosition, 1.0);
    gl_PointSize = max(1.0, pointScale * pointSize / max(1.0, length(inPosition - cameraPosition.xyz)));
}
