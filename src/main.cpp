/**
 * \file main.cpp
 * \author Leonel Matos
 * \date 2026-03-24
 * \brief Main code
 * \copyright Copyright (c) 2026
 */
/*
    Path Tracer
    Leonel Matos 48284

    Recriar a Cornell box com emissive sphere, base no exemplo de 
    Image Synthesis (Summer Term 2026) Prof. Dr. Thorsten Thormählen
    Capítulo 3.2 Path Tracing, Brute-Force Evaluation of the Rendering Equation (8/32)
    (https://www.uni-marburg.de/en/fb12/research-groups/grafikmultimedia/lectures/graphics2)

    Dependências
    shaders/
        common
        display
        path_trace - compute e fragment
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <vector>
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "config.hpp"
#include "common/shader.hpp"
#include "mesh.hpp"
#include "bvh.hpp"

#define VERSION "1.1.0"

using namespace std;
using namespace glm;

/*----------------------------------------------------------
  Global Variables
*/

struct Renderer {
    GLuint display_id, pathtr_frag_id, pathtr_comp_id;
    GLuint active_id;
    GLuint tex[2], fbo[2], vao;
    GLint loc_res, loc_frame, loc_prev, loc_tex;
    int frame_id = 0, cur_f = 0, prev_f = 1;

    GLint loc_depth, loc_spp, loc_rr_min, loc_rr_max, loc_aperture;
    GLint loc_focal_dist, loc_focal_debug, loc_focal_band, loc_background, loc_tone_map;
};

struct Metrics {
    //Time metrics
    uint total_frames = 0;
    double start_time = 0.0;
    double fps, samples_per_s;
};

GLFWwindow* window;

Renderer renderer;
Metrics metrics;

RenderConfig config;

static const int WINDOW_WIDTH = 1000, WINDOW_HEIGHT = 1000;

#define WINDOW_TITLE "Path Tracer"

/**Switches between using fragment or compute shaders
for the path tracer
\note false = fragment; true = compute*/
const bool USE_COMPUTE_SH = false;

static const int COMPUTE_LOCAL_X = 16;
static const int COMPUTE_LOCAL_Y = 16;

const int V_SYNC = 0;
const uint MAX_SAMPLES = 1000;


const char* txt_sep = "----------------------------";

/*----------------------------------------------------------
GPU Mesh Render
*/
GLuint triangle_ssbo;
GLuint material_ssbo;
///Triangle count fixed value passed pre-calculated
GLint loc_tri_count;
GLint loc_aabb_min, loc_aabb_max;


//BVH
GLuint bvh_ssbo;
GLint loc_bvh_root;

/*----------------------------------------------------------
  Function Declarations
*/
void onKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
void formatTime(double seconds, char*buf, int buf_size);
bool initShaders();
void loadScene();
void uploadConfig();
void applyConfig();
bool transferDataToGPU(void);
void cleanDataFromGPU();
void display(void);
void resetAccumulation();
void draw(void);
void saveScreenshot();

/*----------------------------------------------------------
  Input handle
*/
void onKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_F12:
            saveScreenshot();
        break;
        case GLFW_KEY_R:
            resetAccumulation();
        break;
        case GLFW_KEY_E:
            config.background = 1 - config.background;
            applyConfig();
        break;
    }
}

//----------------------------------------------------------
int main(void) {
    if (!glfwInit()) { fprintf(stderr, "Failed to init GLFW\n"); return -1; }
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(V_SYNC);
    
    glewExperimental = GL_TRUE;
    glewInit();
    
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    glfwSetKeyCallback(window, onKeyPress);

    if(!transferDataToGPU())
    return -1;
    
    printf("%s\n%s v%s\nResolution: %dx%d\n", txt_sep, WINDOW_TITLE, VERSION, WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("Shader: %s\nPress \tESC to quit\n \tF12 to screenshot render\n%s\n", USE_COMPUTE_SH ? "Compute" : "Fragment", txt_sep);

    //Time init
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics.start_time = ts.tv_sec + ts.tv_nsec * 1e-9;

    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
        //Suspend the rendering after completion to avoid useless GPU processing
        if (MAX_SAMPLES > 0 && renderer.frame_id >= MAX_SAMPLES) {
            glfwWaitEvents(); //Gets input events and avoids program freezing
            continue;
        }
        draw();
    }
    cleanDataFromGPU();
    glfwTerminate();

    return 0;
}

void formatTime(double seconds, char*buf, int buf_size) {
    if (seconds < 60.0)
        snprintf(buf, buf_size, "%.1fs", seconds);
    else {
        int min = (int)(seconds/60.0);
        float sec = seconds - (min * 60.0);
        snprintf(buf, buf_size, "%dm%.1fs", min, sec);
    }
}

//----------------------------------------------------------

bool initShaders() {
    renderer.display_id = LoadShaders({
        { GL_VERTEX_SHADER,   "shaders/common.vert"    },
        { GL_FRAGMENT_SHADER, "shaders/display.frag" },
    });
    renderer.pathtr_frag_id = LoadShaders({
        { GL_VERTEX_SHADER,   "shaders/common.vert"       },
        { GL_FRAGMENT_SHADER, "shaders/path_trace.frag" },
    });
    renderer.pathtr_comp_id = LoadShaders({
        { GL_COMPUTE_SHADER, "shaders/path_trace.comp" }
    });
    if(!renderer.display_id || !renderer.pathtr_frag_id || !renderer.pathtr_comp_id)  { glfwTerminate(); return false; }
    return true;
}

void initUniforms() {
    GLuint active = renderer.active_id;

    renderer.loc_res = glGetUniformLocation(active, "resolution");
    renderer.loc_frame = glGetUniformLocation(active, "frame_id");
    renderer.loc_tex = glGetUniformLocation(active, "tex");
    loc_tri_count = glGetUniformLocation(active, "triangle_count");
    loc_aabb_min = glGetUniformLocation(active, "mesh_aabb_min");
    loc_aabb_max = glGetUniformLocation(active, "mesh_aabb_max");
    loc_bvh_root = glGetUniformLocation(active, "bvh_root");

    if(!USE_COMPUTE_SH) {
        renderer.loc_prev = glGetUniformLocation(renderer.pathtr_frag_id, "prev_frame");
    }
    renderer.loc_depth      = glGetUniformLocation(active, "DEPTH");
    renderer.loc_spp        = glGetUniformLocation(active, "SAMPLES_PER_PIXEL");
    renderer.loc_rr_min     = glGetUniformLocation(active, "RR_MIN_BOUNCES");
    renderer.loc_rr_max     = glGetUniformLocation(active, "RR_MAX_SURVIVAL");
    renderer.loc_aperture   = glGetUniformLocation(active, "CAM_APERTURE");
    renderer.loc_focal_dist = glGetUniformLocation(active, "CAM_FOCAL_DISTANCE");
    renderer.loc_focal_debug = glGetUniformLocation(active, "FOCAL_DEBUG");
    renderer.loc_focal_band  = glGetUniformLocation(active, "FOCAL_BAND_DEBUG");
    renderer.loc_background  = glGetUniformLocation(active, "BACKGROUND");
    renderer.loc_tone_map  = glGetUniformLocation(active, "TONE_MAPPING");
}

void uploadConfig() {
    GLuint active = renderer.active_id;
    glUseProgram(active);
    glUniform1i(renderer.loc_depth, config.depth);
    glUniform1i(renderer.loc_spp, config.samples_per_pixel);
    glUniform1i(renderer.loc_rr_min, config.rr_min_bounces);
    glUniform1f(renderer.loc_rr_max, config.rr_max_survival);
    glUniform1f(renderer.loc_aperture, config.cam_aperture);
    glUniform1f(renderer.loc_focal_dist, config.cam_focal_distance);
    glUniform1i(renderer.loc_focal_debug, config.focal_debug ? 1 : 0);
    glUniform1f(renderer.loc_focal_band, config.focal_band_debug);
    glUniform1i(renderer.loc_background, config.background);
    glUniform1i(renderer.loc_tone_map, config.tone_mapping);
}

void applyConfig() {
    uploadConfig();
    resetAccumulation();
    printf("\nConfig applied, accumulation reset\n");
}

void loadScene() {
    auto test_triangles = makeTestMesh();
    vector<GPUMaterial> test_materials = {{{1.0f, 1.0f, 1.0f, 1}, {0,0,0,0}, 0, 0, {0,0}}};

    vector<GPUTriangle> tris; vector<GPUMaterial> mats;

    MeshBounds bounds;

    mat4 transform = translate(mat4(1.0f), vec3(0, 0, -1));
    transform = scale(transform, vec3(0.01f));
    transform = rotate(transform, radians(90.0f), vec3(1, 0, 0));
    //transform = rotate(transform, radians(180.0f), vec3(0, 1, 0));
    //transform = rotate(transform, radians(180.0f), vec3(0, 0, 1));

    loadMesh("../models/stanford_bunny_pbr/scene.gltf", tris, mats, transform, &bounds);
    for (auto& mat : mats) { //temp test
        mat.albedo = vec4(0.8f, 0.3f, 0.1f, 1.0f);  // laranja
        mat.type = 0;
    }
    
    vector<BVHNode> bvh_nodes;
    buildBVH(tris, bvh_nodes);
    
    uploadMesh(tris, mats, triangle_ssbo, material_ssbo);
    uploadBVH(bvh_nodes, bvh_ssbo);

    glUseProgram(renderer.active_id);
    glUniform1i(loc_tri_count, (int)tris.size());
    glUniform1i(loc_bvh_root, 0);
    glUniform3f(loc_aabb_min, bounds.min_bound.x, bounds.min_bound.y, bounds.min_bound.z);
    glUniform3f(loc_aabb_max, bounds.max_bound.x, bounds.max_bound.y, bounds.max_bound.z);
}


bool transferDataToGPU(void) {
    initShaders();

    //Select shader
    renderer.active_id = USE_COMPUTE_SH ? renderer.pathtr_comp_id : renderer.pathtr_frag_id;

    initUniforms();

    //Textures DSA
    glCreateTextures(GL_TEXTURE_2D, 2, renderer.tex);
    for (int i = 0; i < 2; i++) {
        glTextureStorage2D(renderer.tex[i], 1, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    //FBO DSA ping-pong
    glCreateFramebuffers(2, renderer.fbo);
    for (int i = 0; i < 2; i++) {
        glNamedFramebufferTexture(renderer.fbo[i], GL_COLOR_ATTACHMENT0, renderer.tex[i], 0);
        if (glCheckNamedFramebufferStatus(renderer.fbo[i], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "FBO %d incomplete. Check FBO DSA implementation.\n", i);
            return false;
        }
    }

    glCreateVertexArrays(1, &renderer.vao);
    glBindVertexArray(renderer.vao);

    //Mesh Loading//
    loadScene();

    uploadConfig();

    return true;
}

void cleanDataFromGPU() {
    glDeleteVertexArrays(1, &renderer.vao);
    glDeleteTextures(2, renderer.tex);
    glDeleteFramebuffers(2, renderer.fbo);
    
    GLuint active_id = USE_COMPUTE_SH ? renderer.pathtr_comp_id : renderer.pathtr_frag_id;
    glDeleteProgram(active_id);
    glDeleteProgram(renderer.display_id);

    glDeleteBuffers(1, &triangle_ssbo);
    glDeleteBuffers(1, &material_ssbo);
    glDeleteBuffers(1, &bvh_ssbo);
    
}

void display(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glUseProgram(renderer.display_id);
    
    glBindTextureUnit(0, renderer.tex[renderer.cur_f]);
    glUniform1i(renderer.loc_tex, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glfwSwapBuffers(window);
    glfwPollEvents();
}

void resetAccumulation() {
    renderer.frame_id = 0;
    renderer.cur_f = 0;
    renderer.prev_f = 1;
}
/*----------------------------------------------------------
  Draw to GPU
*/
void draw(void) {
    double time_now, time_elapsed;
    
    //Step 1 Path Tracing
    glUseProgram(renderer.active_id);
    if(USE_COMPUTE_SH) {
        //Compute pass, no fbo, no quad screen
        glUniform2f(renderer.loc_res, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
        glUniform1i(renderer.loc_frame, renderer.frame_id);

        glBindImageTexture(0, renderer.tex[renderer.cur_f], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(1, renderer.tex[renderer.prev_f], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

        //Dispatch 16x16 work groups
        int groups_x = (WINDOW_WIDTH + COMPUTE_LOCAL_X - 1) / 16;
        int groups_y = (WINDOW_HEIGHT + COMPUTE_LOCAL_Y - 1) / 16;
        glDispatchCompute(groups_x, groups_y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
    else {
        //Fragment pass
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
        glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
        glUniform2f(renderer.loc_res, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
        glUniform1i(renderer.loc_frame, renderer.frame_id);
        glBindTextureUnit(0, renderer.tex[renderer.prev_f]);
        glUniform1i(renderer.loc_prev, 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    int tmp = renderer.cur_f; renderer.cur_f = renderer.prev_f; renderer.prev_f = tmp;
    renderer.frame_id++;

    if(renderer.frame_id == MAX_SAMPLES)
        printf("\n%s\nRender complete - %d samples in %.1fs\n", txt_sep, renderer.frame_id, time_elapsed);
    
    //Step 2 Display : accumulated texture to screen
    display();

    // Metrics
    if (renderer.frame_id % 10 == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        time_now = ts.tv_sec + ts.tv_nsec * 1e-9;
        time_elapsed = time_now - metrics.start_time;
        
        metrics.fps = renderer.frame_id / time_elapsed;
        metrics.samples_per_s = (double)renderer.frame_id * WINDOW_WIDTH * WINDOW_HEIGHT / time_elapsed;
        double ms_frame = time_elapsed / renderer.frame_id * 1000.0;
        char time_buf[32];
        formatTime(time_elapsed, time_buf, sizeof(time_buf));

        printf("\rSamples/pixel: %d | FPS: %.1f | %.1fms/frame | %.1f | Time:%s",
            renderer.frame_id, metrics.fps, ms_frame, metrics.samples_per_s / 1e6, time_buf);
        fflush(stdout);
    }
}

/*----------------------------------------------------------
  Render Extras
*/
void saveScreenshot() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    vector<unsigned char> pixels(WINDOW_WIDTH * WINDOW_HEIGHT * 3);
    char filename[64];
    snprintf(filename, sizeof(filename), "render_%4d%02d%02d_%02d%02d%02d_%d.png",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec, renderer.frame_id);
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glReadPixels(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());

    //flip y
    for (int y = 0; y < WINDOW_HEIGHT / 2; y++) {
        int y2 = WINDOW_HEIGHT - 1 - y;
        for (int x = 0; x < WINDOW_WIDTH * 3; x++)
            swap(pixels[y * WINDOW_WIDTH * 3 + x], pixels[y2 * WINDOW_WIDTH * 3 + x]);
    }
    stbi_write_png(filename, WINDOW_WIDTH, WINDOW_HEIGHT, 3, pixels.data(), WINDOW_WIDTH * 3);
    printf("\nSaved screenshot %s\n", filename);
}