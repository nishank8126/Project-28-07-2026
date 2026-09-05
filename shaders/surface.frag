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

// Classification ID from vertex shader (passed through from SurfaceVertex.classificationID)
layout(location = 9) flat in uint fragClassificationID;

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

// Elevation colormap: 5-stop professional terrain ramp
// blue -> cyan -> green -> yellow -> red
vec3 ElevationColormap(float t) {
    t = clamp(t, 0.0, 1.0);
    const vec3 c0 = vec3(0.0, 0.0, 0.8);  // deep blue
    const vec3 c1 = vec3(0.0, 0.8, 0.8);  // cyan
    const vec3 c2 = vec3(0.0, 0.8, 0.0);  // green
    const vec3 c3 = vec3(1.0, 1.0, 0.0);  // yellow
    const vec3 c4 = vec3(1.0, 0.0, 0.0);  // red
    if (t < 0.25) return mix(c0, c1, t * 4.0);
    if (t < 0.50) return mix(c1, c2, (t - 0.25) * 4.0);
    if (t < 0.75) return mix(c2, c3, (t - 0.50) * 4.0);
    return mix(c3, c4, (t - 0.75) * 4.0);
}

// Compute normal from world-space position derivatives.
// Falls back to the vertex-supplied normal if derivatives are degenerate
// (e.g. screen-space derivatives collapse at triangle edges or when the
// mesh has zero-area triangles).
vec3 ComputeNormalFromPosition(vec3 wp, vec3 vertexNormal) {
    vec3 n = cross(dFdx(wp), dFdy(wp));
    float len = length(n);
    if (len < 1e-6) {
        // Degenerate derivative — use vertex normal as fallback
        return vertexNormal;
    }
    return n / len;
}

// Classification color storage buffer: 256 × vec4 (RGBA).
// Alpha = 0 means the class is hidden. Shared with point shader.
layout(std430, set = 0, binding = 0) readonly buffer ClassificationColors {
    vec4 classificationColors[256];
};

// MicroStation/ArcGIS-style hillshade: a single directional light, no

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
    } else if (mode < 5.5) {
        // Wireframe / unlit / edge color
        color = fragColor;
    } else if (mode < 6.5) {
        // Elevation Heatmap
        float elevMin = fragShadingParams.y;
        float elevMax = fragShadingParams.z;
        float h = clamp((fragWorldPos.z - elevMin) /
                         (elevMax - elevMin + 0.001), 0.0, 1.0);
        color = ElevationColormap(h);
    } else if (mode < 7.5) {
        // Hillshade: elevation colormap + Phong lighting
        float elevMin = fragShadingParams.y;
        float elevMax = fragShadingParams.z;
        float h = clamp((fragWorldPos.z - elevMin) /
                         (elevMax - elevMin + 0.001), 0.0, 1.0);
        vec3 N2 = ComputeNormalFromPosition(fragWorldPos, N);
        color = applyPhong(N2, L, V, ElevationColormap(h));
    } else if (mode < 8.5) {
        // Slope: flat=green, steep=red
        vec3 N2 = ComputeNormalFromPosition(fragWorldPos, N);
        float slope = acos(clamp(N2.z, 0.0, 1.0)) / 1.5708;
        color = mix(vec3(0.0, 0.8, 0.0), vec3(1.0, 0.0, 0.0), slope);
    } else if (mode < 9.5) {
        // Aspect: compass direction coloring
        vec3 N2 = ComputeNormalFromPosition(fragWorldPos, N);
        float asp = (atan(N2.y, N2.x) + 3.14159) / 6.28318;
        color = ElevationColormap(asp);
    } else if (mode < 10.5) {
        // Elevation Composite: elevation + hillshade + EDL
        float elevMin = fragShadingParams.y;
        float elevMax = fragShadingParams.z;
        float h = clamp((fragWorldPos.z - elevMin) /
                         (elevMax - elevMin + 0.001), 0.0, 1.0);
        vec3 N2 = ComputeNormalFromPosition(fragWorldPos, N);
        color = applyPhong(N2, L, V, ElevationColormap(h));
        color = applyEDL(color, N2, V);
    } else if (mode < 11.5) {
        // PTC Classification: lookup classification ID in palette
        int cls = int(fragClassificationID);
        cls = clamp(cls, 0, 255);
        vec4 col = classificationColors[cls];
        // If alpha is 0, class is hidden - use grey
        if (col.a < 0.01) {
            color = vec3(0.5);
        } else {
            color = col.rgb;
        }
    } else if (mode < 12.5) {
        // PTC Hillshade: PTC color + Phong lighting
        int cls = int(fragClassificationID);
        cls = clamp(cls, 0, 255);
        vec4 col = classificationColors[cls];
        if (col.a < 0.01) {
            color = vec3(0.5);
        } else {
            vec3 N2 = ComputeNormalFromPosition(fragWorldPos, N);
            color = applyPhong(N2, L, V, col.rgb);
        }
    } else if (mode < 13.5) {
        // Elevation + PTC Composite: elevation heatmap + PTC tint
        float elevMin = fragShadingParams.y;
        float elevMax = fragShadingParams.z;
        float h = clamp((fragWorldPos.z - elevMin) /
                         (elevMax - elevMin + 0.001), 0.0, 1.0);
        vec3 N2 = ComputeNormalFromPosition(fragWorldPos, N);
        color = applyPhong(N2, L, V, ElevationColormap(h));
        color = applyEDL(color, N2, V);
        // Tint with PTC classification
        int cls = int(fragClassificationID);
        cls = clamp(cls, 0, 255);
        vec4 col = classificationColors[cls];
        if (col.a >= 0.01) {
            color = color * col.rgb;
        }
    } else {
        color = fragColor;
    }

    // Alpha from push constants supports Hybrid mode (surface over points).
    outColor = vec4(color, fragAlpha);
}
