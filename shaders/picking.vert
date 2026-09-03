#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in uint inObjectID;
layout(location = 2) in uint inEntityID;

layout(push_constant) uniform PushConstants {
    mat4 mvp;
    float pointSize;
} pc;

layout(location = 0) flat out uint fragObjectID;
layout(location = 1) flat out uint fragEntityID;

void main() {
    gl_Position = pc.mvp * vec4(inPosition, 1.0);
    gl_PointSize = pc.pointSize;
    fragObjectID = inObjectID;
    fragEntityID = inEntityID;
}
