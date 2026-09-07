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
    // Contrast curve: push dark slopes deeper while keeping illuminated faces bright.
    // Remap [0,1] -> [0,1] with a smooth S-curve that darkens mid-tones.
    float t = max(NdotL - ambientFloor, 0.0) / max(1.0 - ambientFloor, 0.001);
    float shade = ambientFloor + (1.0 - ambientFloor) * t * t * (3.0 - 2.0 * t);
    return color * shade;
}

// Full Blinn-Phong: ambient + diffuse + specular, driven by the user's
// material weights (fragMaterial = ambient/diffuse/specular/shininess).
// Used by the MicroStation-style PTC modes so the base classification color
// is lit rather than replaced.
vec3 applyBlinnPhong(vec3 normal, vec3 lightDir, vec3 viewDir, vec3 color) {
    float ambient = fragMaterial.x;
    float diffuse = fragMaterial.y;
    float specular = fragMaterial.z;
    float shininess = max(fragMaterial.w, 1.0);
    float NdotL = max(dot(normal, lightDir), 0.0);
    // Same contrast curve as applyPhong for consistent terrain relief
    float t = max(NdotL - ambient, 0.0) / max(1.0 - ambient, 0.001);
    float diffContrib = t * t * (3.0 - 2.0 * t);
    vec3 halfDir = normalize(lightDir + viewDir);
    float NdotH = max(dot(normal, halfDir), 0.0);
    // Slightly tint specular by base color for more natural appearance
    // instead of pure white wash-out
    vec3 specColor = mix(vec3(1.0), color, 0.15);
    vec3 spec = specColor * pow(NdotH, shininess) * specular;
    return color * (ambient + diffuse * diffContrib) + spec;
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
    // Stronger depth contrast: bias toward edge darkening for better
    // terrain crevice and building edge separation
    float edgeFactor = clamp(normalTerm * 0.6 + depthDiscontinuity * 0.6, 0.0, 1.0);
    return color * (1.0 - edgeFactor * edlStrength * 0.6);
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

// Check if the vertex normal is the ElevationGrid placeholder (0,0,1)
// ElevationGrid sets placeholder normals to (0,0,1) while SurfaceMeshGenerator
// creates proper flat face normals via FlattenFaceNormals.
bool IsPlaceholderNormal(vec3 n) {
    return abs(n.x) < 1e-4 && abs(n.y) < 1e-4 && abs(n.z - 1.0) < 1e-4;
}

// Classification color storage buffer: 256 × vec4 (RGBA).
// Alpha = 0 means the class is hidden. Shared with point shader.
layout(std430, set = 0, binding = 0) readonly buffer ClassificationColors {
    vec4 classificationColors[256];
};

// ---------------------------------------------------------------------------
// Unified Material Color Pipeline -- GetBaseColor()
// ---------------------------------------------------------------------------
// Single color-source lookup shared by every PTC shading mode: the
// classification ID (loaded from the ENEL PTC file) indexes the SAME
// classification SSBO the point shader reads -- no duplicated palette, no
// baked colors. Hidden classes (alpha == 0) fall back to neutral grey so a
// shading mode never resurrects a class the user hid.
vec3 GetBaseColor() {
    int cls = clamp(int(fragClassificationID), 0, 255);
    vec4 col = classificationColors[cls];
    return (col.a < 0.01) ? vec3(0.5) : col.rgb;
}

// Forward declarations for debug functions (defined after main)
vec3 DebugPTCOnly();
vec3 DebugNormals();
vec3 DebugNdotL(vec3 N, vec3 L);
vec3 DebugLightingOnly(vec3 N, vec3 L, vec3 V, vec3 color);
vec3 DebugPTCLighting(vec3 N, vec3 L, vec3 V);
vec3 DebugDepth();
vec3 DebugEDL(vec3 N, vec3 V);
vec3 DebugAO(vec3 N);

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
        // PTC Classification: flat PTC palette colors (base color only)
        color = GetBaseColor();
    } else if (mode < 12.5) {
        // PTC Hillshade: PTC base color + terrain relief lighting
        color = applyPhong(ComputeNormalFromPosition(fragWorldPos, N), L, V,
                           GetBaseColor());
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
    } else if (mode < 14.5) {
        // PTC Shading (MicroStation-style): PTC classification color stays as
        // the base color; Phong lighting adds depth, lighting and relief on
        // top. The classification hue is NEVER replaced by grey/material.
        // Use vertex normal directly for SurfaceMeshGenerator (flat face normals
        // from FlattenFaceNormals), fall back to derivative normal for ElevationGrid.
        vec3 N2 = IsPlaceholderNormal(N) ? ComputeNormalFromPosition(fragWorldPos, N) : N;
        color = applyBlinnPhong(N2, L, V, GetBaseColor());
    } else if (mode < 15.5) {
        // PTC + EDL: PTC base color + eye-dome lighting edge darkening.
        // Improves pole separation, cable visibility and building edges
        // without shifting the palette hue.
        vec3 N2 = IsPlaceholderNormal(N) ? ComputeNormalFromPosition(fragWorldPos, N) : N;
        color = applyEDL(GetBaseColor(), N2, V);
    } else if (mode < 16.5) {
        // PTC Composite (MicroStation/TerraScan look):
        // PTC color + Phong + EDL + ambient-occlusion-style depth.
        vec3 N2 = IsPlaceholderNormal(N) ? ComputeNormalFromPosition(fragWorldPos, N) : N;
        color = applyBlinnPhong(N2, L, V, GetBaseColor());
        color = applyEDL(color, N2, V);
        // AO-style crevice darkening: where the faceted geometry normal
        // swings sharply between neighbouring fragments, the surface is
        // concave (pole/ground joints, cable attachment points) -- darken
        // those crevices for grounded, occluded depth cues.
        float crevice = clamp(length(fwidth(N2)) * 2.0, 0.0, 1.0);
        color *= mix(1.0, 0.55, crevice);
    } else if (mode < 17.5) {
        // DEBUG 1: PTC only (base classification color)
        color = DebugPTCOnly();
    } else if (mode < 18.5) {
        // DEBUG 2: Normals (visualize vertex/face normals)
        color = DebugNormals();
    } else if (mode < 19.5) {
        // DEBUG 3: NdotL (diffuse lighting factor)
        color = DebugNdotL(N, L);
    } else if (mode < 20.5) {
        // DEBUG 4: Lighting only (Phong on white)
        color = DebugLightingOnly(N, L, V, vec3(1.0));
    } else if (mode < 21.5) {
        // DEBUG 5: PTC x Lighting (PTC color with Phong)
        color = DebugPTCLighting(N, L, V);
    } else if (mode < 22.5) {
        // DEBUG 6: Depth
        color = DebugDepth();
    } else if (mode < 23.5) {
        // DEBUG 7: EDL
        color = DebugEDL(N, V);
    } else if (mode < 24.5) {
        // DEBUG 8: AO (crevice darkening)
        color = DebugAO(N);
    } else {
        color = fragColor;
    }

    // Alpha from push constants supports Hybrid mode (surface over points).
    outColor = vec4(color, fragAlpha);
}

// ---------------------------------------------------------------------------
// Debug mode implementations (must be after all helper functions)
// ---------------------------------------------------------------------------
vec3 DebugPTCOnly() { return GetBaseColor(); }
vec3 DebugNormals() { return normalize(fragNormal) * 0.5 + 0.5; }
vec3 DebugNdotL(vec3 N, vec3 L) {
    float ndotl = max(dot(N, L), 0.0);
    return vec3(ndotl);
}
vec3 DebugLightingOnly(vec3 N, vec3 L, vec3 V, vec3 color) {
    return applyBlinnPhong(N, L, V, color);
}
vec3 DebugPTCLighting(vec3 N, vec3 L, vec3 V) {
    return applyBlinnPhong(N, L, V, GetBaseColor());
}
vec3 DebugDepth() {
    float t = clamp((fragWorldPos.z - fragShadingParams.y) /
                     (fragShadingParams.z - fragShadingParams.y + 0.001), 0.0, 1.0);
    return vec3(t);
}
vec3 DebugEDL(vec3 N, vec3 V) {
    return applyEDL(vec3(1.0), N, V);
}
vec3 DebugAO(vec3 N) {
    float crevice = clamp(length(fwidth(N)) * 2.0, 0.0, 1.0);
    return vec3(1.0 - crevice * 0.5);
}
