#version 450

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragNormal;
layout(location = 2) in vec3 fragColor;
layout(location = 3) in vec3 fragLightDir;
layout(location = 4) in vec3 fragCameraPos;
layout(location = 5) in vec4 fragMaterial;
layout(location = 6) in float fragDepth;
layout(location = 7) in vec4 fragShadingParams;
layout(location = 8) in float fragAlpha;

layout(location = 0) out vec4 outColor;

// MicroStation/ArcGIS-style hillshade: a single directional light, no
// specular term. Facets facing the light saturate to full brightness while
// neighbouring facets at a different slope stay darker -- that hard
// facet-to-facet contrast (not a specular highlight) is what reads as the
// crisp "faceted" look on a triangulated point-cloud surface.
vec3 applyPhong(vec3 normal, vec3 lightDir, vec3 /*viewDir*/, vec3 color) {
    float ambientFloor = fragMaterial.x;
    float NdotL = max(dot(normal, lightDir), 0.0);
    float shade = max(NdotL, ambientFloor);
    return color * shade;
}

vec3 applyDepthShading(vec3 color, float depth) {
    float depthMin = fragShadingParams.y;
    float depthMax = fragShadingParams.z;
    float t = clamp((depth - depthMin) / (depthMax - depthMin + 0.001), 0.0, 1.0);

    vec3 nearColor = vec3(0.0, 0.4, 0.8);
    vec3 farColor = vec3(0.8, 0.2, 0.0);

    return mix(nearColor, farColor, t) * 0.7 + color * 0.3;
}

vec3 applyEDL(vec3 color, vec3 normal, vec3 viewDir) {
    float edlStrength = fragShadingParams.w;
    // Screen-space edge enhancement. The EDL operator darkens pixels whose
    // depth changes sharply against their immediate screen neighbours; we
    // approximate neighbour-depth sampling with fragment derivatives.
    float depthDiscontinuity = clamp(fwidth(fragDepth) * edlStrength * 50.0, 0.0, 1.0);
    // Silhouette term: surfaces seen edge-on change normal rapidly too.
    float normalTerm = 1.0 - max(dot(normal, viewDir), 0.0);
    float edgeFactor = clamp(normalTerm * 0.5 + depthDiscontinuity * 0.5, 0.0, 1.0);
    return color * (1.0 - edgeFactor * edlStrength * 0.5);
}

void main() {
    vec3 N = normalize(fragNormal);
    vec3 L = normalize(fragLightDir);
    vec3 V = normalize(fragCameraPos - fragWorldPos);

    float mode = fragShadingParams.x;

    vec3 color = fragColor;

    if (mode < 0.5) {
        // Normal Phong shading
        color = applyPhong(N, L, V, fragColor);
    } else if (mode < 1.5) {
        // Depth shading
        color = applyDepthShading(fragColor, fragDepth);
    } else if (mode < 2.5) {
        // Surface shading (Phong + depth)
        color = applyPhong(N, L, V, fragColor);
        color = applyDepthShading(color, fragDepth);
    } else if (mode < 3.5) {
        // Eye Dome Lighting
        color = applyPhong(N, L, V, fragColor);
        color = applyEDL(color, N, V);
    } else if (mode < 4.5) {
        // SurfaceDebug: green wireframe for proving triangles exist
        color = vec3(0.0, 1.0, 0.0);
    } else {
        // Wireframe / unlit / edge color
        color = fragColor;
    }

    // Alpha from push constants supports Hybrid mode (surface over points).
    outColor = vec4(color, fragAlpha);
}
