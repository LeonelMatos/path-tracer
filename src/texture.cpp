#include "texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"

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

bool uploadTexture(const vector<CPUMaterial>& cpu_materials, vector<GPUMaterial>& gpu_materials, Renderer& renderer) {
    ///Forces the loaded textures to be this size \todo dynamically change the tex size
    const int TEX_SIZE = 4096;
    const int MAX_LAYERS = (int)cpu_materials.size();
    int mip_levels = 1 + (int)floor(std::log2(TEX_SIZE));

    int tex_count = 0;
    for (auto& m : cpu_materials)
        if(m.has_texture) tex_count++;

    if (tex_count == 0) {
        printf("\n[TEXTURES] Model has no textures to upload\n");
        return false;
    }

    if(renderer.tex_array)
        glDeleteTextures(1, &renderer.tex_array);

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &renderer.tex_array);
    glTextureStorage3D(renderer.tex_array, mip_levels, GL_RGBA8, TEX_SIZE, TEX_SIZE, MAX_LAYERS);
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
        if (!cpu_mat.has_texture) continue;

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

        if (w != TEX_SIZE || h != TEX_SIZE) {
            resized = (unsigned char*)malloc(TEX_SIZE * TEX_SIZE * 4);
            stbir_resize_uint8_linear(data, w, h, 0, resized, TEX_SIZE, TEX_SIZE, 0, STBIR_RGBA);
            upload_data = resized;
        }

        //Load to array to layer i
        glTextureSubImage3D(renderer.tex_array, 0, 0, 0, i, TEX_SIZE, TEX_SIZE, 1, GL_RGBA, GL_UNSIGNED_BYTE, upload_data);

        //connects mat index to layer
        gpu_materials[i].tex_index = i;

        if(to_free) stbi_image_free(to_free);
        if(resized) free(resized);

        printf("[TEXTURES] Loaded %d: %s (%dx%d)\n", i, cpu_mat.tex_path.c_str(), w, h);
    }

    glGenerateTextureMipmap(renderer.tex_array);

    //Texture bind
    glBindTextureUnit(3, renderer.tex_array);
    glUseProgram(renderer.active_id);
    glUniform1i(renderer.loc_tex_array, 3);

    printf("[TEXTURES] Uploaded %d textures to array (%dx%d)\n", tex_count, TEX_SIZE, TEX_SIZE);
    return true;
}

