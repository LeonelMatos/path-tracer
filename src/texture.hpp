#include <string>
#include <vector>
#include "config.hpp"
#include "mesh.hpp"

bool loadEnvMap(const std::string& path, Renderer& renderer);

bool uploadTexture(const vector<CPUMaterial>& cpu_materials, vector<GPUMaterial>& gpu_materials, Renderer& renderer);