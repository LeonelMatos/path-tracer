/**
 * @file bvh.hpp
 * @author Leonel Matos
 * @brief Bounding Volume Hierarchy construction and upload
 * @date 2026-04-02
 * @copyright Copyright (c) 2026
 * Builds a BVH over the scene triangles (through median split on the longest axis of each node) and uploads
 * the flattened node array to SSBO 
 */

#pragma once

#include <vector>
#include <cstddef>
#include <GL/glew.h>
#include <glm/glm.hpp>
#include "mesh.hpp"

///Smap epsilon used to pad AABBs and avoid near-zero thickness
const float AABB_EPS = 1e-4f;

/**
 *\brief A single node of the BVH, structured
 following the std430.
 * 
 Internal nodes reference their children through \ref left_child and
 \ref right_child. Leaf nodes instead describe a run of triangles
 via \ref first_try and \ref tri_count. The `_pad` paddings keep
 the `vec3` bounds aligned to 16 bytes layout.
 *\see buildBVH, uploadBVH
 */
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
static_assert(sizeof(BVHNode) == 48, "BVHNode must match the std430 layout in globals.glsl");
static_assert(offsetof(BVHNode, aabb_max) == 16, "BVHNode.aabb_max offset must match globals.glsl");
static_assert(offsetof(BVHNode, left_child) == 32, "BVHNode.left_child offset must match globals.glsl");

/**
 *\brief Builds the BVH using a triangle list.
 Recursively splits each triangle node at median along the longest axis of its AABB,
 reorders triangles in place so each leaf references a range. The resulting array
 is written to bvh_noes with root at index 0.
 * 
 * @param triangles In-out triangle buffer
 * @param bvh_nodes Output flattened node array
 * @return true on success
 */
bool buildBVH(vector<GPUTriangle>& triangles, vector<BVHNode>& bvh_nodes);

/**
 * @brief Uploads the BVH node array to GPU SSBO.
 Creates the buffer on first use and fills it with bvh_nodes.
 The shader reads this buffer starting from the root idx 0.
 * @param bvh_nodes The node array produced by buildBVH
 * @param ssbo In-out SSBO handle, created if zero
 * @return true on success
 * \see buildBVH
 */
bool uploadBVH(const vector<BVHNode>& bvh_nodes, GLuint& ssbo);