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

/**
 *\brief Loads an HDRI map and uploads it as a GPU texture.
 * @param path Path to the HDRI file (.hdr, .exr)
 * @param renderer Renderer to pass env map
 * @return true if the image was loaded
 */
bool loadEnvMap(const std::string& path, Renderer& renderer);

/**
 * \brief Builds the material texture array from the loaded materials.
 Uploads every material texture into a GPU texture array, and patches 
 each GPUMaterial::tex_index to point at its layer 
 * 
 * @param cpu_materials CPU-sided material data, with texture sources
 * @param gpu_materials In-out GPU material
 * @param renderer Renderer to pass the resulting array
 * @return true on success
 * \see CPUMaterial, GPUMaterials
 */
bool uploadTexture(const vector<CPUMaterial>& cpu_materials, vector<GPUMaterial>& gpu_materials, Renderer& renderer);