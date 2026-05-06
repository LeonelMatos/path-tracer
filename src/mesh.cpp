#include "mesh.hpp"
#include <stdio.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

using namespace std;
using namespace glm;

bool loadMesh(const string& path, vector<GPUTriangle>& triangles, vector<GPUMaterial>& materials, mat4 transform, MeshBounds* bounds) {\
    //Initialize bounds
    if (bounds) {
        bounds->min_bound = vec3(FLT_MAX);
        bounds->max_bound = vec3(-FLT_MAX);
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
         aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices);

    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        fprintf(stderr, "ASSIMP error loading path '%s': %s\n", path.c_str(), importer.GetErrorString());
        return false;
    }
    printf("Loading %s : %d meshes, %d materials\n", path.c_str(), scene->mNumMeshes, scene->mNumMaterials);

    mat3 normal_mat = transpose(inverse(mat3(transform)));

    //Load materials
    materials.clear();
    for (unsigned int m = 0; m < scene->mNumMaterials; m++) {
        aiMaterial* mat = scene->mMaterials[m];
        GPUMaterial gpu_mat{};

        aiColor3D color(0.8f, 0.8f, 0.8f);
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        gpu_mat.albedo = vec4(color.r, color.g, color.b, 1.0f);
        gpu_mat.emission = vec4(0.0f);
        gpu_mat.type = 0;
        gpu_mat.ior = 1.5f;

        materials.push_back(gpu_mat);
    }
    if(materials.empty()) {
        GPUMaterial default_mat{};
        default_mat.albedo = vec4(0.8f, 0.8f, 0.8f, 1.0f);
        default_mat.type = 0;
        materials.push_back(default_mat);
    }

    //Load triangles
    triangles.clear();
    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        aiMesh* mesh = scene->mMeshes[m];

        for(unsigned f = 0; f < mesh->mNumFaces; f++) {
            aiFace& face = mesh->mFaces[f];
            if(face.mNumIndices != 3) continue; //skips to just triangles

            GPUTriangle tri{};
            GPUVertex* verts[3] = {&tri.v0, &tri.v1, &tri.v2};

            for (int v = 0; v < 3; v++) {
                unsigned int idx = face.mIndices[v];

                verts[v]->position = vec3(transform * vec4(mesh->mVertices[idx].x, mesh->mVertices[idx].y, mesh->mVertices[idx].z, 1.0f));

                //normal validation
                if(mesh->HasNormals()) {
                    vec3 n = normal_mat * vec3(mesh->mNormals[idx].x, mesh->mNormals[idx].y, mesh->mNormals[idx].z);

                    float len = length(n);
                    verts[v]->normal = (len > 1e-6f && !isnan(len)) ? n / len : vec3(0, 0, 1);
                }
                
                if(mesh->HasTextureCoords(0))
                    verts[v]->texcoord = vec2(mesh->mTextureCoords[0][idx].x, mesh->mTextureCoords[0][idx].y);

                if(bounds) {
                    bounds->min_bound = min(bounds->min_bound, verts[v]->position);
                    bounds->max_bound = max(bounds->max_bound, verts[v]->position);
                }
            }
            vec3 edge1 = tri.v1.position - tri.v0.position;
            vec3 edge2 = tri.v2.position - tri.v0.position;
            float area = length(cross(edge1, edge2));
            if (area < 1e-10f) continue;

            tri.material_id = mesh->mMaterialIndex;
            triangles.push_back(tri);
        }
    }
    printf("\tTotal %zu triangles, %zu materials", triangles.size(), materials.size());

    if(bounds)
        printf("\n\tBounds: (%.2f,%.2f,%.2f) to (%.2f,%.2f,%.2f)\n",
             bounds->min_bound.x, bounds->min_bound.y, bounds->min_bound.z,
             bounds->max_bound.x, bounds->max_bound.y, bounds->max_bound.z);

    return true;
}

bool uploadMesh(const vector<GPUTriangle>& triangles, const vector<GPUMaterial>& materials, GLuint& tri_ssbo, GLuint& mat_ssbo) {
    //Triangles DSA
    glCreateBuffers(1, &tri_ssbo);
    glNamedBufferData(tri_ssbo, triangles.size() * sizeof(GPUTriangle), triangles.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, tri_ssbo);

    //Materialss
    glCreateBuffers(1, &mat_ssbo);
    glNamedBufferData(mat_ssbo, materials.size() * sizeof(GPUMaterial), materials.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mat_ssbo);

    return true;
}

vector<GPUTriangle> makeTestMesh() {
    GPUMaterial mat;
    mat.albedo = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.emission = vec4(0.0f);
    mat.type = 0;
    mat.ior = 0.0f;

    vec3 A = vec3(-0.4, 0.0, 0.1);
    vec3 B = vec3(-0.9, 0.5, -0.9);
    vec3 C = vec3(0.2, 0.5, -0.9);
    vec3 D = vec3(-0.4, -0.5, -0.9);

    auto makeTriangle = [](vec3 p0, vec3 p1, vec3 p2, int mat_id) {
        GPUTriangle t;
        t.v0.position = p0; t.v0.normal = vec3(0);
        t.v1.position = p1; t.v1.normal = vec3(0);
        t.v2.position = p2; t.v2.normal = vec3(0);
        t.material_id = mat_id;
        return t;
    };

    return {
        makeTriangle(A, B, C, 0), makeTriangle(A, C, D, 0), makeTriangle(A, D, B, 0), makeTriangle(B, D, C, 0)
    };
}