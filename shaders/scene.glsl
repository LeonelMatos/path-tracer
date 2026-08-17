#include "cornell_scene.glsl"

///Chooses the current render preset
///\note 0 = mesh only; 1 = cornell + mesh; 2 = cornell only
uniform int SCENE_PRESET;

void intersects_mesh(const Ray ray, inout Hit h) {
    if(triangle_count == 0) return;

    //AABB Early Rejection
    //Converts uniform min/max corners to center/half_size for boxT
    //Avoids O(triangle_count) tests for most rays
    vec3 aabb_center = (mesh_aabb_min + mesh_aabb_max) * 0.5;
    vec3 aabb_half_size = (mesh_aabb_max - mesh_aabb_min) * 0.5;
    vec3 aabb_normal;
    float aabb_t = boxTAxisAligned(ray, aabb_center, aabb_half_size, aabb_normal);

    if(aabb_t >= h.t) return;

    ///BVH node stack
    ///\note Reduced from 128, since the worst case scenario with millions of triangles will have a
    ///depth of ~20-28 
    int stack[32];
    int stack_top = 0;
    int node_id = 0;

    int nodes_visited = 0, tris_tested = 0;

    while(true) {
        BVHNode node = bvh_nodes[node_id];
        nodes_visited++;

        if (node.tri_count > 0) {
            tris_tested += node.tri_count;
            for (int i = node.first_tri; i < node.first_tri + node.tri_count; i++) {
                vec3 tri_normal, tri_bary;
                float t = triangleT(ray, triangles[i].v0.position, triangles[i].v1.position, triangles[i].v2.position, tri_normal, tri_bary);

                if (t < h.t) {
                    int m_id = triangles[i].material_id;

                    vec2 uv = triangles[i].v0.texcoord * tri_bary.z + triangles[i].v1.texcoord * tri_bary.x + triangles[i].v2.texcoord * tri_bary.y;
 
                    if(USE_TEXTURES == 1 && gpu_materials[m_id].tex_index >= 0) {
                        if (texture(tex_albedo, vec3(uv, float(gpu_materials[m_id].tex_index))).a < 0.5)
                            continue;
                    }

                    vec3 smooth_normal = triangles[i].v0.normal * tri_bary.z + triangles[i].v1.normal * tri_bary.x + triangles[i].v2.normal * tri_bary.y;

                    h.t = t;
                    h.pos = ray.origin + t * ray.direction;
                    h.normal = length(smooth_normal) > EPS_TRI ? normalize(smooth_normal) : tri_normal;
                    h.geom_normal = tri_normal;
                    h.emission = gpu_materials[m_id].emission.rgb;
                    h.material = gpu_materials[m_id].type;
                    if(FORCE_MATERIAL >= 0) {
                        h.material = FORCE_MATERIAL;
                    }
                    h.ior = gpu_materials[m_id].ior;
                    h.light_area = triangleArea(triangles[i].v0.position, triangles[i].v1.position, triangles[i].v2.position);

                    //Texture sample
                    if(USE_TEXTURES == 1 && gpu_materials[m_id].tex_index >= 0) {
                        h.albedo = texture(tex_albedo, vec3(uv, float(gpu_materials[m_id].tex_index))).rgb;
                    }
                    else {
                        h.albedo = gpu_materials[m_id].albedo.rgb;
                    }
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

            float t_left = boxTAxisAligned(ray, lc, lh, nn);
            float t_right = boxTAxisAligned(ray, rc, rh, nn);

            bool hit_left = t_left < h.t;
            bool hit_right = t_right < h.t;

            ///BVH descends directly to the nearer child, avoids stack traffic;
            ///pushes only the farther one, only if it was hit too
            int near_child = node.left_child, far_child = node.right_child;
            bool hit_near = hit_left, hit_far = hit_right;

            if(t_right < t_left) {
                near_child = node.right_child;
                far_child = node.left_child;
                hit_near = hit_right;
                hit_far = hit_left;
            }
            if (hit_near) {
                if(hit_far) stack[stack_top++] = far_child;
                node_id = near_child;
                continue;
            }
            if (hit_far) {
                node_id = far_child;
                continue;
            }
        }
        if(stack_top == 0) break;
        node_id = stack[--stack_top];
    }
    
    if (USE_BVH_HEATMAP == 1) {
        float heat = clamp(float(nodes_visited) / float(BVH_HEATMAP_SCALE), 0.0, 1.0);
        vec3 cold = vec3(0.0, 0.0, 1.0);
        vec3 warm = vec3(0.0, 1.0, 0.0);
        vec3 hot = vec3(1.0, 0.0, 0.0);

        vec3 heatmap_color = heat < 0.5 ? mix(cold, warm, heat * 2.0) : mix(warm, hot, (heat - 0.5) * 2.0);

        h.t         = max(aabb_t, 0.001);
        h.pos       = ray.origin + h.t * ray.direction;
        h.normal    = vec3(0.0, 1.0, 0.0);
        h.albedo    = vec3(0.0);
        h.emission  = heatmap_color;
        h.material  = MAT_DIFFUSE;
        h.ior       = 1.0;
    }
}

bool intersects(const Ray ray, out Hit h) {
    h.t         = INF;
    h.pos       = vec3(0);
    h.normal    = vec3(0, 0, 1);
    h.geom_normal = vec3(0, 0, 1);
    h.albedo    = WHITE;
    h.emission  = vec3(0);
    h.material  = MAT_DIFFUSE;
    h.ior       = 0.0;
    h.light_area = 0.0;

    if(SCENE_PRESET == 1 || SCENE_PRESET == 2)
        intersects_cornell(ray, h);
    if(SCENE_PRESET == 0 || SCENE_PRESET == 1 || SCENE_PRESET == 3)
        intersects_mesh(ray, h);

    if(USE_GROUND_PLANE == 1) {
        float ray_dist = planeT_Z(ray, GROUND_ELEVATION);
        if(ray_dist > 0.0 && ray_dist < h.t) {
            vec3 hit_plane = ray.origin + ray_dist * ray.direction;
            float dist_from_center = length(hit_plane.xy);
            
            if(dist_from_center < GROUND_RADIUS) {
                h.t = ray_dist;
                h.pos = hit_plane;
                h.normal = vec3(0,0,1);
                h.geom_normal = vec3(0,0,1);
                h.albedo = vec3(GROUND_ALBEDO);
                h.emission = vec3(0);
                h.material = (GROUND_SHADOW_CATCHER == 1) ? MAT_SHADOW_CATCHER : MAT_DIFFUSE;
                h.ior = 0.0;
            } 
        }
    }

    return h.t < INF;
}