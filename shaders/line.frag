#version 450

layout(location = 0) out vec4 outColor;

void main() {
    // Flat amber overlay color, distinguishing vector/CAD entities (e.g.
    // .snt shapes) from the point cloud they're drawn over.
    outColor = vec4(1.0, 0.85, 0.2, 1.0);
}
