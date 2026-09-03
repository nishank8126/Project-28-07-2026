#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;

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

layout(location = 0) out vec3 fragColor;

void main() {
    gl_Position = viewProjection * vec4(inPosition, 1.0);
    gl_PointSize = max(1.0, pointScale * pointSize / max(1.0, gl_Position.w));
    fragColor = inColor;
}
