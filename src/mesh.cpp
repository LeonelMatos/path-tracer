#include "mesh.hpp"
#include <stdio.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "stb_image.h"

using namespace std;
using namespace glm;

#define MAT_DIFFUSE 0
#define MAT_MIRROR 1
#define MAT_GLASS 2
#define MAT_TINTED_GLASS 3

int uploadLights(const vector<GPUTriangle>& triangles, const vector<GPUMaterial>& materials, GLuint& out_light_ssbo) {
    vector<int> light_indices;

    for(int i = 0; i < (int)triangles.size(); i++) {
        int m_id = triangles[i].material_id;
        vec3 emission = vec3(materials[m_id].emission);

        if(dot(emission, emission) > 0.0f)
            light_indices.push_back(i);
    }
    printf("\tLights: %zu emissive triangles\n", light_indices.size());

    //real light count, before checking if empty and avoiding empty buffer
    int real_count = (int)light_indices.size();

    if(light_indices.empty())
        light_indices.push_back(0);

    glCreateBuffers(1, &out_light_ssbo);
    glNamedBufferData(out_light_ssbo, light_indices.size() * sizeof(int), light_indices.data(), GL_STATIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 9, out_light_ssbo);

    return real_count;
}

int uploadAnalyticLights(const vector<GPULight>& lights, GLuint& out_ssbo) {
    glCreateBuffers(1, &out_ssbo);
    glNamedBufferData(out_ssbo, lights.size() * sizeof(GPULight), lights.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 10, out_ssbo);
    return (int)lights.size();
}

///Aux function to convert aiMatrix4x4 to glm::mat4 (for .glb hierarchical node transforms)
static mat4 aiToGlm(const aiMatrix4x4 m) {
    return mat4(
        m.a1, m.b1, m.c1, m.d1,
        m.a2, m.b2, m.c2, m.d2,
        m.a3, m.b3, m.c3, m.d3,
        m.a4, m.b4, m.c4, m.d4
    );
}

///Recursively accumulates the transform of each node and associates it to the refering meshes
static void collectMeshTransforms(const aiNode* node, aiMatrix4x4 parent_transform, vector<aiMatrix4x4>& mesh_transforms) {
    aiMatrix4x4 global = parent_transform * node->mTransformation;

    for(unsigned int i = 0; i < node->mNumMeshes; i++) {
        mesh_transforms[node->mMeshes[i]] = global;
    }
    for(unsigned int i = 0; i < node->mNumChildren; i++) {
        collectMeshTransforms(node->mChildren[i], global, mesh_transforms);
    }
}

bool loadMesh(const string& path, vector<GPUTriangle>& triangles, vector<GPUMaterial>& materials, vector<CPUMaterial>& cpu_materials, 
    mat4 transform, MeshBounds* bounds) {
    //Initialize bounds
    if (bounds) {
        bounds->min_bound = vec3(FLT_MAX);
        bounds->max_bound = vec3(-FLT_MAX);
    }

    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path,
         aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_FlipUVs | aiProcess_JoinIdenticalVertices | aiProcess_FixInfacingNormals);

    if(!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        fprintf(stderr, "ASSIMP error loading path '%s': %s\n", path.c_str(), importer.GetErrorString());
        return false;
    }
    printf("\n[MODEL] Loading %s : %d meshes, %d materials\n", path.c_str(), scene->mNumMeshes, scene->mNumMaterials);

    mat3 normal_mat = transpose(inverse(mat3(transform)));

    //Load materials
    materials.clear();
    for (unsigned int m = 0; m < scene->mNumMaterials; m++) {
        aiMaterial* mat = scene->mMaterials[m];
        GPUMaterial gpu_mat{};
        CPUMaterial cpu_mat{};

        aiColor3D color(0.8f, 0.8f, 0.8f);
        mat->Get(AI_MATKEY_COLOR_DIFFUSE, color);
        gpu_mat.albedo = vec4(color.r, color.g, color.b, 1.0f);
        gpu_mat.emission = vec4(0.0f);
        gpu_mat.type = MAT_DIFFUSE;
        gpu_mat.ior = 1.5f;
        gpu_mat.tex_index = -1;

        //Material type + properties

        //Light emission
        aiColor3D emission(0.0f, 0.0f, 0.0f);
        mat->Get(AI_MATKEY_COLOR_EMISSIVE, emission);
        if(emission.r > 0.01f || emission.g > 0.01f || emission.b > 0.01f) {
            float emissive_strength = 1.0f;
            mat->Get(AI_MATKEY_EMISSIVE_INTENSITY, emissive_strength);
            gpu_mat.emission = vec4(emission.r, emission.g, emission.b, 0.0f) * emissive_strength;
        }

        //Transmission (Glass)
        float transmission = 0.0f;
        mat->Get(AI_MATKEY_TRANSMISSION_FACTOR, transmission);
        float opacity = 1.0f;
        mat->Get(AI_MATKEY_OPACITY, opacity);
        
        if(transmission > 0.5f) {
            gpu_mat.type = MAT_GLASS;
            float ior = 1.5f;
            mat->Get(AI_MATKEY_REFRACTI, ior);
            gpu_mat.ior = ior;
        }
        else if (opacity < 0.5f) {
            gpu_mat.type = MAT_TINTED_GLASS; //(not tested, I'll ignore it)
            ///\bug don't know if this will work well with spectral rendering
        }
        else {
            //Metallic <-> Mirror
            float metallic = 0.0f, roughness = 1.0f;
            mat->Get(AI_MATKEY_METALLIC_FACTOR, metallic);
            mat->Get(AI_MATKEY_ROUGHNESS_FACTOR, roughness);
            if(metallic > 0.8f && roughness < 0.1f)
                gpu_mat.type = MAT_MIRROR;
            ///\bug this is a very incorrect way to force the mirror type, but I don't have metallic/roughness
            ///value types, so it is what it is...
        }
        printf("\tMaterial %d   ( type=%d ior=%.2f emission=(%.2f,%.2f,%.2f) )\n", m, gpu_mat.type, gpu_mat.ior, gpu_mat.emission.r, gpu_mat.emission.g, gpu_mat.emission.b);

        //Get material texture
        aiString tex_path;
        aiTextureType tex_type = aiTextureType_NONE;

        if(mat->GetTextureCount(aiTextureType_BASE_COLOR) > 0)
            tex_type = aiTextureType_BASE_COLOR;
        else if (mat->GetTextureCount(aiTextureType_DIFFUSE) > 0)
            tex_type = aiTextureType_DIFFUSE;

        if(tex_type != aiTextureType_NONE && mat->GetTexture(tex_type, 0, &tex_path) == AI_SUCCESS) {
            string full_path;

            //embedded texture glfw base 64
            if(tex_path.data[0] == '*') {
                int idx = atoi(tex_path.C_Str() + 1);
                const aiTexture* tex = scene->mTextures[idx];

                if (tex->mHeight == 0) {
                    //Compressed png or jpg (jpg? jpeg?)
                    int width, height, channels;
                    unsigned char* decoded = stbi_load_from_memory((unsigned char*)tex->pcData, tex->mWidth,
                                        &width, &height, &channels, 4);
                    if (decoded) {
                        cpu_mat.embedded_data = vector<unsigned char>(decoded, decoded + width * height * 4);
                        cpu_mat.embedded_width = width;
                        cpu_mat.embedded_height = height;
                        cpu_mat.has_texture = 1;
                        stbi_image_free(decoded);
                    }
                    else {
                        printf("[MODEL] loadMesh() failed to decode embedded texture %d\n", idx);
                    }
                }
                else {
                    //Raw RGBA
                    int size = tex->mWidth * tex->mHeight * 4;
                    cpu_mat.embedded_data = vector<unsigned char>((unsigned char* )tex->pcData, (unsigned char*)tex->pcData + size);
                    cpu_mat.embedded_width = tex->mWidth;
                    cpu_mat.embedded_height = tex->mHeight;
                    cpu_mat.has_texture = 1;
                }
                
            }
            //external texture
            else {
                filesystem::path model_dir = filesystem::path(path).parent_path();
                cpu_mat.tex_path = (model_dir / tex_path.C_Str()).string();
                cpu_mat.has_texture = 1;
            }
        }

        materials.push_back(gpu_mat);
        cpu_materials.push_back(cpu_mat);
    }
    if(materials.empty()) {
        GPUMaterial default_mat{};
        default_mat.albedo = vec4(0.8f, 0.8f, 0.8f, 1.0f);
        default_mat.type = MAT_DIFFUSE;
        materials.push_back(default_mat);
    }

    //Load triangles
    vector<aiMatrix4x4> mesh_node_transform(scene->mNumMeshes, aiMatrix4x4());
    collectMeshTransforms(scene->mRootNode, aiMatrix4x4(), mesh_node_transform);

    triangles.clear();
    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        aiMesh* mesh = scene->mMeshes[m];

        mat4 node_transform = aiToGlm(mesh_node_transform[m]);
        mat4 final_transform = transform * node_transform;
        mat3 normal_mat = transpose(inverse(mat3(final_transform)));

        for(unsigned f = 0; f < mesh->mNumFaces; f++) {
            aiFace& face = mesh->mFaces[f];
            if(face.mNumIndices != 3) continue; //skips to just triangles

            GPUTriangle tri{};
            GPUVertex* verts[3] = {&tri.v0, &tri.v1, &tri.v2};

            for (int v = 0; v < 3; v++) {
                unsigned int idx = face.mIndices[v];

                verts[v]->position = vec3(final_transform * vec4(mesh->mVertices[idx].x, mesh->mVertices[idx].y, mesh->mVertices[idx].z, 1.0f));

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
    printf("\n\tTotal %zu triangles, %zu materials", triangles.size(), materials.size());

    if(bounds)
        printf("\n\tBounds: (%.2f,%.2f,%.2f),(%.2f,%.2f,%.2f)\n",
             bounds->min_bound.x, bounds->min_bound.y, bounds->min_bound.z,
             bounds->max_bound.x, bounds->max_bound.y, bounds->max_bound.z);

    return true;
}

bool uploadMesh(const vector<GPUTriangle>& triangles, const vector<GPUMaterial>& materials, GLuint& tri_ssbo, GLuint& mat_ssbo) {
    size_t tri_size = triangles.size() * sizeof(GPUTriangle);
    size_t mat_size = materials.size() * sizeof(GPUMaterial);

    printf("\t[UPLOAD] tri_ssbo handle before create: %u\n", tri_ssbo);

    printf("\t[UPLOAD] Triangles: %.1f MB | Materials: %.1f MB\n", tri_size / 1e6f, mat_size / 1e6f);

    //Triangles DSA
    glCreateBuffers(1, &tri_ssbo);
    glNamedBufferData(tri_ssbo, triangles.size() * sizeof(GPUTriangle), triangles.data(), GL_STATIC_DRAW);
    GLenum error = glGetError();
    if(error != GL_NO_ERROR) {
        fprintf(stderr, "\t[UPLOAD MESH] Error: Triangle SSBO failed (GL error 0x%x)\n", error);
        return false;
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, tri_ssbo);

    //Materialss
    glCreateBuffers(1, &mat_ssbo);
    glNamedBufferData(mat_ssbo, materials.size() * sizeof(GPUMaterial), materials.data(), GL_STATIC_DRAW);
    error = glGetError();
    if(error != GL_NO_ERROR) {
        fprintf(stderr, "\t[UPLOAD MESH] Error: Material SSBO failed (GL error 0x%x)\n", error);
        return false;
    }
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, mat_ssbo);

    return true;
}

void clearMesh() {
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer.triangle_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer.material_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer.bvh_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, renderer.light_ssbo);
    glBufferData(GL_SHADER_STORAGE_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

}

vector<GPUTriangle> makeTestMesh() {
    GPUMaterial mat;
    mat.albedo = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    mat.emission = vec4(0.0f);
    mat.type = MAT_DIFFUSE;
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