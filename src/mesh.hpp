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

struct GPUVertex {
    glm::vec3 position;
    float _pad0;
    glm::vec3 normal;
    float _pad1;
    glm::vec2 texcoord;
    glm::vec2 _pad2;
};

struct GPUTriangle {
    GPUVertex v0, v1, v2;
    int material_id;
    float _pad[3];
};

///\note vec4, alpha value used only as padding for std430
struct GPUMaterial {
    glm::vec4 albedo;
    glm::vec4 emission;
    int type; //material
    float ior;
    //-1 = no texture
    int tex_index;
    float _pad;
};

struct CPUMaterial {
    int has_texture = 0;
    string tex_path = "";
    int embedded_index = -1;
    //Embedded texture data
    std::vector<unsigned char> embedded_data;
    int embedded_width = 0;
    int embedded_height = 0;
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