#version 450

layout(binding = 0) uniform sampler2D tex;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec4 fragTint;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 sampled = texture(tex, fragUV);
    outColor = sampled * fragTint;
}