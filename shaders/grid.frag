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

    vec4 color = vec4(0.4, 0.4, 0.4, alpha * 0.5);

    //10-step marks
    vec2 coord10 = pos.xy * 0.1;
    vec2 deriv10 = fwidth(coord10);
    vec2 grid10_uv = abs(fract(coord10 - 0.5) - 0.5) / deriv10;
    float line10 = min(grid10_uv.x, grid10_uv.y);
    float alpha10 = 1.0 - min(line10, 1.0);
    if(alpha10 > 0.05)
        color = mix(color, vec4(0.6, 0.6, 0.6, alpha10 * 0.8), alpha10);
    
    //axis subtle color
    if(abs(pos.y) < deriv.x * 2.5)
        color = vec4(0.6, 0.25, 0.25, alpha * 0.9);
    if(abs(pos.x) < deriv.y * 2.5)
        color = vec4(0.25, 0.6, 0.25, alpha * 0.9);

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

    frag_color = g * fade;
    if (frag_color.a < 0.01) discard;
}