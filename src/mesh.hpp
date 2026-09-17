/**
 * \file mesh.hpp
 * \author Leonel Matos
 * \brief Loads 3D models through ASSIMP into CPU-side geometry and material data,
 * defines the GPUVertex/GPUTriangle/GPUMaterial layouts shared with the shader
 * \see globals.glsl for the mirrored std430 structs 
 * @date 2026-09-10
 * @copyright Copyright (c) 2026
 */

#pragma once

#include <vector>
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

#include "config.hpp"

using namespace std;
using namespace glm;

extern Renderer renderer;

/*----------------------------------------------------------
  Triangles/Material structs
*/

///\warning Adding tangent here increases GPUTriangle from 160 to 208 bytes.
///This will increase VRAM usage by ~30%.
///\todo Might as well use the pre-calculated size to warn if the GPU can't handle it.
struct GPUVertex {
    glm::vec3 position;
    float _pad0;
    glm::vec3 normal;
    float _pad1;
    glm::vec2 texcoord;
    glm::vec2 _pad2;
    ///Stored vertex tangents, xyz = tangent direction,
    ///w = handedness sign for bitangent (= cross(normal, tan.xyz) * tan.w)
    ///Needed to shade correctly across mirrored UV islands
    glm::vec4 tangent;
};

static_assert(sizeof(GPUVertex) == 64, "GPUVertex must match the std430 layout in globals.glsl");

struct GPUTriangle {
    GPUVertex v0, v1, v2;
    int material_id;
    float _pad[3];
};

static_assert(sizeof(GPUTriangle) == 208, "GPUTriangle must match the std430 layout in globals.glsl");

///\note vec4, alpha value used only as padding for std430
struct GPUMaterial {
    glm::vec4 albedo;
    glm::vec4 emission;
    ///\todo Add uv_scale implementation to reduce texture unused size (here and globals), remove _pad
    //glm::vec2 uv_scale; ///< fraction of the array layer holding the texture data
    int type; ///< Material type
    float ior;
    int tex_index; ///< -1 = no albedo texture
    int normal_tex_index; ///< -1 = no normal map
    float roughness; ///< 0 = smooth like mirror, 1 = full rough
    float metallic; ///< 0 = dielectric, 1 = metal
    glm::vec2 _pad;
};

static_assert(sizeof(GPUMaterial) == 64, "GPUMaterial must match the std430 layout in globals.glsl");

///Describes one texture slot (any albedo, normal, rough...) as loaded from source file.
///Can be either a path from disk to load, or already-decoded embedded data.
///\note Extended and generalized from the old single-tex CPUMaterial.
struct CPUTextureSlot {
    int has_texture = 0;
    string tex_path = "";
    std::vector<unsigned char> embedded_data;
    int embedded_width = 0;
    int embedded_height = 0;
};

struct CPUMaterial {
    CPUTextureSlot albedo_tex;
    CPUTextureSlot normal_tex;
};

///\brief Mesh light storage to pass emissive triangles
extern GLuint light_ssbo;
extern GLint loc_light_count;

/**\brief Loaded mesh bounding box, axis-aligned, used for AABB early rejection
Calculated during loadMesh. Used for AABB early rejection for rays that miss the bouding box
\see loadMesh, intersects
*/
struct MeshBounds {
    ///Minimum corner in world position
    vec3 min_bound;
    ///Maximum corner in world position
    vec3 max_bound;
};

/*----------------------------------------------------------
  Light Structure, Types Definitions
*/
/**
\note vec4 used for padding on the ssbo (w-value not used)
\todo Replace type hardcoded number to light definitions ex. LIGHT_POINT
 */
struct GPULight {
    vec4 position;
    vec4 emission;
    vec4 direction;
    int type;
    float radius;
    float spot_inner;
    float spot_outer;
};

static_assert(sizeof(GPULight) == 64, "GPULight must match the std430 layout in globals.glsl");

#define LIGHT_POINT 0
#define LIGHT_DIRECTIONAL 1
#define LIGHT_SPOT  2

/*----------------------------------------------------------
 Header Functions
*/

///\brief Recreates/Creates a GPU buffer, freeing any previous held handle first.
///Prevents leaking the old buffer when called repeatedly
///\param ssbo Buffer handle; deleted if exists, then recreates
///\param size_bytes Byte size of the data to upload
///\param data Pointer to source data (can be nullptr to just allocate space)
///\param usage GL usage hint (STATIC_DRAW, DYNAMIC_DRAW...)
inline void recreateBuffer(GLuint& ssbo, GLsizeiptr size_bytes, const void* data, GLenum usage) {
    if(ssbo) glDeleteBuffers(1, &ssbo);
    glCreateBuffers(1, &ssbo);
    glNamedBufferData(ssbo, size_bytes, data, usage);
}

int uploadLights(const vector<GPUTriangle>& triangles, const vector<GPUMaterial>& materials, GLuint& light_ssbo);

int uploadAnalyticLights(const vector<GPULight>& lights, GLuint& light_ssbo);

/**\brief Loads a mesh and calculates its bounding box
\param path Path to the mesh file (any file format supported by ASSIMP)
\param triangles Output triangle buffer
\param materials Output material buffer
\param transform Transform matrix applied to all vertices (default: Identity)
\param bounds Optional output bounding box - nullptr to skip
\return true if loaded correctly
\see MeshBounds, uploadMesh
 */
bool loadMesh(const string& path, vector<GPUTriangle>& triangles, vector<GPUMaterial>& materials, vector<CPUMaterial>& cpu_materials, mat4 transform, MeshBounds* bounds);

bool uploadMesh(const vector<GPUTriangle>& triangles, const vector<GPUMaterial>& materials, GLuint& tri_ssbo, GLuint& mat_ssbo);

void clearMesh();

vector<GPUTriangle> makeTestMesh();


/*----------------------------------------------------------
  Debux Aux
*/

///\brief quick checkpoint to check OpenGL silent error throws.
///Catches the error and sends it, or it prints an OK check
///\param label 
inline void checkGL(const char* label) {
    GLenum e;
    bool found = false;
    while ((e = glGetError()) != GL_NO_ERROR) {
        printf("[GL ERROR] %s : 0x%x\n", label, e);
        found = true;
    }
    if(!found)
        printf("[GL OK] %s\n", label);
}