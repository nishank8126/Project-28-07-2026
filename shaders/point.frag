#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in float inDepth;

layout(location = 0) out vec4 outColor;

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
    if (visualizationMode == 11u) {
        // DEBUG mode: no discard, no fade, just solid color
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    vec2 center = gl_PointCoord - vec2(0.5);
    float dist = length(center);

    // Adaptive discard: only discard for points large enough to form a circle.
    // For points <= 2 pixels, skip the circle test to avoid killing all fragments.
    float pointRadius = length(vec2(dFdx(gl_PointCoord.x), dFdy(gl_PointCoord.y))) * 0.5;
    float adaptiveThreshold = max(0.5, pointRadius * 2.0);
    if (dist > adaptiveThreshold) {
        discard;
    }

    float edgeFade = smoothstep(adaptiveThreshold, adaptiveThreshold * 0.7, dist);
    float depthFade = clamp(1.0 - inDepth * 0.0001, 0.3, 1.0);

    outColor = vec4(inColor.rgb, edgeFade * depthFade);
}
