#include "texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include <algorithm>

using namespace std;

bool loadEnvMap(const string& path, Renderer& renderer) {
    stbi_set_flip_vertically_on_load(false);
    int width, height;
    float* data = stbi_loadf(path.c_str(), &width, &height, nullptr, 3);
    if(!data) {
        printf("\n[ENVMAP] Failed to load env map: %s\n", path.c_str());
        return false;
    }

    if(renderer.env_map_tex) glDeleteTextures(1, &renderer.env_map_tex);

    glCreateTextures(GL_TEXTURE_2D, 1, &renderer.env_map_tex);
    glTextureStorage2D(renderer.env_map_tex, 1, GL_RGBA16F, width, height);
    glTextureSubImage2D(renderer.env_map_tex, 0, 0, 0, width, height, GL_RGB, GL_FLOAT, data);
    glTextureParameteri(renderer.env_map_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(renderer.env_map_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(renderer.env_map_tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(renderer.env_map_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);

    glUseProgram(renderer.active_id);
    glBindTextureUnit(2, renderer.env_map_tex);
    glUniform1i(renderer.loc_env_map, 2);
    glUniform1i(renderer.loc_use_env_map, 1);
    renderer.use_env_map = true;

    printf("\n[ENVMAP] %s  (%dx%d)\n", path.c_str(), width, height);
    return true;
}

static bool getTextureDimensions(const CPUMaterial& mat, int& out_w, int& out_h) {
    if(!mat.embedded_data.empty()) {
        out_w = mat.embedded_width;
        out_h = mat.embedded_height;
        return true;
    }
    int channels;
    if(!stbi_info(mat.tex_path.c_str(), &out_w, &out_h, &channels)) {
        printf("\n[TEXTURES] Failed to read dimensions of %s\n", mat.tex_path.c_str());
        return false;
    }
    return true;
}

bool uploadTexture(const vector<CPUMaterial>& cpu_materials, vector<GPUMaterial>& gpu_materials, Renderer& renderer) {
    //Only allocate array layers for materials that actually have a texture
    vector<int> mat_to_layer(cpu_materials.size(), -1);
    int tex_count = 0;
    for (int i = 0; i < (int)cpu_materials.size(); i++) {
        if(cpu_materials[i].has_texture)
            mat_to_layer[i] = tex_count++;
    }

    if (tex_count == 0) {
        printf("\n[TEXTURES] Model has no textures to upload\n");
        return false;
    }

    //Size the array to the largest texture present
    int max_dim = MIN_TEX_SIZE;
    for(auto& m : cpu_materials) {
        if(!m.has_texture) continue;
        int w, h;
        if(!getTextureDimensions(m, w, h)) continue;
        max_dim = std::max({max_dim, w, h});
    }
    max_dim = std::min(max_dim, MAX_TEX_SIZE);

    int tex_size = MIN_TEX_SIZE;
    while(tex_size < max_dim) tex_size <<= 1;

    const int MAX_LAYERS = tex_count;
    int mip_levels = 1 + (int)floor(std::log2(tex_size));

    if(renderer.tex_array)
        glDeleteTextures(1, &renderer.tex_array);

    size_t est_bytes_per_layer = (size_t)tex_size * tex_size * 4;
    size_t est_total = (size_t)(est_bytes_per_layer * (4.0/3.0)) * MAX_LAYERS;
    printf("[TEXTURES] Estimated array VRAM: %.1f MB (%dx%d, %d layers)\n", est_total / 1e6, tex_size,
        tex_size, MAX_LAYERS);

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &renderer.tex_array);
    glTextureStorage3D(renderer.tex_array, mip_levels, GL_SRGB8_ALPHA8, tex_size, tex_size, MAX_LAYERS);
    glTextureParameteri(renderer.tex_array, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(renderer.tex_array, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(renderer.tex_array, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(renderer.tex_array, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //Anisotropic filtering
    GLfloat max_aniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);
    glTextureParameterf(renderer.tex_array, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

    stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < (int)cpu_materials.size(); i++) {
        const CPUMaterial& cpu_mat = cpu_materials[i];
        int layer = mat_to_layer[i];
        if (layer < 0) continue;

        int w, h;
        unsigned char* data = nullptr;
        unsigned char* to_free = nullptr;

        //data already decoded at CPUMaterial
        if(!cpu_mat.embedded_data.empty()) {
            data = const_cast<unsigned char*>(cpu_mat.embedded_data.data());
            w = cpu_mat.embedded_width;
            h = cpu_mat.embedded_height;
        }
        //external texture, load from disk
        else {
            int channels;
            data = stbi_load(cpu_mat.tex_path.c_str(), &w, &h, &channels, 4);
            to_free = data;
            if(!data) {
                printf("\n[TEXTURES] Failed to load %s\n", cpu_mat.tex_path.c_str());
                continue;
            }
        }

        //Texture resize to TEX_SIZE for uniformity
        unsigned char* upload_data = data;
        unsigned char* resized = nullptr;

        if (w != tex_size || h != tex_size) {
            resized = (unsigned char*)malloc(tex_size * tex_size * 4);
            stbir_resize_uint8_srgb(data, w, h, 0, resized, tex_size, tex_size, 0, STBIR_RGBA);
            upload_data = resized;
        }

        //Load to array to layer i
        glTextureSubImage3D(renderer.tex_array, 0, 0, 0, layer, tex_size, tex_size, 1, GL_RGBA, GL_UNSIGNED_BYTE, upload_data);

        //connects mat index to layer
        gpu_materials[i].tex_index = layer;

        if(to_free) stbi_image_free(to_free);
        if(resized) free(resized);

        printf("[TEXTURES] Loaded %d → layer %d: %s (%dx%d)\n", i, layer, cpu_mat.tex_path.c_str(), w, h);
    }

    glGenerateTextureMipmap(renderer.tex_array);

    //Texture bind
    glBindTextureUnit(3, renderer.tex_array);
    glUseProgram(renderer.active_id);
    glUniform1i(renderer.loc_tex_array, 3);

    printf("[TEXTURES] Uploaded %d textures to array (%dx%d, %d layers)\n", tex_count, tex_size, tex_size, MAX_LAYERS);
    return true;
}

