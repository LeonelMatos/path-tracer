#include "hdri.hpp"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace std;

bool loadEnvMap(const string& path, Renderer& renderer) {
    stbi_set_flip_vertically_on_load(false);
    int width, height;
    float* data = stbi_loadf(path.c_str(), &width, &height, nullptr, 3);
    if(!data) {
        printf("\nENV MAP: Failed to load env map: %s\n", path.c_str());
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

    printf("ENVMAP: Loaded env map %s (%dx%d)\n", path.c_str(), width, height);
    return true;
}