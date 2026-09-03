#version 450

layout(location = 0) flat in uint fragObjectID;
layout(location = 1) flat in uint fragEntityID;

layout(location = 0) out uvec4 outIDs;

void main() {
    outIDs = uvec4(fragObjectID, fragEntityID, 0, 0);
}
