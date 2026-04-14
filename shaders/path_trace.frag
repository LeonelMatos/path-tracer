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


///\brief Calculates the base of the camera in world-space
///\param vec3 All camera axes
void camera_axes(out vec3 cam_x, out vec3 cam_y, out vec3 cam_z) {
    cam_z = normalize(camera_position - camera_lookat);
    cam_x = normalize(cross(camera_up, cam_z));
    cam_y = cross(cam_z, cam_x);
}

/**Calculates the direction of a ray for a pixel
\param uv Pixel coordinates
\param vec3 All camera axes
\return Normalized direction of ray in worldspace
*/
vec3 cameraRay(vec2 uv, vec3 cam_x, vec3 cam_y, vec3 cam_z) {
    float aspect = resolution.x / resolution.y;
    float f_len = 1.0 / tan(0.5 * 30.0 * PI / 180.0);
    vec2 p = 2.0 * uv - 1.0;
    vec3 ray_cam = vec3(p.x * aspect, p.y, -f_len);
    return normalize(cam_x * ray_cam.x + cam_y * ray_cam.y + cam_z * ray_cam.z);
}

/**
\brief Creates a ray with depth of field
\param uv pixel coordinates [0,1]
\return Ray with origin at the lens directed to the focal point
\note CAM_APERTURE = 0.0 is compares to pinhole
\see CAM_APERTURE, CAM_FOCAL_DISTANCE
*/
Ray cameraRayDOF(vec2 uv) {
    vec3 cam_x, cam_y, cam_z;
    camera_axes(cam_x, cam_y, cam_z);

    //Focal point definition
    vec3 base_dir = cameraRay(uv, cam_x, cam_y, cam_z);
    vec3 focal_point = camera_position + base_dir * CAM_FOCAL_DISTANCE;

    vec3 r = rand3(-2);
    float angle = r.x * 2.0 * PI;
    float radius = sqrt(r.y) * CAM_APERTURE;
    vec3 lens_offset = (cos(angle) * cam_x + sin(angle) * cam_y) * radius;

    vec3 origin = camera_position + lens_offset;
    return Ray(origin, normalize(focal_point - origin));
}

/**
Traces a path for each pixel and returns the radiance
\param uv pixel coordinates [0,1]
\return vec3 color
\note Uses cosine-weighted sampling for diffuse materials
and Fresnel+Snell for glass materials
\see BACKGROUND, FOCAL_DEBUG
\todo check brdf, deve ser como uma árvore
*/
vec3 pathTrace(vec2 uv) {
    Ray ray = cameraRayDOF(uv);
    vec3 color = vec3(0);
    vec3 throughput = vec3(1);

    //foreach ray bounce
    for (int b = 0; b < DEPTH; b++) {
        Hit h;
        //Background Alternative Colors
        if (!intersects(ray, h)) {
            switch(BACKGROUND) {
                case BG_BLACK:
                break;
                case BG_WHITE:
                    color += throughput * vec3(0.9);
                break;
                case BG_GRADIENT: //skybox-like
                    float t = clamp(ray.direction.z * 0.5 + 0.5, 0.0, 1.0);
                    color += throughput * mix(vec3(0), vec3(1), t);
                break;
            }
            break;
        }

        if (FOCAL_DEBUG && b == 0) {
            float dist_to_focal = abs(h.t - CAM_FOCAL_DISTANCE);
            if(dist_to_focal < FOCAL_BAND_DEBUG)
                color += vec3(0.0, 1.0, 0.0) * 0.5;
        }

        color += throughput * h.emission;
        if (dot(h.emission, h.emission) > 0.0) break;

        vec3 r = rand3(b);

        switch(h.material) {
            //Diffuse Materials
            case MAT_DIFFUSE: {
                //Cosine-weighted hemisphere
                float cosT = sqrt(r.x);
                float sinT = sqrt(1.0 - r.x);
                float phi = 2.0 * PI * r.y;
                ray.direction = onb(h.normal) * vec3(sinT*cos(phi), sinT*sin(phi), cosT);
                throughput *= h.albedo;
                ray.origin = h.pos + h.normal * EPS;
                break;
            }
            case MAT_MIRROR: {
                ray.direction = reflect(ray.direction, h.normal);
                throughput *= h.albedo;
                ray.origin = h.pos + h.normal * EPS;
                break;
            }
            case MAT_GLASS: {
                bool h_entering = dot(ray.direction, h.normal) < 0.0;
                vec3 normal = h_entering ? h.normal : -h.normal;
                float eta = h_entering ? (1.0 / h.ior) : h.ior;

                float cos_theta = abs(dot(-ray.direction, normal));
                float r0 =  (1.0 - eta) / (1.0 + eta);
                r0 *= r0; //r0²
                float fresnel = r0 + (1.0 - r0) * pow(1.0 - cos_theta, 5.0);

                float sin_theta_sq = eta * eta * (1.0 - cos_theta * cos_theta);
                bool total_reflect = sin_theta_sq > 1.0;

                if (total_reflect || r.z < fresnel) {
                    ray.direction = reflect(ray.direction, normal);
                    ray.origin = h.pos + normal * EPS;
                }
                else {
                    ray.direction = refract(ray.direction, normal, eta);
                    ray.origin = h.pos - normal * EPS;
                }
                throughput *= h.albedo;
                break;
            }
        }

        // Russian Roulette
        if (RR_MIN_BOUNCES > 0 && b >= RR_MIN_BOUNCES) {
            float survival = max(throughput.r, max(throughput.g, throughput.b));

            survival = min(survival, RR_MAX_SURVIVAL);

            //Ends path with P = 1 - survival
            if (rand3(b + DEPTH).x > survival) break;

            throughput /= survival;
        }
    }
    return color;
}

void main() {
    vec3 linear = vec3(0);

    for (int s = 0; s < SAMPLES_PER_PIXEL; s++) {
        vec2 jitter = (rand3(-(s+1)).xy - 0.5) / resolution;
        linear += pathTrace(vUV + jitter);
    }
    linear /= float(SAMPLES_PER_PIXEL);

    vec3 accumulated;   
    if (frame_id == 0) {
        accumulated = linear;
    } else {
        vec3 prev = texture(prev_frame, vUV).rgb;
        accumulated = mix(prev, linear, 1.0 / float(frame_id + 1));
    }
    frag_color = vec4(accumulated, 1.0);
}