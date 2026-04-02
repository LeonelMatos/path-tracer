/**
 * \file path_trace.frag
 * \author Leonel Matos
 * \date 2026-03-24
 * \brief Path Tracer Shader
 * \copyright Copyright (c) 2026
 */

#version 460 core

in vec2 vUV;
out vec4 frag_color;

uniform vec2 resolution;
uniform int frame_id;
uniform sampler2D prev_frame;

#include "globals.glsl"
#include "sampling.glsl"
#include "intersection.glsl"
#include "cornell_scene.glsl"

vec3 cameraRay(vec2 uv) {
    float aspect = resolution.x / resolution.y;
    float f_len = 1.0 / tan(0.5 * 30.0 * PI / 180.0);
    vec2 p = 2.0 * uv - 1.0;
    vec3 ray_cam = vec3(p.x * aspect, p.y, -f_len);

    vec3 cam_z = normalize(camera_position - camera_lookat);
    vec3 cam_x = normalize(cross(camera_up, cam_z));
    vec3 cam_y = cross(cam_z, cam_x);

    return normalize(cam_x * ray_cam.x + cam_y * ray_cam.y + cam_z * ray_cam.z);
}

///\todo check brdf
///deve ser como uma árvore
vec3 pathTrace(vec2 uv) {
    vec3 ro = camera_position;
    vec3 rd = cameraRay(uv);
    vec3 color = vec3(0);
    vec3 throughput = vec3(1);

    for (int b = 0; b < DEPTH; b++) {
        Hit h;
        if (!intersects(ro, rd, h)) break;

        color += throughput * h.emission;
        if (dot(h.emission, h.emission) > 0.0) break;

        vec3 r = rand3(b);
        float cosT = sqrt(r.x);
        float sinT = sqrt(1.0 - r.x);
        float phi = 2.0 * PI * r.y;
        rd = onb(h.normal) * vec3(sinT*cos(phi), sinT*sin(phi), cosT);
        throughput *= h.albedo;
        ro = h.pos + h.normal * EPS;
    }
    return color;
}

void main() {
    vec2 jitter = (rand3(-1).xy - 0.5) / resolution;
    vec3 linear = pathTrace(vUV + jitter);

    if (frame_id == 0) {
        frag_color = vec4(pow(linear, vec3(1.0/2.2)), 1.0);
    } else {
        vec3 prev = pow(texture(prev_frame, vUV).rgb, vec3(2.2));
        vec3 new_avg = mix(prev, linear, 1.0 / float(frame_id + 1));
        frag_color = vec4(pow(new_avg, vec3(1.0/2.2)), 1.0);
    }
}