#version 450
layout(location = 0) in vec2 inUV;
layout(location = 1) in vec4 inColor;
layout(location = 0) out vec4 fragColor;
layout(binding = 0) uniform sampler2D uiTexture;

void main() {
    fragColor = texture(uiTexture, inUV) * inColor;
}