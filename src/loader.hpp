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

///\brief Stages of the async model load in order
//\see ModelLoader::stage, loadScene
enum LoadStage {
    STAGE_PARSING = 0,  //reading paths
    STAGE_BVH,          //building BVH
    STAGE_UPLOAD_MESH,  //uploading geometry/BVH/lights SSBOs
    STAGE_UPLOAD_TEXTURES   //decoding, uploading textures (might be the slowest right now)
};

///\brief Display text per LoadStage
///\see LoadStage
inline const char* LOAD_STAGE_TEXT[] = {
    "Loading Model...",
    "Building BVH...",
    "Uploading Geometry...",
    "Loading Textures"
};

struct ModelLoader {
    std::vector<GPUTriangle> pending_tris;
    std::vector<GPUMaterial> pending_mats;
    std::vector<CPUMaterial> pending_cpu_mats;
    std::vector<BVHNode> pending_bvh;
    MeshBounds pending_bounds;
    std::atomic<bool> upload_pending = false;
    std::atomic<bool> is_loading = false;
    std::atomic<int> stage = STAGE_PARSING;
};

inline ModelLoader loader;