#pragma once

#include <vector>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "mesh.hpp"

struct BVHNode {
    vec3 aabb_min;
    float _pad0;
    vec3 aabb_max;
    float _pad1;
    int left_child;
    int right_child;
    int first_tri;
    int tri_count;
};

bool buildBVH(vector<GPUTriangle>& triangles, vector<BVHNode>& bvh_nodes);

bool uploadBVH(const vector<BVHNode>& bvh_nodes, GLuint& ssbo);