#version 450

layout(location = 0) in vec4 inColor;
layout(location = 1) in float inDepth;
layout(location = 2) in vec3 inWorldPos;
layout(location = 3) in vec3 inNormal;

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
    float depthMin;
    float depthMax;
    float surfaceAmbient;
    float surfaceDiffuse;
    float surfaceSpecular;
    float surfaceShininess;
    float edlStrength;
    uint hasCustomPalette;
};

void main() {
    if (visualizationMode == 11u) {
        // DEBUG mode: no discard, no fade, just solid color
        outColor = vec4(1.0, 0.0, 0.0, 1.0);
        return;
    }

    // Hidden classification class: alpha was set to 0 by the vertex shader.
    if (inColor.a < 0.01) {
        discard;
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

    // Only fade at the sprite's circular edge (antialiasing) - a distance
    // -based alpha fade was previously multiplied in here too, which floored
    // at 0.3 opacity for far points and, blended over the dark background
    // through many overlapping points, washed every colour (including
    // classification palette colours) toward a muddy grey/black regardless
    // of the point's true colour. Classification/RGB/etc. colours should
    // read at full strength; distance cues belong in a dedicated depth
    // -shading mode, not baked into every mode's alpha.
    float edgeFade = smoothstep(adaptiveThreshold, adaptiveThreshold * 0.7, dist);

    outColor = vec4(inColor.rgb, edgeFade);
}
