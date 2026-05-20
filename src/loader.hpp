/**
 * \file loader.hpp
 * \author Leonel Matos
 * \brief Represents the async model loading to the scene, holding data temporarily
 * \date 2026-05-20
 * \copyright Copyright (c) 2026
 */
#pragma once
#include "mesh.hpp"
#include "bvh.hpp"
#include <atomic>
#include <vector>

struct ModelLoader {
    std::vector<GPUTriangle> pending_tris;
    std::vector<GPUMaterial> pending_mats;
    std::vector<BVHNode> pending_bvh;
    MeshBounds pending_bounds;
    std::atomic<bool> upload_pending = false;
    std::atomic<bool> is_loading = false;
};

inline ModelLoader loader;