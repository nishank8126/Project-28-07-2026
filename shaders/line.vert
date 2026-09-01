#version 450

layout(location = 0) in vec3 inPosition;

// Same layout as point.vert/point.frag's push constants so this pipeline can
// share the existing pipeline layout -- only viewProjection is used here.
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
}
