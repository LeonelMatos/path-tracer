/**
 * \file path_trace_core.glsl
 * \author Leonel Matos
 * \date 2026-04-22
 * \brief Path Tracer Core Shader
 * \copyright Copyright (c) 2026
 */

//----------------------------------------------------------
//Camera Functions

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
    float f_len = 1.0 / tan(0.5 * CAM_FOV);
    vec2 p = 2.0 * uv - 1.0;
    vec3 ray_cam = vec3(p.x * aspect, p.y, -f_len);
    return normalize(cam_x * ray_cam.x + cam_y * ray_cam.y + cam_z * ray_cam.z);
}

/**
\brief Creates a ray with depth of field
\param uv pixel coordinates [0,1]
\return Ray with origin at the lens directed to the focal point
\note random jitter generated here, for each spp
\note CAM_APERTURE = 0.0 is compares to pinhole
\see CAM_APERTURE, CAM_FOCAL_DISTANCE
*/
Ray cameraRayDOF(vec2 uv, int spp_index, uvec2 px) {
    vec3 cam_x, cam_y, cam_z;
    camera_axes(cam_x, cam_y, cam_z);

    vec3 rj = rand3(-2, spp_index, px);
    vec2 jitter = (rj.xz - 0.5) / resolution;

    //Focal point definition
    vec3 base_dir = cameraRay(uv + jitter, cam_x, cam_y, cam_z);
    vec3 focal_point = camera_position + base_dir * CAM_FOCAL_DISTANCE;

    //Lens disk
    float angle = rj.x * TWO_PI;
    vec3 rDOF = rand3(-3, spp_index, px); //needs different random seed
    float radius = sqrt(rDOF.x) * CAM_APERTURE;
    vec3 lens_offset = (cos(angle) * cam_x + sin(angle) * cam_y) * radius;

    vec3 origin = camera_position + lens_offset;
    return Ray(origin, normalize(focal_point - origin));
}

//----------------------------------------------------------
//Next Event Estimation

vec3 sampleAnalyticLight(Hit h, int bounce, int spp_index, uvec2 px) {
    if (analytic_light_count == 0) return vec3(0);

    //pick random light from buffer
    vec3 rnd = rand3(bounce + 200, spp_index, px);
    int light_idx = int(rnd.x * float(analytic_light_count));
    GPULight light = analytic_lights[light_idx];

    //calculates the distance and direction
    vec3 to_light;
    float dist;
    float attenuation = 1.0;

    switch (light.type) {
        case LIGHT_POINT: {
            vec3 offset = vec3(0);
            if(light.radius > 0.0) {
                float r = light.radius * pow(rnd.y, 1.0 / 3.0);
                float phi = TWO_PI * rnd.z;
                float cos_t = 2.0 * rnd.x - 1.0;
                float sin_t = sqrt(1.0 - cos_t * cos_t);
                offset = r * vec3(sin_t * cos(phi), sin_t * sin(phi), cos_t);
            }
            to_light = light.position.xyz + offset - h.pos;
            dist = length(to_light);
            attenuation = 1.0 / (dist * dist);
            break;
        }
        case LIGHT_DIRECTIONAL: {
            to_light = -light.direction.xyz;
            dist = 1e10;
            attenuation = 1.0;
            break;
        }
        case LIGHT_SPOT: {
            to_light = light.position.xyz - h.pos;
            dist = length(to_light);
            attenuation = 1.0 / (dist * dist);
            vec3 dir_to_dir = -normalize(to_light);
            float cos_angle = dot(dir_to_dir, light.direction.xyz);
            float t = smoothstep(light.spot_outer, light.spot_inner, cos_angle);
            attenuation *= t;
            if (attenuation <= 0) return vec3(0);
            break;
        }
        default:
            return vec3(0);
    }

    vec3 dir_light = normalize(to_light);
    float cos_surface = dot(h.normal, dir_light);
    if (cos_surface <= 0.0) return vec3(0);

    Ray shadow_ray;
    shadow_ray.origin = h.pos + h.normal * EPS;
    shadow_ray.direction = dir_light;

    Hit shadow_hit;
    bool occluded = intersects(shadow_ray, shadow_hit) && shadow_hit.t < dist - EPS;

    if (occluded) return vec3(0);

    float pdf = 1.0 / float(analytic_light_count);
    //Direct contribution
    return h.albedo * light.emission.rgb * cos_surface * attenuation / (PI * pdf);
}

vec3 sampleEmissiveTriangles(Hit h, int bounce, int spp_index, uvec2 px) {
    if(light_count == 0) return vec3(0);

    vec3 rnd = rand3(bounce + 100, spp_index, px);
    int light_idx = light_indices[int(rnd.x * float(light_count))];

    GPUTriangle light_tri = triangles[light_idx];
    int m_id = light_tri.material_id;
    vec3 emission = gpu_materials[m_id].emission.rgb;

    if (dot(emission, emission) == 0.0) return vec3(0);

    //Point sample on light surface
    vec3 light_pos = sampleTriangle(light_tri.v0.position, light_tri.v1.position, light_tri.v2.position, rnd.yz);

    //Shadow rays
    vec3 to_light = light_pos - h.pos;
    float dist = length(to_light);
    vec3 dir_light = to_light / dist;

    //Check if light is in the correct surface side
    float cos_surface = dot(h.normal, dir_light);
    if (cos_surface <= 0.0) return vec3(0);

    //Light normal
    vec3 light_normal = normalize(cross(light_tri.v1.position - light_tri.v0.position, light_tri.v2.position - light_tri.v0.position));
    float cos_light = dot(-dir_light, light_normal);
    if(cos_light <= 0.0) return vec3(0);

    //check occlusion of shadow rays
    Ray shadow_ray;
    shadow_ray.origin = h.pos + h.normal * EPS;
    shadow_ray.direction = dir_light;

    Hit shadow_hit;
    
    bool occluded = intersects(shadow_ray, shadow_hit) && shadow_hit.t < dist - EPS;

    if (occluded) return vec3(0);

    //PDF sampling
    float area = triangleArea(light_tri.v0.position, light_tri.v1.position, light_tri.v2.position);
    float pdf = (dist * dist) / (cos_light * area * float(light_count));

    //Direct contribution
    return h.albedo * emission * cos_surface / (PI * pdf);
}

vec3 estimateDirectLight(Hit h, int bounce, int spp_index, uvec2 px) {
    vec3 result = vec3(0);

    if(light_count > 0)
        result += sampleEmissiveTriangles(h, bounce, spp_index, px);

    if(analytic_light_count > 0)
        result += sampleAnalyticLight(h, bounce, spp_index, px);

    return result;
}

//----------------------------------------------------------
//Path Tracer

/**
Traces a path for each pixel and returns the radiance
\param uv pixel coordinates [0,1]
\return vec3 color
\note Uses cosine-weighted sampling for diffuse materials
and Fresnel+Snell for glass materials
\see BACKGROUND, FOCAL_DEBUG, RR_MAX_SURVIVAL
\todo check brdf, deve ser como uma árvore
*/
vec3 pathTrace(vec2 uv, int spp_index, uvec2 px) {
    Ray ray = cameraRayDOF(uv, spp_index, px);
    vec3 color = vec3(0);
    vec3 throughput = vec3(1);

    //foreach ray bounce
    for (int b = 0; b < DEPTH; b++) {
        Hit h;
        if (!intersects(ray, h)) {
            //Background Alternative Colors
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

        if(b == 0) {
            if(FOCAL_DEBUG) {
                float dist_to_focal = abs(h.t - CAM_FOCAL_DISTANCE);
                if(dist_to_focal < FOCAL_BAND_DEBUG)
                    color += vec3(0.0, 1.0, 0.0) * 0.5;
            }

            color += throughput * h.emission;
            if (dot(h.emission, h.emission) > 0.0) break;
        }

        //NEE
        if (USE_NEE == 1 && h.material == MAT_DIFFUSE) {
            color += throughput * estimateDirectLight(h, b, spp_index, px);
        }

        vec3 r = rand3(b, spp_index, px);

        switch(h.material) {
            //Diffuse Materials
            case MAT_DIFFUSE: {
                //Cosine-weighted hemisphere
                float cosT = sqrt(r.x);
                float sinT = sqrt(1.0 - r.x);
                float phi = TWO_PI * r.y;
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
            case MAT_TINTED_GLASS: { //ray passes directly, no Fresnel reflection
                throughput *= h.albedo;
                ray.origin = h.pos + ray.direction * EPS;
                break;
            }
        }

        // Russian Roulette
        if (RR_MIN_BOUNCES > 0 && b >= RR_MIN_BOUNCES) {
            float survival = max(throughput.r, max(throughput.g, throughput.b));

            survival = min(survival, RR_MAX_SURVIVAL);

            //Ends path with P = 1 - survival
            if (rand3(b + DEPTH, spp_index, px).x > survival) break;

            throughput /= survival;
        }
    }
    return color;
}