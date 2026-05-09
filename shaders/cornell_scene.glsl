const vec3 WHITE = vec3(0.90, 0.90, 0.90);
const vec3 RED   = vec3(0.90, 0.05, 0.05);
const vec3 GREEN = vec3(0.05, 0.90, 0.05);


bool intersects_basic(const Ray ray, inout Hit h) {
    float ray_dist;
    vec3  hit_p;

    // Light
    ray_dist = planeT(ray, vec3(0, 0, 1), 1);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (abs(hit_p.x) <= 0.5 && hit_p.y >= -0.5 && hit_p.y <= 0.5) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, -1);
            h.albedo = WHITE;
            h.emission = vec3(5.0);
        }
    }
    // Floor
    ray_dist = planeT_Z(ray, -1.0);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (abs(hit_p.x) <= 1.0 && hit_p.y >= -1.0 && hit_p.y <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, 1);
            h.albedo = WHITE;
            h.emission = vec3(0);
        }
    }
    return h.t < INF;
}

/**
\brief Tests ray intersection with a described scene
\param ray Entry ray
\param h Hit record point
\return true if ray intersected with scene, false if ray escaped
*/
bool intersects_cornell(const Ray ray, out Hit h) {
    h.t        = INF;
    h.pos      = vec3(0);
    h.normal   = vec3(0, 0, 1);
    h.albedo   = WHITE;
    h.emission = vec3(0);
    h.material = MAT_DIFFUSE;
    h.ior      = 0.0;

    float ray_dist;
    vec3  hit_p;

    // Light
    ray_dist = planeT(ray, vec3(0, 0, 1), 1);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (abs(hit_p.x) <= 0.5 && hit_p.y >= -0.5 && hit_p.y <= 0.5) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, -1);
            h.albedo = WHITE;
            h.emission = vec3(5.0);
        }
    }
    // Floor
    ray_dist = planeT_Z(ray, -1.0);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (abs(hit_p.x) <= 1.0 && hit_p.y >= -1.0 && hit_p.y <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, 1);
            h.albedo = WHITE;
            h.emission = vec3(0);
        }
    }

    // Roof
    ray_dist = planeT_Z(ray, 1.0);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (abs(hit_p.x) <= 1.0 && hit_p.y >= -1.0 && hit_p.y <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, -1);
            h.albedo = WHITE;
            h.emission = vec3(0);
        }
    }

    // Front Wall
    ray_dist = planeT_Y(ray, 1.0);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (abs(hit_p.x) <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, -1, 0);
            h.albedo = WHITE;
            h.emission = vec3(0);
        }
    }

    // Left wall
    ray_dist = planeT_X(ray, -1.0);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (hit_p.y >= -1.0 && hit_p.y <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(1, 0, 0);
            h.albedo = RED;
            h.emission = vec3(0);
        }
    }

    // Right Wall
    ray_dist = planeT_X(ray, 1.0);
    if (ray_dist < h.t) {
        hit_p = ray.origin + ray_dist * ray.direction;
        if (hit_p.y >= -1.0 && hit_p.y <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(-1, 0, 0);
            h.albedo = GREEN;
            h.emission = vec3(0);
        }
    }

    // Left sphere
    const vec3 sphere_left_center = vec3(0.20, -0.3, -0.65);
    ray_dist = sphereT(ray, sphere_left_center, 0.35);
    if (ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray.origin + ray_dist * ray.direction;
        h.normal = normalize(h.pos - sphere_left_center);
        h.albedo = WHITE;
        h.emission = vec3(0);
        h.material = MAT_GLASS;
        h.ior = 1.5;
    }

    // Cube
    vec3 box_normal;
    ray_dist = boxT(ray,vec3(-0.50, 0.4, -0.6), vec3(0.30, 0.30, 0.30), -10.0, 30.0, box_normal);
    if(ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray.origin + ray_dist * ray.direction;
        h.normal = box_normal;
        h.albedo = WHITE;
        h.emission = vec3(0);
        h.material = MAT_MIRROR;
        h.ior = 0.0;
    }

    // Right sphere
    const vec3 sphere_right_center = vec3(0.1, -0.0, -0.20);
    ray_dist = sphereT(ray, sphere_right_center, 0.30);
    if (ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray.origin + ray_dist * ray.direction;
        h.normal = normalize(h.pos - sphere_right_center);
        h.albedo = WHITE;
        h.emission = vec3(0);
        h.material = MAT_DIFFUSE;
        h.ior = 0.5;
    }

    // 1 Triangle
    /*
    vec3 tri_normal;
    vec3 tri_bary;
    ray_dist = triangleT(ray, vec3(-0.8, -0.8, -0.6), vec3( 0.8, -0.8, -0.6), vec3( 0.0,  0.8, -0.4), tri_normal, tri_bary);
    if(ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray.origin + ray_dist * ray.direction;
        h.normal = tri_normal;
        h.albedo = 
            vec3(1.0, 0.0, 0.0) * tri_bary.x +
            vec3(0.0, 1.0, 0.0) * tri_bary.y +
            vec3(0.0, 0.0, 1.0) * tri_bary.z;
        h.emission = vec3(0);
        h.material = MAT_TINTED_GLASS;
        h.ior = 1;
    }
    */

/*
    //AABB Early Rejection
    //Converts uniform min/max corners to center/half_size for boxT
    //Avoids O(triangle_count) tests for most rays
    vec3 aabb_center = (mesh_aabb_min + mesh_aabb_max) * 0.5;
    vec3 aabb_half_size = (mesh_aabb_max - mesh_aabb_min) * 0.5;
    vec3 aabb_normal;
    float aabb_t = boxT(ray, aabb_center, aabb_half_size, 0.0, 0.0, aabb_normal);
    
    if (aabb_t < h.t) {
        //Draw Triangle loop for all triangles in buffer
        for (int i = 0; i < triangle_count; i++) {
            vec3 tri_normal, tri_bary;
            float t = triangleT(
                ray,
                triangles[i].v0.position,
                triangles[i].v1.position,
                triangles[i].v2.position,
                tri_normal, tri_bary);
            if(t < h.t) {
                //Interpolate vertices normals with bari for smooth shading
                vec3 smooth_normal = triangles[i].v0.normal * (1.0 - tri_bary.x - tri_bary.y) +
                    triangles[i].v1.normal * tri_bary.x + triangles[i].v2.normal * tri_bary.y;

                int m_id = triangles[i].material_id;
                h.t = t;
                h.pos = ray.origin + t * ray.direction;
                h.normal = length(smooth_normal) > EPS ? normalize(smooth_normal) : tri_normal;
                h.albedo = gpu_materials[m_id].albedo.rgb;
                h.emission = gpu_materials[m_id].emission.rgb;
                h.material = gpu_materials[m_id].type;
                h.ior = gpu_materials[m_id].ior;
            }
        }
    }
*/
    return h.t < INF;
}