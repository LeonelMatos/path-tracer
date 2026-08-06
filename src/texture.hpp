/**
 * @file texture.hpp
 * @author Leonel Matos
 * @brief HDRI environment map and material texture array
 * @date 2026-07-02
 * @copyright Copyright (c) 2026
 */
#include <string>
#include <vector>
#include "config.hpp"
#include "mesh.hpp"

//Using nvtop to profile program VRAM/GPU usage

///Maximum texture array size. Limits VRAM usage forcing large textures to
///fit to a maximum value.
///\note Most PBR packs top out at 4K; raise this if you need higher resolutions
///\note Having the max be 4096 makes the textures somehow not appear. Weird bug, might only be because of running 4GB VRAM on an iGPU
///and have the VRAM.
static const int MAX_TEX_SIZE = 2048;
///Minimum texture array size. Avoids allocation on models with small placeholder textures
static const int MIN_TEX_SIZE = 64;

/**
 *\brief Loads an HDRI map and uploads it as a GPU texture.
 * @param path Path to the HDRI file (.hdr, .exr)
 * @param r Renderer to pass env map
 * @return true if the image was loaded
 */
bool loadEnvMap(const std::string& path, Renderer& r);

/**
 * \brief Builds the material texture array from the loaded materials.
 Uploads every material texture into a GPU texture array, and patches 
 each GPUMaterial::tex_index to point at its layer 
 *\todo Bad handling textures that are not squared (like 128x2048, or 64x512), but forces to be. It's hip to be square
 *\param cpu_materials CPU-sided material data, with texture sources
 *\param gpu_materials In-out GPU material
 *\param renderer Renderer to pass the resulting array
 *\return true on success
 *\see CPUMaterial, GPUMaterials
 */
bool uploadTexture(const vector<CPUMaterial>& cpu_materials, vector<GPUMaterial>& gpu_materials, Renderer& r);