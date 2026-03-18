#version 460 core

out vec2 vUV;

void main() {
    vec2 pos[4] = vec2[4](
        vec2(-1.0, -1.0), vec2(1.0, -1.0),
        vec2(-1.0,  1.0), vec2(1.0,  1.0)
    );
    gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
    vUV = pos[gl_VertexID] * 0.5 + 0.5;
}