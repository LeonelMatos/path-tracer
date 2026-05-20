#include "cornell_scene.glsl"

///Chooses the current render preset
///\note 0 = mesh only; 1 = cornell + mesh; 2 = cornell only
uniform int SCENE_PRESET;

void intersects_mesh(const Ray ray, inout Hit h) {
    //AABB Early Rejection
    //Converts uniform min/max corners to center/half_size for boxT
    //Avoids O(triangle_count) tests for most rays
    vec3 aabb_center = (mesh_aabb_min + mesh_aabb_max) * 0.5;
    vec3 aabb_half_size = (mesh_aabb_max - mesh_aabb_min) * 0.5;
    vec3 aabb_normal;
    float aabb_t = boxT(ray, aabb_center, aabb_half_size, 0.0, 0.0, aabb_normal);

    if(aabb_t >= h.t) return;

    int stack[128];
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
                    h.normal = length(smooth_normal) > EPS_TRI ? normalize(smooth_normal) : tri_normal;
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
            BVHNode left_node = bvh_nodes[node.left_child];
            BVHNode right_node = bvh_nodes[node.right_child];

            vec3 lc = (left_node.aabb_min + left_node.aabb_max) * 0.5;
            vec3 lh = (left_node.aabb_max - left_node.aabb_min) * 0.5;
            vec3 rc = (right_node.aabb_min + right_node.aabb_max) * 0.5;
            vec3 rh = (right_node.aabb_max - right_node.aabb_min) * 0.5;
            vec3 nn;

            float t_left = boxT(ray, lc, lh, 0.0, 0.0, nn);
            float t_right = boxT(ray, rc, rh, 0.0, 0.0, nn);

            bool hit_left = t_left < h.t;
            bool hit_right = t_right < h.t;

            if(hit_left && hit_right) {
                //Push the most distant first; the closest is passed first
                if (t_left < t_right) {
                    stack[stack_top++] = node.right_child;
                    stack[stack_top++] = node.left_child;
                }
                else {
                    stack[stack_top++] = node.left_child;
                    stack[stack_top++] = node.right_child;
                }
            }
            else if (hit_left) {
                stack[stack_top++] = node.left_child;
            }
            else if (hit_right) {
                stack[stack_top++] = node.right_child;
            }
        }
    }
    
    if (USE_BVH_HEATMAP == 1) {
        float heat = clamp(float(nodes_visited) / float(BVH_HEATMAP_SCALE), 0.0, 1.0);
        vec3 cold = vec3(0.0, 0.0, 1.0);
        vec3 warm = vec3(0.0, 1.0, 0.0);
        vec3 hot = vec3(1.0, 0.0, 0.0);

        vec3 heatmap_color = heat < 0.5 ? mix(cold, warm, heat * 2.0) : mix(warm, hot, (heat - 0.5) * 2.0);

        h.t        = max(aabb_t, 0.001);
        h.pos      = ray.origin + h.t * ray.direction;
        h.normal   = vec3(0.0, 1.0, 0.0);
        h.albedo   = heatmap_color;
        h.emission = vec3(0.0);
        h.material = MAT_DIFFUSE;
        h.ior      = 1.0;
    }
    
}

bool intersects(const Ray ray, out Hit h) {
    h.t        = INF;
    h.pos      = vec3(0);
    h.normal   = vec3(0, 0, 1);
    h.albedo   = WHITE;
    h.emission = vec3(0);
    h.material = MAT_DIFFUSE;
    h.ior      = 0.0;

    if(SCENE_PRESET == 1 || SCENE_PRESET == 2)
        intersects_cornell(ray, h);
    if(SCENE_PRESET == 0 || SCENE_PRESET == 1)
        intersects_mesh(ray, h);

    return h.t < INF;
}