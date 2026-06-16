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
//Environment Mapping

vec3 sampleEnvMap(vec3 dir) {
    float phi = atan(dir.y, dir.x);
    float theta = acos(clamp(dir.z, -1.0, 1.0));
    vec2 uv = vec2(phi / TWO_PI + 0.5, theta / PI);
    return texture(env_map, uv).rgb;
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

    ///Adaptive EPS for shadow rays to avoid self-intersection
    float shadow_eps = length(mesh_aabb_max - mesh_aabb_min) * EPS_SHADOW;

    Ray shadow_ray;
    shadow_ray.origin = h.pos + h.geom_normal * shadow_eps;
    shadow_ray.direction = dir_light;

    Hit shadow_hit;
    bool occluded = intersects(shadow_ray, shadow_hit) && shadow_hit.t < dist - shadow_eps && shadow_hit.material != MAT_SHADOW_CATCHER;

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

    ///Adaptive EPS for shadow rays to avoid self-intersection
    float shadow_eps = length(mesh_aabb_max - mesh_aabb_min) * EPS_SHADOW;

    //check occlusion of shadow rays
    Ray shadow_ray;
    shadow_ray.origin = h.pos + h.geom_normal * shadow_eps;
    shadow_ray.direction = dir_light;

    Hit shadow_hit;
    
    bool occluded = intersects(shadow_ray, shadow_hit) && shadow_hit.t < dist - shadow_eps && shadow_hit.material != MAT_SHADOW_CATCHER;

    if (occluded) return vec3(0);

    //PDF sampling
    float area = triangleArea(light_tri.v0.position, light_tri.v1.position, light_tri.v2.position);
    float pdf = (dist * dist) / (cos_light * area * float(light_count));

    //Direct contribution
    return h.albedo * emission * cos_surface / (PI * pdf);
}

/**
\note Because `cos_theta / (PI * pdf)` is 1.0, the last return value can be `return h.albedo * env_color`
But I kept the entire formula
*/
vec3 sampleEnvLight(Hit h, int bounce, int spp_index, uvec2 px) {
    if(USE_ENV_MAP == 0) return vec3(0);

    vec3 r = rand3(bounce + 300, spp_index, px);

    ///Geometric normal safeguard for hits that don't have geom_normal but are forced to use it
    vec3 safe_geom_normal = length(h.geom_normal) > 0.1 ? h.geom_normal : h.normal;

    //Cosine-weighted hemisphere sampling
    float cosT = sqrt(r.x);
    float sinT = sqrt(1.0 - r.x);
    float phi = TWO_PI * r.y;
    vec3 local_dir = vec3(sinT * cos(phi), sinT * sin(phi), cosT);
    vec3 world_dir = onb(h.normal) * local_dir;

    //Shadow ray to check occlusion
    float shadow_eps = length(mesh_aabb_max - mesh_aabb_min) * EPS_SHADOW;
    Ray shadow_ray;
    shadow_ray.origin = h.pos + safe_geom_normal * shadow_eps;
    shadow_ray.direction = world_dir;

    Hit shadow_hit;
    if(intersects(shadow_ray, shadow_hit) && shadow_hit.material != MAT_SHADOW_CATCHER) return vec3(0);

    //Cosine-weighted sampling PDF = cos(theta) / PI
    float cos_theta = max(dot(h.normal, world_dir), 0.0);
    float pdf = cos_theta / PI;
    if (pdf < 1e-4) return vec3(0);

    vec3 env_color = sampleEnvMap(world_dir);
    return h.albedo * env_color * cos_theta / (PI * pdf);
}

//----------------------------------------------------------
//Path Tracer

/**
Used to debug the brute-force path tracer for only the primary ray,
no bounces, no materials, no optimizations, no NEE
\return hit normal in color
*/
vec4 pathTraceDebug(vec2 uv, int spp_index, uvec2 px) {
    Ray ray = cameraRayDOF(uv, spp_index, px);
    Hit h;
    if(!intersects(ray, h)) return vec4(0.0, 0.0, 0.0, 1.0);
    return vec4(h.normal * 0.5 + 0.5, 1.0);
}


/**
Traces a path for each pixel and returns the radiance
\param uv pixel coordinates [0,1]
\return vec3 color
\note Uses cosine-weighted sampling for diffuse materials
and Fresnel+Snell for glass materials
\see BACKGROUND, FOCAL_DEBUG, RR_MAX_SURVIVAL
\todo check brdf, deve ser como uma árvore
*/
vec4 pathTrace(vec2 uv, int spp_index, uvec2 px) {
    Ray ray = cameraRayDOF(uv, spp_index, px);
    vec3 color = vec3(0);
    vec3 throughput = vec3(1);
    float alpha = 1.0;

    int diffuse_bounces = 0;

    //foreach ray bounce
    for (int b = 0; b < DEPTH; b++) {
        Hit h;
        if (!intersects(ray, h)) {
            //Environment Mapping
            if(USE_ENV_MAP == 1)
                color += throughput * sampleEnvMap(ray.direction);
            else {
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
            }
            break;
        }

        //Focal debug only on 1st bounce
        if(b == 0 && FOCAL_DEBUG) {
            float dist_to_focal = abs(h.t - CAM_FOCAL_DISTANCE);
            if(dist_to_focal < FOCAL_BAND_DEBUG)
                color += vec3(0.0, 1.0, 0.0) * 0.5;
        }

        //Avoids double counting with NEE for emission
        if (dot(h.emission, h.emission) > 0.0) {
            if(b == 0 || USE_NEE == 0) 
                color += throughput * h.emission;
            break;
        }

        //NEE
        //Only analytic lights without geometry (point, directional, spot)
        if(USE_NEE == 1 && h.material == MAT_DIFFUSE && analytic_light_count > 0) {
            color += throughput * sampleAnalyticLight(h, b, spp_index, px);
        }

        //NEE on emissive triangles
        if (USE_NEE == 1 && h.material == MAT_DIFFUSE && light_count > 0) {
            color += throughput * sampleEmissiveTriangles(h, b, spp_index, px);
        }

        //NEE on Environment Map HDRI
        //Env map lighting only affects up-to 2 bounces for performance
        if (USE_NEE == 1 && h.material == MAT_DIFFUSE && USE_ENV_MAP == 1 && b < 2) {
            color += throughput * sampleEnvLight(h, b, spp_index, px);
        }

        vec3 r = rand3(b, spp_index, px);

        switch(h.material) {
            //Diffuse Materials
            case MAT_DIFFUSE: {
                //Reduces diffuse bounces without losing quality because of NEE
                if(diffuse_bounces++ >= 2 && USE_NEE == 1) break;

                //Cosine-weighted hemisphere
                float cosT = sqrt(r.x);
                float sinT = sqrt(1.0 - r.x);
                float phi = TWO_PI * r.y;
                ray.direction = onb(h.normal) * vec3(sinT*cos(phi), sinT*sin(phi), cosT);
                throughput *= h.albedo;
                ray.origin = h.pos + h.normal * EPS_TRI;
                break;
            }
            case MAT_MIRROR: {
                ray.direction = reflect(ray.direction, h.normal);
                throughput *= h.albedo;
                ray.origin = h.pos + h.normal * EPS_TRI;
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
                    ray.origin = h.pos + normal * EPS_TRI;
                }
                else {
                    ray.direction = refract(ray.direction, normal, eta);
                    ray.origin = h.pos - normal * EPS_TRI;
                }
                throughput *= h.albedo;
                break;
            }
            case MAT_TINTED_GLASS: { //ray passes directly, no Fresnel reflection
                throughput *= h.albedo;
                ray.origin = h.pos + ray.direction * EPS_TRI;
                break;
            }
            case MAT_SHADOW_CATCHER: {
                float shadow_eps = max(EPS, length(h.pos) * EPS_SHADOW);
                bool in_shadow = false;

                for (int li = 0; li < analytic_light_count; li++) {
                    GPULight light = analytic_lights[li];
                    vec3 to_light = (light.type == LIGHT_DIRECTIONAL) ? -light.direction.xyz : normalize(light.position.xyz - h.pos);
                    float dist = (light.type == LIGHT_DIRECTIONAL) ? 1e10 : length(light.position.xyz - h.pos);

                    Ray shadow_ray;
                    shadow_ray.origin = h.pos + h.geom_normal * shadow_eps;
                    shadow_ray.direction = to_light;
                    Hit shadow_h;

                    if(intersects(shadow_ray, shadow_h) && shadow_h.t < dist - shadow_eps && shadow_h.material != MAT_SHADOW_CATCHER) {
                        in_shadow = true;
                        break;
                    }
                }
                if(in_shadow) {
                    color = vec3(0.0);
                    alpha = GROUND_SHADOW_OPACITY;
                }
                else {
                    alpha = 0.0;
                }
                return vec4(color, alpha);
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

        // Early Termination
        //Mostly works as a safeguard for the RR (removes rays with < 0.1% intensity)
        if(max(throughput.r, max(throughput.g, throughput.b)) < 0.001) break;
    }

    // Firefly Clamp
    if (FIREFLY_CLAMP > 0.0) {
        float lum = dot(color, vec3(0.2126, 0.7152, 0.0722));
        if (lum > FIREFLY_CLAMP)
            color *= FIREFLY_CLAMP / lum;
    }

    return vec4(color, alpha);
}
