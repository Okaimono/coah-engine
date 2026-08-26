#version 450
layout(location = 0) in vec3 localPos;      // binding 0 — per-vertex, real 3D cube offset
layout(location = 1) in vec2 vertUV;        // binding 0 — per-vertex uv (0..1 per face)
layout(location = 2) in vec3 worldPos;      // binding 1 — per-instance
layout(location = 3) in float size;
layout(location = 4) in vec4 rotation;

layout(push_constant) uniform PushData {
    mat4 view;
    mat4 proj;
} pc;

layout(location = 0) out vec2 fragUV;

vec3 rotateByQuat(vec3 v, vec4 q) {
    vec3 t = 2.0 * cross(q.xyz, v);
    return v + q.w * t + cross(q.xyz, t);
}

void main() {
    vec3 rotatedPos = rotateByQuat(localPos * size, rotation);
    vec3 finalPos   = worldPos + rotatedPos;

    gl_Position = pc.proj * pc.view * vec4(finalPos, 1.0);
    fragUV = vertUV;
}