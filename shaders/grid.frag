#version 460 core

in vec3 nearPoint;
in vec3 farPoint;
out vec4 frag_color;

uniform float near_plane;
uniform float far_plane;

vec4 grid(vec3 pos, float scale) {
    vec2 coord = pos.xy * scale;
    vec2 deriv = fwidth(coord);
    vec2 grid_uv = abs(fract(coord - 0.5) - 0.5) / deriv;
    float line = min(grid_uv.x, grid_uv.y);
    float alpha = 1.0 - min(line, 1.0);

    vec4 color = vec4(0.4, 0.4, 0.4, alpha * 0.6);
    
    float axis_thickness = 2.0;
    if (abs(pos.x) < deriv.y * axis_thickness || abs(pos.y) < deriv.x * axis_thickness)
        color = vec4(0.6, 0.6, 0.6, alpha);

    return color;
}

void main() {
    float t = -(nearPoint.z + 1.0) / (farPoint.z - nearPoint.z);
    if (t < 0.0) discard;

    vec3 pos = nearPoint + t * (farPoint - nearPoint);

    //Distance fade
    float dist = length(pos.xy);
    float fade = 1.0 - clamp(dist / 20.0, 0.0, 1.0);

    vec4 g = grid(pos, 1.0);

//0.4 hardcoded to be more transparent, avoids overlay on the path tracer meshes
    frag_color = g * fade * 0.4;
    if (frag_color.a < 0.01) discard;
}