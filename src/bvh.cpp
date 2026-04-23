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
}

//BVH Builder

struct BVHBuilder {
    vector<BVHNode>& nodes;
    vector<GPUTriangle>& tris;

    BVHBuilder(vector<BVHNode>& n, vector<GPUTriangle>& t) : nodes(n), tris(t) {}
    
    int build(int start, int count) {
        int node_id = nodes.size();
        nodes.push_back({});

        calculate_aabb(tris, start, count, nodes[node_id].aabb_min, nodes[node_id].aabb_max);

        if(count <= 4) {
            nodes[node_id].left_child = -1;
            nodes[node_id].first_tri = start;
            nodes[node_id].tri_count = count;
            return node_id;
        }

        //Split longest axis
        vec3 extent = nodes[node_id].aabb_max - nodes[node_id].aabb_min;
        int axis = 0;
        if(extent.y > extent.x) axis = 1;
        if(extent.z > extent[axis]) axis = 2;

        sort(tris.begin() + start, tris.begin() + start + count, [axis](const GPUTriangle& a, const GPUTriangle& b) {
            return triangle_center(a)[axis] < triangle_center(b)[axis];
        });
        
        int mid = start + count / 2;
        int left_id = build(start, mid - start);
        int right_id = build(mid, start + count - mid);

        //need to update node reference
        nodes[node_id].left_child = left_id;
        nodes[node_id].right_child = right_id;
        nodes[node_id].first_tri = -1;
        nodes[node_id].tri_count = 0;

        (void)right_id;
        return node_id;
    }
};

bool buildBVH(vector<GPUTriangle>& triangles, vector<BVHNode>& bvh_nodes) {
    if (triangles.empty()) {
        printf("\tBVH: No triangles to build\n");
        return false;
    }
    bvh_nodes.clear();
    bvh_nodes.reserve(triangles.size() * 2);

    BVHBuilder builder(bvh_nodes, triangles);
    builder.build(0, triangles.size());

    printf("\tBVH: %zu nodes for %zu triangles\n", bvh_nodes.size(), triangles.size());
    return true;
}

bool uploadBVH(const vector<BVHNode>& bvh_nodes, GLuint& ssbo) {
    glCreateBuffers(1, &ssbo);
    glNamedBufferData(ssbo, bvh_nodes.size() * sizeof(BVHNode), bvh_nodes.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 8, ssbo);
    return true;
}