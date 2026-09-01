#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in float inDepth;

layout(location = 0) out vec4 outColor;

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

void main() {
    vec2 center = gl_PointCoord - vec2(0.5);
    float dist = length(center);
    if (dist > 0.5) {
        discard;
    }

    float edgeFade = smoothstep(0.5, 0.35, dist);
    float depthFade = clamp(1.0 - inDepth * 0.0001, 0.3, 1.0);

    outColor = vec4(inColor.rgb, edgeFade * depthFade);
}
