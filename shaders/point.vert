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
    float depthMin;
    float depthMax;
    float surfaceAmbient;
    float surfaceDiffuse;
    float surfaceSpecular;
    float surfaceShininess;
    float edlStrength;
    uint hasCustomPalette;
};

// Classification color storage buffer: 256 × vec4 (RGBA).
// Alpha = 0 means the class is hidden.
layout(std430, set = 0, binding = 0) readonly buffer ClassificationColors {
    vec4 classificationColors[256];
};

layout(location = 0) out vec4 fragColor;
layout(location = 1) out float fragDepth;
layout(location = 2) out vec3 fragWorldPos;
layout(location = 3) out vec3 fragNormal;

// ---------------------------------------------------------------------------
// ASPRS LAS 1.4 classification colours (18 classes) -- full palette
// Matches ClassificationPalette in VisualizationManager.
// ---------------------------------------------------------------------------
vec3 ClassifyColorFull(float classification) {
    int cls = int(classification);
    switch (cls) {
        case 0:  return vec3(0.50, 0.50, 0.50); // Created / Never classified
        case 1:  return vec3(0.00, 1.00, 0.00); // Unclassified
        case 2:  return vec3(0.00, 0.78, 0.00); // Ground
        case 3:  return vec3(0.00, 0.60, 0.00); // Low Vegetation
        case 4:  return vec3(0.00, 0.42, 0.00); // Medium Vegetation
        case 5:  return vec3(0.00, 0.25, 0.00); // High Vegetation
        case 6:  return vec3(1.00, 0.00, 0.00); // Building
        case 7:  return vec3(1.00, 0.50, 0.00); // Low Point (noise)
        case 8:  return vec3(1.00, 1.00, 0.00); // Model Key-point
        case 9:  return vec3(0.50, 0.00, 0.50); // Water
        case 10: return vec3(0.75, 0.75, 0.75); // Overlap Flight Path
        case 11: return vec3(0.80, 0.80, 0.00); // Wire – Conductor
        case 12: return vec3(0.60, 0.60, 0.00); // Wire – Fence
        case 13: return vec3(0.40, 0.40, 0.00); // Wire – Transmission
        case 14: return vec3(0.20, 0.20, 0.00); // Wire – Cable
        case 15: return vec3(0.60, 0.30, 0.00); // Wire – Structure
        case 16: return vec3(0.00, 0.00, 1.00); // Road Surface
        case 17: return vec3(0.50, 0.50, 1.00); // Roof
        default: return vec3(1.0, 1.0, 1.0);
    }
}

// Original 8-class palette used by classification mode == 2
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

// Jet colourmap for depth shading
vec3 JetColormap(float t) {
    t = clamp(t, 0.0, 1.0);
    float r = clamp(1.5 - abs(t - 0.75) * 4.0, 0.0, 1.0);
    float g = clamp(1.5 - abs(t - 0.50) * 4.0, 0.0, 1.0);
    float b = clamp(1.5 - abs(t - 0.25) * 4.0, 0.0, 1.0);
    return vec3(r, g, b);
}

void main() {
    vec4 pos = vec4(inPosition, 1.0);

    if (visualizationMode == 11u) {
        // DEBUG mode: bypass everything, just project and use fixed size
        gl_Position = viewProjection * pos;
        fragColor = vec4(1.0, 0.0, 0.0, 1.0);
        fragDepth = gl_Position.w;
        fragWorldPos = inPosition;
        fragNormal = vec3(0.0);
        gl_PointSize = 5.0;
        return;
    }

    vec4 viewPos = viewProjection * pos;
    gl_Position = viewPos;

    float dist = length(pos.xyz - cameraPosition.xyz);

    vec4 visualColor;
    switch (visualizationMode) {
        case 0: // RGB
            visualColor = vec4(inColor, 1.0);
            break;
        case 1: // Intensity
        {
            float i = clamp((inIntensity - intensityMin) / (intensityMax - intensityMin), 0.0, 1.0);
            visualColor = vec4(vec3(i), 1.0);
            break;
        }
        case 2: // Classification (8-class) or custom PTC palette
        {
            if (hasCustomPalette == 1u) {
                int cls = clamp(int(inClassification), 0, 255);
                vec4 custom = classificationColors[cls];
                // DEBUG: force bright colors for known ENEL classes to verify SSBO lookup
                if (cls == 14) { visualColor = vec4(1.0, 0.0, 0.0, 1.0); }
                else if (cls == 16) { visualColor = vec4(0.0, 0.0, 1.0, 1.0); }
                else if (cls == 17) { visualColor = vec4(0.6, 0.3, 0.0, 1.0); }
                else { visualColor = vec4(custom.rgb, custom.a); }
            } else {
                visualColor = vec4(ClassifyColor(inClassification), 1.0);
            }
            break;
        }
        case 3: // Elevation
        {
            float h = clamp((inPosition.z - elevationMin) / (elevationMax - elevationMin), 0.0, 1.0);
            visualColor = vec4(vec3(h), 1.0);
            break;
        }
        case 4: // Height Ramp
        {
            float h4 = clamp((inPosition.z - elevationMin) / (elevationMax - elevationMin), 0.0, 1.0);
            visualColor = vec4(HeightRamp(h4), 1.0);
            break;
        }
        case 5: // Normal Shading
        {
            // Blinn-Phong (diffuse + specular), matching case 13's material
            // terms - diffuse alone (the old behaviour here) reads as flat
            // and dim; the specular highlight is what gives shaded point
            // clouds that crisp, faceted look (each point's own normal
            // catches the light differently, like a tiny mirror facet).
            vec3 n = normalize(inNormal);
            vec3 lightDir = normalize(lightDirection.xyz);
            float ndotl = max(dot(n, lightDir), 0.0);
            vec3 viewDir = normalize(cameraPosition.xyz - pos.xyz);
            vec3 halfDir = normalize(lightDir + viewDir);
            float ndoth = max(dot(n, halfDir), 0.0);
            float spec = pow(ndoth, surfaceShininess) * surfaceSpecular;
            vec3 lit = inColor * (surfaceAmbient + surfaceDiffuse * ndotl) + vec3(spec);
            visualColor = vec4(lit, 1.0);
            break;
        }
        case 6: // Density
        {
            float density = 1.0 / (1.0 + dist * 0.001);
            visualColor = vec4(vec3(density), 1.0);
            break;
        }
        case 12: // Depth Shading
        {
            float d = clamp((dist - depthMin) / max(depthMax - depthMin, 0.001), 0.0, 1.0);
            visualColor = vec4(JetColormap(d), 1.0);
            break;
        }
        case 13: // Surface Shading (normals + depth composite)
        {
            vec3 n = normalize(inNormal);
            vec3 lightDir = normalize(lightDirection.xyz);
            float ndotl = max(dot(n, lightDir), 0.0);
            vec3 viewDir = normalize(cameraPosition.xyz - pos.xyz);
            vec3 halfDir = normalize(lightDir + viewDir);
            float ndoth = max(dot(n, halfDir), 0.0);
            float spec = pow(ndoth, surfaceShininess) * surfaceSpecular;
            float d = clamp((dist - depthMin) / max(depthMax - depthMin, 0.001), 0.0, 1.0);
            vec3 baseColor = JetColormap(d);
            vec3 lit = baseColor * (surfaceAmbient + surfaceDiffuse * ndotl) + vec3(spec);
            visualColor = vec4(lit, 1.0);
            break;
        }
        case 14: // Eye-Dome Lighting (simplified per-point)
        {
            float d = clamp((dist - depthMin) / max(depthMax - depthMin, 0.001), 0.0, 1.0);
            vec3 baseColor = JetColormap(d);
            // Simplified EDL: darken based on depth gradient estimation
            float depthFactor = 1.0 / (1.0 + dist * 0.001 * edlStrength);
            visualColor = vec4(baseColor * depthFactor, 1.0);
            break;
        }
        case 15: // Classification Palette (full 18-class ASPRS or custom PTC)
        {
            if (hasCustomPalette == 1u) {
                int cls = clamp(int(inClassification), 0, 255);
                vec4 custom = classificationColors[cls];
                visualColor = vec4(custom.rgb, custom.a);
            } else {
                visualColor = vec4(ClassifyColorFull(inClassification), 1.0);
            }
            break;
        }
        default:
            visualColor = vec4(inColor, 1.0);
            break;
    }

    fragColor = visualColor;
    fragDepth = viewPos.w;
    fragWorldPos = inPosition;
    fragNormal = inNormal;
    // pointScale (viewport height * 0.5) / dist has no upper bound: for a
    // compact, dense scan viewed at a typical framing distance this computes
    // to a circle tens of pixels wide, so neighbouring points (often
    // millimetres apart in a dense LiDAR scan) overlap and merge into solid
    // blobs instead of showing as distinct dots. Cap it so points stay small
    // and crisp regardless of how close the camera gets.
    //
    // Shaded modes (Normal/Surface Shading) are the exception: there, points
    // NEED to touch/overlap a little so each one's own lit normal reads as a
    // small flat facet rather than an isolated dot - that faceted look is
    // the whole point of per-point normal shading on a dense cloud.
    float maxPointPx = (visualizationMode == 5u || visualizationMode == 13u) ? 6.0 : 3.0;
    gl_PointSize = clamp(pointScale * pointSize / max(1.0, dist), 1.0, maxPointPx);
}
