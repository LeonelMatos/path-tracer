#version 460 core

out vec3 nearPoint;
out vec3 farPoint;

///Covers the entire screen
vec2 positions[4] = vec2[](
    vec2(-1.0, -1.0),
    vec2(-1.0, 1.0),
    vec2(1.0, -1.0),
    vec2(1.0, 1.0)
);

uniform mat4 view;
uniform mat4 projection;

vec3 unproject(float x, float y, float z) {
    mat4 inv = inverse(projection * view);
    vec4 p = inv * vec4(x, y, z, 1.0);
    return p.xyz / p.w;
}

void main() {
    vec2 p = positions[gl_VertexID];
    nearPoint = unproject(p.x, p.y, 0.0);
    farPoint = unproject(p.x, p.y, 1.0);
    gl_Position = vec4(p, 0.0, 1.0);
}