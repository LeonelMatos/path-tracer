#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest.h>

#include "bvh.hpp"
#include <random>
#include <functional>
#include <cmath>
#include <algorithm>

///Generates a fixed, reproducible set of random triangles
///to test multiple BVH split levels.
static std::vector<GPUTriangle> makeRandomTriangles(int count, unsigned seed = 42) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> pos(-10.0f, 10.0f);
    std::uniform_real_distribution<float> jitter(-0.5f, 0.5f);

    std::vector<GPUTriangle> tris(count);
    for (int i = 0; i < count; i++) {
        vec3 base(pos(rng), pos(rng), pos(rng));
        tris[i].v0.position = base;
        tris[i].v1.position = base + vec3(jitter(rng), jitter(rng), jitter(rng));
        tris[i].v2.position = base + vec3(jitter(rng), jitter(rng), jitter(rng));
        tris[i].material_id = 0;
    }
    return tris;
}

///Walks every node from the root, doing a check on each with different TEST_CASEs
static void forEachNode(const std::vector<BVHNode>& nodes, int node_id, int depth, const std::function<void(const BVHNode&, int)>& check) {
    if(node_id < 0 || node_id >= (int)nodes.size()) return;
    
    const BVHNode& node = nodes[node_id];
    check(node, depth);
    if(node.left_child >= 0) forEachNode(nodes, node.left_child, depth + 1, check);
    if(node.right_child >= 0) forEachNode(nodes, node.right_child, depth + 1, check);
}

TEST_CASE("BVH: child AABB is contained within its parent's AABB") {
    auto tris = makeRandomTriangles(500);
    std::vector<BVHNode> nodes;
    REQUIRE(buildBVH(tris, nodes));

    const float eps = 1e-3f; // float rounding slack
    forEachNode(nodes, 0, 0, [&](const BVHNode& parent, int) {
        if (parent.left_child < 0) return; //leaf
        for (int child_id : {parent.left_child, parent.right_child}) {
            const BVHNode& child = nodes[child_id];
            CHECK(child.aabb_min.x >= parent.aabb_min.x - eps);
            CHECK(child.aabb_min.y >= parent.aabb_min.y - eps);
            CHECK(child.aabb_min.z >= parent.aabb_min.z - eps);
            CHECK(child.aabb_max.x <= parent.aabb_max.x + eps);
            CHECK(child.aabb_max.y <= parent.aabb_max.y + eps);
            CHECK(child.aabb_max.z <= parent.aabb_max.z + eps);
        }
    });
}

TEST_CASE("BVH: every triangle in a leaf fits inside that leaf's AABB") {
    auto tris = makeRandomTriangles(500);
    std::vector<BVHNode> nodes;
    REQUIRE(buildBVH(tris, nodes)); //reorders tris in place to match leaf ranges

    const float eps = 1e-3f;
    forEachNode(nodes, 0, 0, [&](const BVHNode& node, int) {
        if (node.left_child >= 0) return; //internal node
        for (int i = node.first_tri; i < node.first_tri + node.tri_count; i++) {
            for (const GPUVertex* v : {&tris[i].v0, &tris[i].v1, &tris[i].v2}) {
                CHECK(v->position.x >= node.aabb_min.x - eps);
                CHECK(v->position.y >= node.aabb_min.y - eps);
                CHECK(v->position.z >= node.aabb_min.z - eps);
                CHECK(v->position.x <= node.aabb_max.x + eps);
                CHECK(v->position.y <= node.aabb_max.y + eps);
                CHECK(v->position.z <= node.aabb_max.z + eps);
            }
        }
    });
}

TEST_CASE("BVH: leaf ranges exactly partition [0, triangle_count]") {
    auto tris = makeRandomTriangles(500);
    std::vector<BVHNode> nodes;
    REQUIRE(buildBVH(tris, nodes));

    std::vector<int> covered(tris.size(), 0);
    forEachNode(nodes, 0, 0, [&](const BVHNode& node, int) {
        if (node.left_child >= 0) return;
        for (int i = node.first_tri; i < node.first_tri + node.tri_count; i++) {
            REQUIRE(i >= 0);
            REQUIRE(i < (int)covered.size());
            covered[i]++;
        }
    });
    for (int i = 0; i < (int)covered.size(); i++) {
        CHECK(covered[i] == 1); //every triangle claimed by exactly one leaf
    }
}

TEST_CASE("BVH: tree depth stays within the expected bound for a median split") {
    auto tris = makeRandomTriangles(2000);
    std::vector<BVHNode> nodes;
    REQUIRE(buildBVH(tris, nodes));

    //Longest-axis median split roughly halves the triangle count at every
    //level down to leaves of <=8, so depth should stay close to log2(N/8).
    int max_depth = 0;
    forEachNode(nodes, 0, 0, [&](const BVHNode&, int depth) {
        max_depth = std::max(max_depth, depth);
    });

    int expected = (int)std::ceil(std::log2((double)tris.size() / 8.0)) + 2;
    CHECK(max_depth <= expected);
}

TEST_CASE("BVH: single triangle produces a single leaf root") {
    auto tris = makeRandomTriangles(1);
    std::vector<BVHNode> nodes;
    REQUIRE(buildBVH(tris, nodes));
    REQUIRE(nodes.size() == 1);

    int visits = 0;
    forEachNode(nodes, 0, 0, [&](const BVHNode& node, int depth) {
        visits++;
        CHECK(depth == 0);
        CHECK(node.left_child == -1);
        CHECK(node.right_child == -1);
        CHECK(node.first_tri == 0);
        CHECK(node.tri_count == 1);
    });
    CHECK(visits == 1);
}

TEST_CASE("BVH: empty triangle list is rejected, not crashed on") {
    std::vector<GPUTriangle> tris;
    std::vector<BVHNode> nodes;
    CHECK_FALSE(buildBVH(tris, nodes));
    CHECK(nodes.empty());
}