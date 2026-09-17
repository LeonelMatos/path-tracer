#include "texture.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include "stb_image_resize2.h"
#include <algorithm>
#include <functional>

using namespace std;

bool loadEnvMap(const string& path, Renderer& r) {
    stbi_set_flip_vertically_on_load(false);
    int width, height;
    float* data = stbi_loadf(path.c_str(), &width, &height, nullptr, 3);
    if(!data) {
        printf("\n[ENVMAP] Failed to load env map: %s\n", path.c_str());
        return false;
    }

    if(r.env_map_tex) glDeleteTextures(1, &r.env_map_tex);

    glCreateTextures(GL_TEXTURE_2D, 1, &r.env_map_tex);
    glTextureStorage2D(r.env_map_tex, 1, GL_RGBA16F, width, height);
    glTextureSubImage2D(r.env_map_tex, 0, 0, 0, width, height, GL_RGB, GL_FLOAT, data);
    glTextureParameteri(r.env_map_tex, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTextureParameteri(r.env_map_tex, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(r.env_map_tex, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(r.env_map_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);

    glUseProgram(r.active_id);
    glBindTextureUnit(2, r.env_map_tex);
    glUniform1i(r.loc_env_map, 2);
    glUniform1i(r.loc_use_env_map, 1);
    r.use_env_map = true;

    printf("\n[ENVMAP] %s  (%dx%d)\n", path.c_str(), width, height);
    return true;
}

/**
 *\brief Reads width/height of a texture without decoding it (fast path for on-disk files)
 *Embedded textures are already decoded in CPUTextureSlot, so it's readed directly
 *\param out_w Width output
 *\param out_h Height output
 *\return true File dimensions read successfully
 */
static bool getTextureDimensions(const CPUTextureSlot& slot, int& out_w, int& out_h) {
    if(!slot.embedded_data.empty()) {
        out_w = slot.embedded_width;
        out_h = slot.embedded_height;
        return true;
    }
    int channels;
    if(!stbi_info(slot.tex_path.c_str(), &out_w, &out_h, &channels)) {
        printf("\n[TEXTURES] Failed to read dimensions of %s\n", slot.tex_path.c_str());
        return false;
    }
    return true;
}

/**
 *\brief Packs one texture slot (albedo or normal) across all materials into a single
 GL_TEXTURE_2D_ARRAY, writing the resolved layer index back to the corresponding GPUMaterial field.
 *\note Previous uploadTexture()
 \param get_slot Selects which CPUTextureSlot to read (albedo or normal)
 \param write_index Writes the resolved layer (or -1) into the right GPUMaterial field
 \param internal_format GL_SRGB8_ALPHA8 for color data (albedo); GL_RGBA8 for vector-encoded data (normals)
 \param label Short name used only in log output (ex. "albedo", "normal")
 */
bool packTextureArray(const vector<CPUMaterial>& cpu_materials, vector<GPUMaterial>& gpu_materials,
 GLuint& out_array, GLenum internal_format, const char* label,
 std::function<const CPUTextureSlot&(const CPUMaterial&)> get_slot,
 std::function<void(GPUMaterial&, int)> write_index) {

    bool is_srgb = (internal_format == GL_SRGB8_ALPHA8);

    //Only allocate array layers for materials that actually have a texture
    vector<int> mat_to_layer(cpu_materials.size(), -1);
    int tex_count = 0;
    for (int i = 0; i < (int)cpu_materials.size(); i++) {
        if(get_slot(cpu_materials[i]).has_texture)
            mat_to_layer[i] = tex_count++;
    }

    for(int i = 0; i < (int)gpu_materials.size(); i++)
        write_index(gpu_materials[i], -1);

    if (tex_count == 0) {
        printf("\n[TEXTURES] Model has no textures to upload\n");
        if(out_array) { glDeleteTextures(1, &out_array); out_array = 0; }
        return false;
    }

    //Size the array to the largest texture present
    int max_dim = MIN_TEX_SIZE;
    for(auto& m : cpu_materials) {
        const CPUTextureSlot& slot = get_slot(m);
        if(!slot.has_texture) continue;
        int w, h;
        if(!getTextureDimensions(slot, w, h)) continue;
        max_dim = std::max({max_dim, w, h});
    }
    max_dim = std::min(max_dim, MAX_TEX_SIZE);

    int tex_size = MIN_TEX_SIZE;
    while(tex_size < max_dim) tex_size <<= 1;

    const int MAX_LAYERS = tex_count;
    int mip_levels = 1 + (int)floor(std::log2(tex_size));

    if(out_array)
        glDeleteTextures(1, &out_array);

    size_t est_bytes_per_layer = (size_t)tex_size * tex_size * 4;
    size_t est_total = (size_t)(est_bytes_per_layer * (4.0/3.0)) * MAX_LAYERS;
    printf("[TEXTURES] Estimated %s array VRAM: %.1f MB (%dx%d, %d layers)\n", label, est_total / 1e6, tex_size,
        tex_size, MAX_LAYERS);

    glCreateTextures(GL_TEXTURE_2D_ARRAY, 1, &out_array);
    glTextureStorage3D(out_array, mip_levels, internal_format, tex_size, tex_size, MAX_LAYERS);
    glTextureParameteri(out_array, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTextureParameteri(out_array, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTextureParameteri(out_array, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTextureParameteri(out_array, GL_TEXTURE_WRAP_T, GL_REPEAT);

    //Anisotropic filtering
    GLfloat max_aniso = 0.0f;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &max_aniso);
    glTextureParameterf(out_array, GL_TEXTURE_MAX_ANISOTROPY, max_aniso);

    stbi_set_flip_vertically_on_load(false);

    for (int i = 0; i < (int)cpu_materials.size(); i++) {
        const CPUTextureSlot& slot = get_slot(cpu_materials[i]);
        int layer = mat_to_layer[i];
        if (layer < 0) continue;

        int w, h;
        unsigned char* data = nullptr;
        unsigned char* to_free = nullptr;

        //data already decoded at CPUMaterial
        if(!slot.embedded_data.empty()) {
            data = const_cast<unsigned char*>(slot.embedded_data.data());
            w = slot.embedded_width;
            h = slot.embedded_height;
        }
        //external texture, load from disk
        else {
            int channels;
            data = stbi_load(slot.tex_path.c_str(), &w, &h, &channels, 4);
            to_free = data;
            if(!data) {
                printf("\n[TEXTURES] Failed to load %s: %s\n", label, slot.tex_path.c_str());
                continue;
            }
        }

        //Texture resize to TEX_SIZE for uniformity
        unsigned char* upload_data = data;
        unsigned char* resized = nullptr;

        if (w != tex_size || h != tex_size) {
            resized = (unsigned char*)malloc((size_t)tex_size * tex_size * 4);
            if(is_srgb)
                stbir_resize_uint8_srgb(data, w, h, 0, resized, tex_size, tex_size, 0, STBIR_RGBA);
            else
                stbir_resize_uint8_linear(data, w, h, 0, resized, tex_size, tex_size, 0, STBIR_RGBA);
            
            upload_data = resized;
        }

        //Load to array to layer i
        glTextureSubImage3D(out_array, 0, 0, 0, layer, tex_size, tex_size, 1, GL_RGBA, GL_UNSIGNED_BYTE, upload_data);
        write_index(gpu_materials[i], layer);

        if(to_free) stbi_image_free(to_free);
        if(resized) free(resized);

        printf("[TEXTURES] Loaded %s %d → layer %d (%dx%d)\n", label, i, layer, w, h);
    }

    glGenerateTextureMipmap(out_array);

    return true;
}

bool uploadTexture(const vector<CPUMaterial>& cpu_materials, vector<GPUMaterial>& gpu_materials, Renderer& r) {
    bool albedo_ok = packTextureArray(
        cpu_materials, gpu_materials, r.tex_array, GL_SRGB8_ALPHA8, "albedo",
        [](const CPUMaterial& m) -> const CPUTextureSlot& { return m.albedo_tex; },
        [](GPUMaterial& g, int layer) { g.tex_index = layer; }
    );

    bool normal_ok = packTextureArray(
        cpu_materials, gpu_materials, r.tex_normal_array, GL_RGBA8, "normal",
        [](const CPUMaterial& m) -> const CPUTextureSlot& { return m.albedo_tex; },
        [](GPUMaterial& g, int layer) { g.tex_index = layer; }
    );

    if(albedo_ok) {
        glBindTextureUnit(3, r.tex_array);
        glUseProgram(r.active_id);
        glUniform1i(r.loc_tex_array, 3);
    }
    if(normal_ok) {
        glBindTextureUnit(4, r.tex_normal_array);
        glUseProgram(r.active_id);
        glUniform1i(r.loc_tex_normal, 4);
    }

    return albedo_ok;
}