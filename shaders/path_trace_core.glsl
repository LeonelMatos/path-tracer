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
\param fov_scale Multiplier on CAM_FOV, so callers can perturb it per color/colour channel
\return Normalized direction of ray in worldspace
\see CAM_DISTORTION_K1, CAM_PROJECTION_MODE
*/
vec3 cameraRay(vec2 uv, vec3 cam_x, vec3 cam_y, vec3 cam_z, float fov_scale) {
    float aspect = resolution.x / resolution.y;
    vec2 p = (2.0 * uv - 1.0) * vec2(aspect, 1.0);

    //Radial lens distortion, applied in aspect-corrected space
    if(CAM_DISTORTION_K1 != 0.0 || CAM_DISTORTION_K2 != 0.0) {
        float r2 = dot(p, p);
        p *= 1.0 + CAM_DISTORTION_K1 * r2 + CAM_DISTORTION_K2 * r2 * r2;
    }

    float half_fov = 0.5 * CAM_FOV * fov_scale;

    if(CAM_PROJECTION_MODE == PROJ_RECTILINEAR) {
        float f_len = 1.0 / tan(half_fov);
        vec3 ray_cam = vec3(p.x, p.y, -f_len);
        return normalize(cam_x * ray_cam.x + cam_y * ray_cam.y + cam_z * ray_cam.z);
    }

    //Fisheye projection map image-plane
    float r = length(p);
    if(r < EPS) return -cam_z;

    float theta;
    if(CAM_PROJECTION_MODE == PROJ_FISHEYE_EQUIDISTANT) {
        theta = r * half_fov;
    }
    else if (CAM_PROJECTION_MODE == PROJ_FISHEYE_STEREOGRAPHIC) {
        theta = 2.0 * atan(r * tan(half_fov * 0.5));
    }
    else { //PROJ_FISHEYE_EQUISOLID
        theta = 2.0 * asin(clamp(r * sin(half_fov * 0.5), -1.0, 1.0));
    }

    vec2 dir2d = p / r;
    vec3 local_dir = vec3(dir2d * sin(theta), -cos(theta));

    return normalize(cam_x * local_dir.x + cam_y * local_dir.y + cam_z * local_dir.z);
}

/**\brief Max radius of a regular N-gon at a given angle
\param blades number of aperture blades, <3 returns 1.0 (circular)
\note Standart "polygon bokeh": r(θ) = cos(π/n) / cos(θ' - π/n), where θ' is θ measured relative to the nearest edge's midpoint.
*/
float polygonAperture(float angle, int blades) {
    if(blades < 3) return 1.0;
    float corner = TWO_PI / float(blades);
    float a = mod(angle, corner) - corner * 0.5;
    return cos(corner * 0.5) / cos(a);
}

/**\brief Samples a point on the camera's aperture (lens disk)
Shaped by CAM_APERTURE_BLADES/CAM_BLADE_ROTATION/CAM_ANAMORPHIC_SQUEEZE, and
is biased towards the frame edges (behaviour like mechanical vignette) by CAM_CATEYE_STRENGTH
\param uv pixel coordinates [0,1], used only for cat's-eye
\param angle random angle around the lens
\param radius_rand random [0,1[ for the radius, distributed in uniform disk density using the sqrt
\return world-space offset from camera_position, on the lens plane
*/
vec3 sampleAperture(vec3 cam_x, vec3 cam_y, vec2 uv, float angle, float radius_rand) {
    float shape = polygonAperture(angle + CAM_BLADE_ROTATION, CAM_APERTURE_BLADES);
    float radius = sqrt(radius_rand) * CAM_APERTURE * shape;

    vec2 disk = vec2(cos(angle), sin(angle)) * radius;
    disk.x *= CAM_ANAMORPHIC_SQUEEZE;

    //Cat's-eye: biases the sample towards the center, proportional to how far off-axis the pixel is.
    //This method is an approximation, not a real occlusion test, but it's cheap and doesn't add noise
    if(CAM_CATEYE_STRENGTH > 0.0) {
        vec2 frame_offset = (uv - 0.5) * 2.0;
        disk -= frame_offset * length(frame_offset) * CAM_CATEYE_STRENGTH * CAM_APERTURE;
    }

    return cam_x * disk.x + cam_y * disk.y;
}

/**\brief Point on the (maybe tilted) focal plane that a given ray focuses on
\note CAM_TILT==0 keeps the original sphere-approx behaviour
\see CAM_TILT
*/
vec3 focalPlanePoint(vec3 cam_x, vec3 cam_y, vec3 cam_z, vec3 ray_dir, float focal_distance) {
    if(CAM_TILT == 0.0) {
        return camera_position + ray_dir * focal_distance;
    }

    vec3 plane_point = camera_position + cam_z * focal_distance;
    vec3 plane_normal = cam_z * cos(CAM_TILT) + cam_y * sin(CAM_TILT);

    float denom = dot(ray_dir, plane_normal);
    if(abs(denom) < EPS)
        return camera_position + ray_dir * focal_distance;
    
    float t = dot(plane_point - camera_position, plane_normal) / denom;
    return camera_position + ray_dir * t;
}

/**\brief Creates a ray with depth of field, using one channel's lens parameters
\return Ray with origin at the lens directed to the disp_coefficient's directed focal point
\see cameraRayDOF, CAM_LATERAL_CA, CAM_AXIAL_CA
*/
Ray cameraRayDOFChannel(vec2 uv, int spp_index, uvec2 px, float disp_coeff) {
    vec3 cam_x, cam_y, cam_z;
    camera_axes(cam_x, cam_y, cam_z);

    float fov_scale = 1.0 + CAM_LATERAL_CA * disp_coeff;
    float focal_distance = CAM_FOCAL_DISTANCE * (1.0 + CAM_AXIAL_CA * disp_coeff);

    vec3 rj = rand3(-2, spp_index, px);
    vec2 jitter = (rj.xz - 0.5) / resolution;

    //Focal point definition
    vec3 base_dir = cameraRay(uv + jitter, cam_x, cam_y, cam_z, fov_scale);
    vec3 focal_point = focalPlanePoint(cam_x, cam_y, cam_z, base_dir, focal_distance);

    //Lens disk
    float angle = rj.x * TWO_PI;
    vec3 rDOF = rand3(-3, spp_index, px); //needs different random seed
    vec3 lens_offset = sampleAperture(cam_x, cam_y, uv, angle, rDOF.x);

    vec3 origin = camera_position + lens_offset;
    return Ray(origin, normalize(focal_point - origin));
}

/**
\brief Creates a ray with depth of field
\param uv pixel coordinates [0,1]
\return Ray with origin at the lens directed to the focal point
\note random jitter generated here, for each spp
\note This is a wrapper around cameraRayDOFChannel(...1) so every caller keeps the pre-chromatic dispersion
\see CAM_APERTURE, CAM_FOCAL_DISTANCE
*/
Ray cameraRayDOF(vec2 uv, int spp_index, uvec2 px) {
    return cameraRayDOFChannel(uv, spp_index, px, 0.0);
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
    float pdf_dir = 1.0; //Directional-density PDF. 1.0 = delta (point/spot or directional with radius=0)

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
            vec3 sun_dir = -light.direction.xyz;
            if(light.radius > 0.0) {
                to_light = sampleCone(sun_dir, light.radius, rnd.yz);
                pdf_dir = conePDF(light.radius);
            }
            else {
                to_light = sun_dir;
            }
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

    float pdf_select = 1.0 / float(analytic_light_count);
    float pdf_light = pdf_select * pdf_dir;
    if (pdf_light <= 0.0) return vec3(0);

    ///MIS only applies where a BSDF sampled ray could land here
    ///Like a directional light with real angular extent. Delta light (point, spot, 0-radius sun)
    ///get full weight
    float weight = 1.0;
    if(light.type == LIGHT_DIRECTIONAL && light.radius > 0.0) {
        float pdf_bsdf = bsdfPDF_diffuse(h.normal, dir_light);
        weight = powerHeuristic(pdf_light, pdf_bsdf);
    }

    //Direct contribution
    return h.albedo * light.emission.rgb * cos_surface * weight / (PI * pdf_light);
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
    float pdf_light = trianglelightPDF(dist, cos_light, area);
    if(pdf_light <= 0.0) return vec3(0);

    //MIS weight
    float pdf_bsdf = bsdfPDF_diffuse(h.normal, dir_light);
    float weight = powerHeuristic(pdf_light, pdf_bsdf);

    //Direct contribution
    return h.albedo * emission * cos_surface / (PI * pdf_light);
}

/**
\note Because `cos_theta / (PI * pdf)` is 1.0, the last return value can be `return h.albedo * env_color`
But I kept the entire formula
*/
vec3 sampleEnvLight(Hit h, int bounce, int spp_index, uvec2 px) {
    if(USE_ENV_MAP == 0) return vec3(0);

    vec3 r = rand3(bounce + 300, spp_index, px);

    //Cosine-weighted hemisphere sampling
    float cosT = sqrt(r.x);
    float sinT = sqrt(1.0 - r.x);
    float phi = TWO_PI * r.y;
    vec3 local_dir = vec3(sinT * cos(phi), sinT * sin(phi), cosT);
    vec3 world_dir = onb(h.normal) * local_dir;

    //Shadow ray to check occlusion
    float shadow_eps = length(mesh_aabb_max - mesh_aabb_min) * EPS_SHADOW;
    Ray shadow_ray;
    shadow_ray.origin = h.pos + h.geom_normal * shadow_eps;
    shadow_ray.direction = world_dir;

    Hit shadow_hit;

    bool occluded = intersects(shadow_ray, shadow_hit) && shadow_hit.material != MAT_SHADOW_CATCHER;

    if (occluded) return vec3(0);

    vec3 env_color = sampleEnvMap(world_dir);

    //NEE and the diffuse BSDF sample the same cosine-weight hemisphere, so their PDFs are the same
    //Means that the powerHeuristic is always 0.5 here
    return h.albedo * env_color * 0.5;
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
\brief Traces a single already-generated path through the bounce loop and returns the radiance.
Extracted from pathTrace() so the exact same bounce logic can be called per color channel, hero-sampling style.
\param channel to which this ray is dedicated to (0=R, 1=G, 2=B), for MAT_GLASS
\return vec3 color
\note Uses cosine-weighted sampling for diffuse materials
and Fresnel+Snell for glass materials
\see pathTrace, BACKGROUND, FOCAL_DEBUG, RR_MAX_SURVIVAL
*/
vec4 pathTraceFromRay(Ray ray, int spp_index, uvec2 px, float disp_coeff) {
    vec3 color = vec3(0);
    vec3 throughput = vec3(1);
    float alpha = 1.0;

    int diffuse_bounces = 0;

    ///PDF of the direction the BSDF sampler chose at the previous vertex. < 0 == no valid competing NEE strategy
    ///Means that emission hit directly so it gets full weight instead of MIS-weighted
    float prev_bsdf_pdf = -1.0;

    //foreach ray bounce
    for (int b = 0; b < DEPTH; b++) {
        Hit h;
        if (!intersects(ray, h)) {
            for (int li = 0; li < analytic_light_count; li++) {
                GPULight light = analytic_lights[li];
                if(light.type != LIGHT_DIRECTIONAL) continue;
                float cos_angle = dot(ray.direction, -light.direction.xyz);
                if(cos_angle > cos(light.radius)) {
                    float mis_weight = 1.0;
                    if (light.radius > 0.0 && prev_bsdf_pdf >= 0.0) {
                        float pdf_light = conePDF(light.radius) / float(analytic_light_count);
                        mis_weight = powerHeuristic(prev_bsdf_pdf, pdf_light);
                    }
                    color += throughput * light.emission.rgb * mis_weight;
                }
            }

            //Environment Mapping
            if(USE_ENV_MAP == 1) {
                float mis_weight = (prev_bsdf_pdf >= 0.0 && b < 3) ? 0.5 : 1.0;
                color += throughput * sampleEnvMap(ray.direction) * mis_weight;
            }
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
        //MIS, full weight for the primary ray or after specular bounce
        //otherwise weighted against what NEE PDF would be for this hit
        if (dot(h.emission, h.emission) > 0.0) {
            float mis_weight = 1.0;
            if(prev_bsdf_pdf >= 0.0) {
                float cos_light = dot(h.geom_normal, -ray.direction);
                float pdf_light = trianglelightPDF(h.t, cos_light, h.light_area);
                mis_weight = (pdf_light > 0.0) ? powerHeuristic(prev_bsdf_pdf, pdf_light) : 1.0;
            }
            color += throughput * h.emission * mis_weight;
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

        //Used by MAT_DIFFUSE when the bounce cap is reached
        //\note break inside the switch only exits the switch, not the for-loop,
        //the end_path flag is used to terminate the path instead of leaving to re-hit the same point
        bool end_path = false;

        switch(h.material) {
            //Diffuse Materials
            case MAT_DIFFUSE: {
                //Reduces diffuse bounces without losing quality because of NEE
                if(diffuse_bounces++ >= 2 && USE_NEE == 1) {
                    end_path = true;
                }

                //Cosine-weighted hemisphere
                float cosT = sqrt(r.x);
                float sinT = sqrt(1.0 - r.x);
                float phi = TWO_PI * r.y;
                ray.direction = onb(h.normal) * vec3(sinT*cos(phi), sinT*sin(phi), cosT);
                throughput *= h.albedo;
                ray.origin = h.pos + h.normal * EPS_TRI;
                prev_bsdf_pdf = bsdfPDF_diffuse(h.normal, ray.direction);
                break;
            }
            case MAT_MIRROR: {
                ray.direction = reflect(ray.direction, h.normal);
                throughput *= h.albedo;
                ray.origin = h.pos + h.normal * EPS_TRI;
                prev_bsdf_pdf = -1.0;
                break;
            }
            case MAT_GLASS: {
                bool h_entering = dot(ray.direction, h.normal) < 0.0;
                vec3 normal = h_entering ? h.normal : -h.normal;

                //Dispersion. This trace is dedicated to a channel's wavelength
                //so every refraction it does uses the respective channel's IOR, same idea as cameraRayDOFChannel(),
                //but applied to glass mat.
                float ior = h.ior * (1.0 + GLASS_DISPERSION * disp_coeff);
                float eta = h_entering ? (1.0 / ior) : ior;

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
                prev_bsdf_pdf = -1.0;
                break;
            }
            case MAT_TINTED_GLASS: { //ray passes directly, no Fresnel reflection
                throughput *= h.albedo;
                ray.origin = h.pos + ray.direction * EPS_TRI;
                prev_bsdf_pdf = -1.0;
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

        if (end_path) break;

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

vec4 pathTrace(vec2 uv, int spp_index, uvec2 px) {
    if(CAM_LATERAL_CA <= 0.0 && CAM_AXIAL_CA <= 0.0 && GLASS_DISPERSION <= 0.0) {
        Ray ray = cameraRayDOF(uv, spp_index, px);
        return pathTraceFromRay(ray, spp_index, px, 0.0);
    }

    vec3 traces[3];
    float final_alpha = 0.0;
    for(int c = 0; c < 3; c++) {
        float jitter = (rand3(-10 - c, spp_index, px).x - 0.5) * DISPERSION_JITTER_WIDTH;
        float disp_coeff = DISPERSION_COEFF[c] + jitter;

        Ray ray = cameraRayDOFChannel(uv, spp_index, px, disp_coeff);
        vec4 result = pathTraceFromRay(ray, spp_index, px, disp_coeff);
        traces[c] = result.rgb;
        final_alpha += result.a;
    }

    vec3 rc = traces[0], gc = traces[1], bc = traces[2];
    vec3 final_color;
    final_color.r = 0.70 * rc.r + 0.25 * gc.r + 0.05 * bc.r;
    final_color.g = 0.15 * rc.g + 0.70 * gc.g + 0.16 * bc.g;
    final_color.b = 0.05 * rc.b + 0.25 * gc.b + 0.70 * bc.b;

    return vec4(final_color, final_alpha / 3.0);
}