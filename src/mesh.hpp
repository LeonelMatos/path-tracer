#pragma once

#include <vector>
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

using namespace std;
using namespace glm;

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
    float _pad[2];
};

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

/**\brief Loads a mesh and calculates its bounding box
\param path Path to the mesh file (any file format supported by ASSIMP)
\param triangles Output triangle buffer
\param materials Output material buffer
\param transform Transform matrix applied to all vertices (default: Identity)
\param bounds Optional output bounding box - nullptr to skip
\return true if loaded correctly
\see MeshBounds, uploadMesh
 */
bool loadMesh(const string& path, vector<GPUTriangle>& triangles, vector<GPUMaterial>& materials, mat4 transform = mat4(1.0f), MeshBounds* bounds = nullptr);

bool uploadMesh(const vector<GPUTriangle>& triangles, const vector<GPUMaterial>& materials, GLuint& tri_ssbo, GLuint& mat_ssbo);

vector<GPUTriangle> makeTestMesh();