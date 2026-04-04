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
    vec3 ray_origin = camera_position;
    vec3 ray_dir = cameraRay(uv);
    vec3 color = vec3(0);
    vec3 throughput = vec3(1);

    for (int b = 0; b < DEPTH; b++) {
        Hit h;
        //Background Alternative Colors
        if (!intersects(ray_origin, ray_dir, h)) {
            switch(BACKGROUND) {
                case BG_BLACK:
                    color += vec3(0);
                break;
                case BG_WHITE:
                    color += throughput * vec3(0.9);
                break;
                case BG_GRADIENT: //skybox-like
                    float t = clamp(ray_dir.z * 0.5 + 0.5, 0.0, 1.0);
                    color += throughput * mix(vec3(0.5, 0.55, 0.6), vec3(0.8, 0.8, 0.8), t);
                break;
            }
            break;
        }

        color += throughput * h.emission;
        if (dot(h.emission, h.emission) > 0.0) break;

        vec3 r = rand3(b);

        switch(h.material) {
            //Diffuse Materials
            case MAT_DIFFUSE:
                //Cosine-weighted hemisphere
                float cosT = sqrt(r.x);
                float sinT = sqrt(1.0 - r.x);
                float phi = 2.0 * PI * r.y;
                ray_dir = onb(h.normal) * vec3(sinT*cos(phi), sinT*sin(phi), cosT);
                throughput *= h.albedo;
                ray_origin = h.pos + h.normal * EPS;
            break;
            case MAT_MIRROR:
                ray_dir = reflect(ray_dir, h.normal);
                throughput += h.albedo;
                ray_origin = h.pos + h.normal * EPS;
            break;
            case MAT_GLASS:
                ///\todo Glass material math
            break;
        }
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