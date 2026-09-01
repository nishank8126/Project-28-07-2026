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

vec3 ClassifyColor(float cls) {
    int c = int(cls);
    switch (c) {
        case 0: return vec3(0.5, 0.5, 0.5);
        case 1: return vec3(0.0, 1.0, 0.0);
        case 2: return vec3(0.0, 0.8, 0.0);
        case 3: return vec3(0.0, 0.6, 0.0);
        case 4: return vec3(1.0, 1.0, 0.0);
        case 5: return vec3(1.0, 0.5, 0.0);
        case 6: return vec3(1.0, 0.0, 0.0);
        case 7: return vec3(0.5, 0.0, 0.5);
        default: return vec3(1.0, 1.0, 1.0);
    }
}

void main() {
    vec4 pos = vec4(inPosition, 1.0);
    vec4 viewPos = viewProjection * pos;
    gl_Position = viewPos;
    vertColor = vec4(ClassifyColor(inClassification), 1.0);
    vertDepth = viewPos.w;
    float dist = length(pos.xyz - cameraPosition.xyz);
    gl_PointSize = max(1.0, pointScale * pointSize / max(1.0, dist));
}
