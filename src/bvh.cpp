#include "bvh.hpp"
#include <stdio.h>
#include <algorithm>
#include <cfloat>

using namespace glm;

//AUX

static vec3 triangle_center(const GPUTriangle& tri) {
    return (tri.v0.position + tri.v1.position + tri.v2.position) / 3.0f;
}

static void calculate_aabb(const vector<GPUTriangle>& tris, int start, int count, vec3& out_min, vec3& out_max) {
    out_min = vec3(FLT_MAX);
    out_max = vec3(-FLT_MAX);
    for (int i = start; i < start + count; i++) {
        for(auto* v : {&tris[i].v0, &tris[i].v1, &tris[i].v2}) {
            out_min = min(out_min, v->position);
            out_max = max(out_max, v->position);
        }
    }
    //padding
    out_min -= vec3(AABB_EPS);
    out_max += vec3(AABB_EPS);
}

///Pre-calculated triangle center stored for runtime
struct TriInfo {
    vec3 center;
    int original_index;
};

//BVH Builder

struct BVHBuilder {
    vector<BVHNode>& nodes;
    vector<GPUTriangle>& tris;
    vector<TriInfo>& tri_info;
    vector<GPUTriangle> scratch;

    BVHBuilder(vector<BVHNode>& n, vector<GPUTriangle>& t, vector<TriInfo>& ti) : nodes(n), tris(t), tri_info(ti) {
        scratch.resize(t.size());
    }
    
    int build(int start, int count) {
        int node_id = nodes.size();
        nodes.push_back({});

        calculate_aabb(tris, start, count, nodes[node_id].aabb_min, nodes[node_id].aabb_max);

        //Leaf size, number of triangles (default =4)
        if(count <= 8) {
            nodes[node_id].left_child = -1;
            nodes[node_id].right_child = -1;
            nodes[node_id].first_tri = start;
            nodes[node_id].tri_count = count;
            return node_id;
        }

        //Split longest axis
        vec3 extent = nodes[node_id].aabb_max - nodes[node_id].aabb_min;
        int axis = 0;
        if(extent.y > extent.x) axis = 1;
        if(extent.z > extent[axis]) axis = 2;

        auto mid_idx = start + count / 2;
        nth_element(tri_info.begin() + start, tri_info.begin() + mid_idx, tri_info.begin() + start + count,
            [axis](const TriInfo& a, const TriInfo& b) {
                return a.center[axis] < b.center[axis];
            }
        );
        
        for (int i = 0; i < count; i++)
        scratch[i] = tris[tri_info[start+i].original_index];
        for (int i = 0; i < count; i++) {
            tris[start + i] = scratch[i];
            tri_info[start + i].original_index = start + i;
        }

        int left_id = build(start, mid_idx - start);
        int right_id = build(mid_idx, start + count - mid_idx);

        nodes[node_id].aabb_min = min(nodes[left_id].aabb_min, nodes[right_id].aabb_min);
        nodes[node_id].aabb_max = max(nodes[left_id].aabb_max, nodes[right_id].aabb_max);

        //need to update node reference
        nodes[node_id].left_child = left_id;
        nodes[node_id].right_child = right_id;
        nodes[node_id].first_tri = -1;
        nodes[node_id].tri_count = 0;

        return node_id;
    }
};

bool buildBVH(vector<GPUTriangle>& triangles, vector<BVHNode>& bvh_nodes) {
    if (triangles.empty()) {
        printf("\t[BVH] No triangles to build\n");
        return false;
    }
    printf("[BVH] Building...\n");
    bvh_nodes.clear();
    bvh_nodes.reserve(triangles.size() * 2);

    //Pre-calculates triangles center
    vector<TriInfo> tri_info(triangles.size());
    for(int i = 0; i < (int)triangles.size(); i++) {
        tri_info[i].center = triangle_center(triangles[i]);
        tri_info[i].original_index = i;
    }
    BVHBuilder builder(bvh_nodes, triangles, tri_info);

    builder.build(0, triangles.size());

    printf("\n\t[BVH] %zu nodes for %zu triangles\n", bvh_nodes.size(), triangles.size());
    return true;
}

bool uploadBVH(const vector<BVHNode>& bvh_nodes, GLuint& ssbo) {
    glCreateBuffers(1, &ssbo);
    glNamedBufferData(ssbo, bvh_nodes.size() * sizeof(BVHNode), bvh_nodes.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, ssbo);
    return true;
}