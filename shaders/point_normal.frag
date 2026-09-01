#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in float inIntensity;
layout(location = 3) in float inClassification;
layout(location = 4) in vec3 inNormal;

layout(push_constant) uniform PushConstants {
    mat4 viewProjection;
    mat4 view;
    mat4 projection;
    vec4 cameraPosition;
    vec4 cameraDirection;
    vec4 lightDirection;
    float pointScale;
    float pointSize;
    uint visualizationMode;
    float intensityMin;
    float intensityMax;
    float elevationMin;
    float elevationMax;
    uint padding0;
    uint padding1;
};

layout(location = 0) out vec4 vertColor;
layout(location = 1) out float vertDepth;

void main() {
    vec4 pos = vec4(inPosition, 1.0);
    vec4 viewPos = viewProjection * pos;
    gl_Position = viewPos;
    vec3 n = normalize(inNormal);
    float lighting = abs(dot(n, normalize(lightDirection.xyz)));
    vertColor = vec4(inColor * (0.3 + 0.7 * lighting), 1.0);
    vertDepth = viewPos.w;
    float dist = length(pos.xyz - cameraPosition.xyz);
    gl_PointSize = max(1.0, pointScale * pointSize / max(1.0, dist));
}
