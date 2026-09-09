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
    uint lodLevel;
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

// ---------------------------------------------------------------------------
// Unified Material Color Pipeline (MicroStation/TerraScan-style)
// ---------------------------------------------------------------------------
// Layer 1 -- Color source: PTC classification ID -> shared classification
// SSBO. This is the SAME buffer the surface shader reads (no duplicated
// palette, no color baking). Hidden classes (alpha == 0) fall back to grey.
vec3 GetPTCBaseColor() {
    int cls = clamp(int(inClassification), 0, 255);
    vec4 col = classificationColors[cls];
    return (col.a < 0.01) ? vec3(0.5) : col.rgb;
}

// Layer 2a -- Phong lighting applied ON TOP of the base color (Blinn-Phong,
// matching the material weights already pushed for surface shading).
vec3 ApplyBlinnPhong(vec3 baseColor, vec3 n, vec3 lightDir, vec3 viewDir) {
    float ndotl = max(dot(n, lightDir), 0.0);
    // Same contrast curve as surface shader for consistent terrain relief
    float t = max(ndotl - surfaceAmbient, 0.0) / max(1.0 - surfaceAmbient, 0.001);
    float diffContrib = t * t * (3.0 - 2.0 * t);
    vec3 halfDir = normalize(lightDir + viewDir);
    float ndoth = max(dot(n, halfDir), 0.0);
    vec3 specColor = mix(vec3(1.0), baseColor, 0.15);
    vec3 spec = specColor * pow(ndoth, surfaceShininess) * surfaceSpecular;
    return baseColor * (surfaceAmbient + surfaceDiffuse * diffContrib) + spec;
}

// Layer 2b -- Hillshade lighting: single directional light, no specular.
// The hard facet-to-facet contrast (neighbouring points catching the light
// differently) is what reads as terrain relief on a dense LiDAR cloud.
vec3 ApplyHillshade(vec3 baseColor, vec3 n, vec3 lightDir) {
    float ndotl = max(dot(n, lightDir), 0.0);
    float t = max(ndotl - surfaceAmbient, 0.0) / max(1.0 - surfaceAmbient, 0.001);
    float shade = surfaceAmbient + (1.0 - surfaceAmbient) * t * t * (3.0 - 2.0 * t);
    return baseColor * shade;
}

// Layer 2c -- Eye-Dome Lighting (per-point approximation, reuses the EDL
// strength parameter): distance-based depth contrast plus a silhouette term
// for points seen edge-on. Darkens WITHOUT shifting the base hue -- this is
// what separates poles from their background and makes cables pop.
vec3 ApplyPointEDL(vec3 baseColor, vec3 n, vec3 viewDir, float dist) {
    float depthFactor = 1.0 / (1.0 + dist * 0.001 * edlStrength);
    float rim = 1.0 - abs(dot(n, viewDir));
    // Stronger rim darkening for better edge separation on poles/cables
    float edge = clamp(rim * edlStrength * 0.7, 0.0, 0.7);
    return baseColor * depthFactor * (1.0 - edge);
}

// Layer 3 -- Depth attenuation: near points keep full strength, far points
// soften slightly so foreground structure pops. Brightness-only (never
// touches alpha -- classification colors must read at full opacity).
vec3 ApplyDepthAttenuation(vec3 color, float dist) {
    float t = clamp((dist - depthMin) / max(depthMax - depthMin, 0.001), 0.0, 1.0);
    return color * mix(1.0, 0.85, t);
}

// NaN guard: normalize(0) = NaN, and a single NaN normal poisons the whole
// lighting product (NaN * color = NaN -> driver-defined garbage output).
// Fall back to straight-up (Z is up in this data) instead.
vec3 SafeNormalize(vec3 v, vec3 fallback) {
    float len = length(v);
    return (len > 1e-6) ? v / len : fallback;
}


// ---------------------------------------------------------------------------
// Adaptive point size calculation (shared by all code paths)
// ---------------------------------------------------------------------------
float ComputePointSize(float dist) {
    bool isShadedMode = (visualizationMode == 5u || visualizationMode == 13u ||
                         (visualizationMode >= 17u && visualizationMode <= 20u) ||
                         (visualizationMode >= 21u && visualizationMode <= 28u));
    float maxPointPx = isShadedMode ? 6.0 : 3.0;
    float lodPointSize;
    if (lodLevel == 0u) lodPointSize = 3.0;
    else if (lodLevel == 1u) lodPointSize = 2.0;
    else if (lodLevel == 2u) lodPointSize = 1.5;
    else if (lodLevel == 3u) lodPointSize = 1.0;
    else lodPointSize = 0.8;
    maxPointPx = min(maxPointPx, lodPointSize);
    return clamp(pointScale * pointSize / max(1.0, dist), 1.0, maxPointPx);
}

void main() {
    vec4 pos = vec4(inPosition, 1.0);

    if (visualizationMode == 11u) {
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

    // PHASE 9: Early exits for simple modes (skip lighting math)
    vec4 visualColor;
    switch (visualizationMode) {
        case 0: // RGB
            visualColor = vec4(inColor, 1.0);
            break;
        case 1: // Intensity
        {
            float i = clamp((inIntensity - intensityMin) / max(intensityMax - intensityMin, 0.001), 0.0, 1.0);
            visualColor = vec4(vec3(i), 1.0);
            break;
        }
        case 2: // Classification (8-class) or custom PTC palette
        {
            if (hasCustomPalette == 1u) {
                int cls = clamp(int(inClassification), 0, 255);
                vec4 custom = classificationColors[cls];
                visualColor = vec4(custom.rgb, custom.a);
            } else {
                visualColor = vec4(ClassifyColor(inClassification), 1.0);
            }
            break;
        }
        case 3: // Elevation
        {
            float h = clamp((inPosition.z - elevationMin) / max(elevationMax - elevationMin, 0.001), 0.0, 1.0);
            visualColor = vec4(vec3(h), 1.0);
            break;
        }
        case 4: // Height Ramp
        {
            float h4 = clamp((inPosition.z - elevationMin) / max(elevationMax - elevationMin, 0.001), 0.0, 1.0);
            visualColor = vec4(HeightRamp(h4), 1.0);
            break;
        }
        case 5: // Normal Shading
        {
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
        case 13: // Surface Shading
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
        case 14: // Eye-Dome Lighting
        {
            float d = clamp((dist - depthMin) / max(depthMax - depthMin, 0.001), 0.0, 1.0);
            vec3 baseColor = JetColormap(d);
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
        case 17: // PTC + Phong
        {
            vec3 baseColor = GetPTCBaseColor();
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 lightDir = SafeNormalize(lightDirection.xyz, vec3(0.0, 0.0, 1.0));
            vec3 viewDir = SafeNormalize(cameraPosition.xyz - pos.xyz, vec3(0.0, 0.0, 1.0));
            vec3 lit = ApplyBlinnPhong(baseColor, n, lightDir, viewDir);
            visualColor = vec4(ApplyDepthAttenuation(lit, dist), 1.0);
            break;
        }
        case 18: // PTC + Hillshade
        {
            vec3 baseColor = GetPTCBaseColor();
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 lightDir = SafeNormalize(lightDirection.xyz, vec3(0.0, 0.0, 1.0));
            visualColor = vec4(ApplyHillshade(baseColor, n, lightDir), 1.0);
            break;
        }
        case 19: // PTC + EDL
        {
            vec3 baseColor = GetPTCBaseColor();
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 viewDir = SafeNormalize(cameraPosition.xyz - pos.xyz, vec3(0.0, 0.0, 1.0));
            visualColor = vec4(ApplyPointEDL(baseColor, n, viewDir, dist), 1.0);
            break;
        }
        case 20: // PTC Composite
        {
            vec3 baseColor = GetPTCBaseColor();
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 lightDir = SafeNormalize(lightDirection.xyz, vec3(0.0, 0.0, 1.0));
            vec3 viewDir = SafeNormalize(cameraPosition.xyz - pos.xyz, vec3(0.0, 0.0, 1.0));
            vec3 lit = ApplyBlinnPhong(baseColor, n, lightDir, viewDir);
            lit = ApplyPointEDL(lit, n, viewDir, dist);
            visualColor = vec4(ApplyDepthAttenuation(lit, dist), 1.0);
            break;
        }
        case 21: visualColor = vec4(GetPTCBaseColor(), 1.0); break;
        case 22:
        {
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            visualColor = vec4(n * 0.5 + 0.5, 1.0);
            break;
        }
        case 23:
        {
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 lightDir = SafeNormalize(lightDirection.xyz, vec3(0.0, 0.0, 1.0));
            visualColor = vec4(vec3(max(dot(n, lightDir), 0.0)), 1.0);
            break;
        }
        case 24:
        {
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 lightDir = SafeNormalize(lightDirection.xyz, vec3(0.0, 0.0, 1.0));
            vec3 viewDir = SafeNormalize(cameraPosition.xyz - pos.xyz, vec3(0.0, 0.0, 1.0));
            visualColor = vec4(ApplyBlinnPhong(vec3(1.0), n, lightDir, viewDir), 1.0);
            break;
        }
        case 25:
        {
            vec3 baseColor = GetPTCBaseColor();
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 lightDir = SafeNormalize(lightDirection.xyz, vec3(0.0, 0.0, 1.0));
            vec3 viewDir = SafeNormalize(cameraPosition.xyz - pos.xyz, vec3(0.0, 0.0, 1.0));
            visualColor = vec4(ApplyBlinnPhong(baseColor, n, lightDir, viewDir), 1.0);
            break;
        }
        case 26:
        {
            float t = clamp((dist - depthMin) / max(depthMax - depthMin, 0.001), 0.0, 1.0);
            visualColor = vec4(vec3(t), 1.0);
            break;
        }
        case 27:
        {
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            vec3 viewDir = SafeNormalize(cameraPosition.xyz - pos.xyz, vec3(0.0, 0.0, 1.0));
            visualColor = vec4(ApplyPointEDL(vec3(1.0), n, viewDir, dist), 1.0);
            break;
        }
        case 28:
        {
            vec3 n = SafeNormalize(inNormal, vec3(0.0, 0.0, 1.0));
            float crevice = 1.0 - length(n);
            visualColor = vec4(vec3(1.0 - crevice * 0.5), 1.0);
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
    gl_PointSize = ComputePointSize(dist);
}
