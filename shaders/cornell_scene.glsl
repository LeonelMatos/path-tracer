const vec3 WHITE = vec3(0.90, 0.90, 0.90);
const vec3 RED   = vec3(0.90, 0.05, 0.05);
const vec3 GREEN = vec3(0.05, 0.90, 0.05);

const vec3 camera_position = vec3(0.0, -5.0, 0.0);
const vec3 camera_lookat   = vec3(0.0,  0.0, 0.0);
const vec3 camera_up       = vec3(0.0,  0.0, 1.0);

bool intersects(vec3 ray_origin, vec3 ray_dir, out Hit h) {
    h.t        = INF;
    h.pos      = vec3(0);
    h.normal   = vec3(0, 0, 1);
    h.albedo   = WHITE;
    h.emission = vec3(0);
    h.material = MAT_DIFFUSE;
    h.ior      = 0.0;

    float ray_dist;
    vec3  hit_p;

    // Floor
    ray_dist = planeT(ray_origin, ray_dir, vec3(0, 0, 1), -1.0);
    if (ray_dist < h.t) {
        hit_p = ray_origin + ray_dist * ray_dir;
        if (abs(hit_p.x) <= 1.0 && hit_p.y >= -1.0 && hit_p.y <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, 1);
            h.albedo = WHITE;
            h.emission = vec3(0);
        }
    }

    // Roof
    ray_dist = planeT(ray_origin, ray_dir, vec3(0, 0, -1), -1.0);
    if (ray_dist < h.t) {
        hit_p = ray_origin + ray_dist * ray_dir;
        if (abs(hit_p.x) <= 1.0 && hit_p.y >= -1.0 && hit_p.y <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, -1);
            h.albedo = WHITE;
            h.emission = vec3(0);
        }
    }

    // Front Wall
    ray_dist = planeT(ray_origin, ray_dir, vec3(0, -1, 0), -1.0);
    if (ray_dist < h.t) {
        hit_p = ray_origin + ray_dist * ray_dir;
        if (abs(hit_p.x) <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, -1, 0);
            h.albedo = WHITE;
            h.emission = vec3(0);
        }
    }

    // Left wall
    ray_dist = planeT(ray_origin, ray_dir, vec3(1, 0, 0), -1.0);
    if (ray_dist < h.t) {
        hit_p = ray_origin + ray_dist * ray_dir;
        if (hit_p.y >= -1.0 && hit_p.y <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(1, 0, 0);
            h.albedo = RED;
            h.emission = vec3(0);
        }
    }

    // Right Wall
    ray_dist = planeT(ray_origin, ray_dir, vec3(-1, 0, 0), -1.0);
    if (ray_dist < h.t) {
        hit_p = ray_origin + ray_dist * ray_dir;
        if (hit_p.y >= -1.0 && hit_p.y <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(-1, 0, 0);
            h.albedo = GREEN;
            h.emission = vec3(0);
        }
    }

    // Light
    /*const vec3 light_center = vec3(0.0, 0.0, 0.80);
    ray_dist = sphereT(ray_origin, ray_dir, light_center, 0.20);
    if (ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray_origin + ray_dist * ray_dir;
        h.normal = normalize(h.pos - light_center);
        h.albedo = vec3(0);
        h.emission = vec3(10.0);
    }
    */

    // Light 2
    ray_dist = planeT(ray_origin, ray_dir, vec3(0, 0, -1), -0.99);
    if (ray_dist < h.t) {
        hit_p = ray_origin + ray_dist * ray_dir;
        if (abs(hit_p.x) <= 0.5 && hit_p.y >= -0.5 && hit_p.y <= 0.5) {
            h.t = ray_dist;
            h.pos = hit_p;
            h.normal = vec3(0, 0, -1);
            h.albedo = WHITE;
            h.emission = vec3(5.0);
        }
    }

    // Left sphere
    /*const vec3 sphere_left_center = vec3(-0.5, 0.0, -0.65);
    ray_dist = sphereT(ray_origin, ray_dir, sphere_left_center, 0.35);
    if (ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray_origin + ray_dist * ray_dir;
        h.normal = normalize(h.pos - sphere_left_center);
        h.albedo = WHITE;
        h.emission = vec3(0);
        h.material = MAT_DIFFUSE;
        h.ior = 0.0;
    }*/

    // Cube
    vec3 box_normal;
    ray_dist = boxT(ray_origin, ray_dir,
                    vec3(-0.40, -0.40, -0.6),
                    vec3(0.30, 0.30, 0.40),
                    0.0, 15.0,
                    box_normal);
    if(ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray_origin + ray_dist * ray_dir;
        h.normal = box_normal;
        h.albedo = WHITE;
        h.emission = vec3(0);
        h.material = MAT_DIFFUSE;
        h.ior = 0.0;
    }

    // Right sphere
    const vec3 sphere_right_center = vec3(0.3, 0.5, -0.50);
    ray_dist = sphereT(ray_origin, ray_dir, sphere_right_center, 0.50);
    if (ray_dist < h.t) {
        h.t = ray_dist;
        h.pos = ray_origin + ray_dist * ray_dir;
        h.normal = normalize(h.pos - sphere_right_center);
        h.albedo = RED;
        h.emission = vec3(0);
        h.material = MAT_MIRROR;
        h.ior = 1.5;
    }

    return h.t < INF;
}