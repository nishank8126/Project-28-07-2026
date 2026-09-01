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

struct VertexOutput {
    vec4 gl_Position;
    vec4 color;
    float gl_PointSize;
    float depth;
};

layout(location = 0) out VertexOutput vertOutput;

vec3 ClassifyColor(float classification) {
    int cls = int(classification);
    switch (cls) {
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

vec3 HeightRamp(float normalizedHeight) {
    vec3 low = vec3(0.0, 0.0, 1.0);
    vec3 mid = vec3(0.0, 1.0, 0.0);
    vec3 high = vec3(1.0, 0.0, 0.0);
    if (normalizedHeight < 0.5) {
        return mix(low, mid, normalizedHeight * 2.0);
    }
    return mix(mid, high, (normalizedHeight - 0.5) * 2.0);
}

void main() {
    vec4 pos = vec4(inPosition, 1.0);
    vec4 viewPos = viewProjection * pos;
    gl_Position = viewPos;

    float dist = length(pos.xyz - cameraPosition.xyz);

    vec4 visualColor;
    switch (visualizationMode) {
        case 0:
            visualColor = vec4(inColor, 1.0);
            break;
        case 1:
            float i = clamp((inIntensity - intensityMin) / (intensityMax - intensityMin), 0.0, 1.0);
            visualColor = vec4(vec3(i), 1.0);
            break;
        case 2:
            visualColor = vec4(ClassifyColor(inClassification), 1.0);
            break;
        case 3:
            float h = clamp((inPosition.y - elevationMin) / (elevationMax - elevationMin), 0.0, 1.0);
            visualColor = vec4(vec3(h), 1.0);
            break;
        case 4:
            float h4 = clamp((inPosition.y - elevationMin) / (elevationMax - elevationMin), 0.0, 1.0);
            visualColor = vec4(HeightRamp(h4), 1.0);
            break;
        case 5:
            vec3 n = normalize(inNormal);
            float lighting = abs(dot(n, normalize(lightDirection.xyz)));
            visualColor = vec4(inColor * (0.3 + 0.7 * lighting), 1.0);
            break;
        case 6:
            float density = 1.0 / (1.0 + dist * 0.001);
            visualColor = vec4(vec3(density), 1.0);
            break;
        default:
            visualColor = vec4(inColor, 1.0);
            break;
    }

    vertOutput.color = visualColor;
    vertOutput.depth = viewPos.w;
    gl_PointSize = max(1.0, pointScale * pointSize / max(1.0, dist));
}
