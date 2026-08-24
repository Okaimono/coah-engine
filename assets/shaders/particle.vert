#version 450

layout(location = 0) in vec2 localPos;      // binding 0 — shared quad corner, -0.5..0.5
layout(location = 1) in vec3 worldPos;      // binding 1 — per-instance
layout(location = 2) in float size;
layout(location = 3) in vec4 uvRect;
layout(location = 4) in vec4 tint;

layout(push_constant) uniform PushData {
    mat4 view;
    mat4 proj;
} pc;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec4 fragTint;

void main() {
    // Billboard: face the camera by using view matrix's right/up columns
    vec3 camRight = vec3(pc.view[0][0], pc.view[1][0], pc.view[2][0]);
    vec3 camUp    = vec3(pc.view[0][1], pc.view[1][1], pc.view[2][1]);

    vec3 finalPos = worldPos + (camRight * localPos.x + camUp * localPos.y) * size;

    gl_Position = pc.proj * pc.view * vec4(finalPos, 1.0);

    // Map local quad corner (-0.5..0.5) to this instance's UV rect
    vec2 uv01 = localPos + 0.5;
    fragUV = mix(uvRect.xy, uvRect.zw, uv01);
    fragTint = tint;
}