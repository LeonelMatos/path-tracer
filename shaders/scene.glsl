#include "cornell_scene.glsl"

void intersects_mesh(const Ray ray, inout Hit h) {
    //AABB Early Rejection
    //Converts uniform min/max corners to center/half_size for boxT
    //Avoids O(triangle_count) tests for most rays
    vec3 aabb_center = (mesh_aabb_min + mesh_aabb_max) * 0.5;
    vec3 aabb_half_size = (mesh_aabb_max - mesh_aabb_min) * 0.5;
    vec3 aabb_normal;
    float aabb_t = boxT(ray, aabb_center, aabb_half_size, 0.0, 0.0, aabb_normal);

    if(aabb_t >= h.t) return;

    int stack[32];
    int stack_top = 0;
    stack[stack_top++] = 0;

    int nodes_visited = 0, tris_tested = 0;

    while(stack_top > 0) {
        int node_id = stack[--stack_top];
        BVHNode node = bvh_nodes[node_id];
        nodes_visited++;

        vec3 node_center = (node.aabb_min + node.aabb_max) * 0.5;
        vec3 node_half_size = (node.aabb_max - node.aabb_min) * 0.5;
        vec3 node_normal;
        float node_t = boxT(ray, node_center, node_half_size, 0.0, 0.0, node_normal); //remove local axis orientation with pitch+yaw for optimization

        if (node_t >= h.t) continue;

        if (node.tri_count > 0) {
            tris_tested += node.tri_count;
            for (int i = node.first_tri; i < node.first_tri + node.tri_count; i++) {
                vec3 tri_normal, tri_bary;
                float t = triangleT(ray, triangles[i].v0.position, triangles[i].v1.position, triangles[i].v2.position, tri_normal, tri_bary);

                if (t < h.t) {
                    int m_id = triangles[i].material_id;
                    vec3 smooth_normal = triangles[i].v0.normal * tri_bary.z + triangles[i].v1.normal * tri_bary.x +
                        triangles[i].v2.normal * tri_bary.y;
                    
                    h.t = t;
                    h.pos = ray.origin + t * ray.direction;
                    h.normal = length(smooth_normal) > EPS ? normalize(smooth_normal) : tri_normal;
                    if (dot(ray.direction, h.normal) > 0.0)
                        h.normal = -h.normal;
                    h.albedo = gpu_materials[m_id].albedo.rgb;
                    h.emission = gpu_materials[m_id].emission.rgb;
                    h.material = gpu_materials[m_id].type;
                    h.ior = gpu_materials[m_id].ior;
                }
            }
        }
        else {
            stack[stack_top++] = node.left_child;
            stack[stack_top++] = node.right_child;
        }
    }
    /*
    if (nodes_visited > 0 && h.t >= INF) {
        h.t        = aabb_t;
        h.pos      = ray.origin + aabb_t * ray.direction;
        h.normal   = aabb_normal;
        // Verde = visitou nós mas não acertou triângulos
        h.albedo   = vec3(0, float(nodes_visited) / 20.0, 0);
        h.emission = vec3(0);
        h.material = MAT_DIFFUSE;
        h.ior      = 0.0;
    }*/
}

bool intersects(const Ray ray, out Hit h) {
    h.t        = INF;
    h.pos      = vec3(0);
    h.normal   = vec3(0, 0, 1);
    h.albedo   = WHITE;
    h.emission = vec3(0);
    h.material = MAT_DIFFUSE;
    h.ior      = 0.0;

    intersects_basic(ray, h);
    intersects_mesh(ray, h);

    return h.t < INF;
}