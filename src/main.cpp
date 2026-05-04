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
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#include "config.hpp"
#include "common/shader.hpp"
#include "mesh.hpp"
#include "bvh.hpp"

#define VERSION "1.2.1"

using namespace std;
using namespace glm;

/*----------------------------------------------------------
  Global Variables
*/

struct Metrics {
    //Time metrics
    double start_time = 0.0;
    double fps, samples_per_s;
    double ms_frame;
    double last_frame_time = 0.0;
    char time_buf[32];
};

GLFWwindow* window;

Renderer renderer;
CameraConfig camera;

double mouse_last_x = 0.0, mouse_last_y = 0.0;
bool mouse_first = true;
bool mouse_captured = false;

Metrics metrics;

RenderConfig config;

static const int WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720;
int render_w = WINDOW_WIDTH, render_h = WINDOW_HEIGHT;

#define WINDOW_TITLE "Path Tracer"

static const int COMPUTE_LOCAL_X = 16;
static const int COMPUTE_LOCAL_Y = 16;

const int V_SYNC = 0;
uint MAX_SAMPLES = 10;


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
void onMouseMove(GLFWwindow* w, double x, double y);
void onMouseButton(GLFWwindow* w, int button, int action, int mods);
void onMouseScroll(GLFWwindow* w, double xoffset, double yoffset);
vec3 cameraForward();
void processMovement();
void formatTime(double seconds, char*buf, int buf_size);
bool initShaders();
void loadScene();
void uploadConfig();
void applyConfig();
void uploadCamera();
bool transferDataToGPU(void);
void cleanDataFromGPU();
void display(void);
void clearTextures();
void resetAccumulation();
void draw(void);
void saveScreenshot();
void drawUI();

/*----------------------------------------------------------
  Input handle
*/
void onKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
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

void onMouseMove(GLFWwindow* w, double x, double y) {
    if (ImGui::GetIO().WantCaptureMouse) return;
    if (!mouse_captured) return;

    if(mouse_first) {
        mouse_last_x = x;
        mouse_last_y = y;
        mouse_first = false;
        return;
    }

    double dx = x - mouse_last_x;
    double dy = y  - mouse_last_y;
    mouse_last_x = x;
    mouse_last_y = y;

    camera.yaw -= (float)dx * camera.mouse_sens;
    camera.pitch -= (float)dy * camera.mouse_sens;

    camera.pitch = clamp(camera.pitch, -1.5f, 1.5f);

    camera.moving = true;
}

void onMouseButton(GLFWwindow* w, int button, int action, int mods) {
    if (ImGui::GetIO().WantCaptureMouse) return; 
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            mouse_captured = true;
            mouse_first = true;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        }
        else if (action == GLFW_RELEASE) {
            mouse_captured = false;
            glfwSetInputMode(w, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
}

void onMouseScroll(GLFWwindow* w, double xoffset, double yoffset) {
    //Simulate Unity's camera control speed multiplier
    camera.move_speed *= (yoffset > 0) ? 1.2f : 0.8f;
    camera.move_speed = clamp(camera.move_speed, 0.001f, 10.0f);

    printf("\nMove speed: %.3f\n", camera.move_speed);
}

vec3 cameraForward() {
    return normalize(vec3(cos(camera.pitch) * cos(camera.yaw), cos(camera.pitch) * sin(camera.yaw), sin(camera.pitch)));
}

int frames_since_moved = 9999;

void processMovement() {
    vec3 forward = cameraForward();
    vec3 right = normalize(cross(forward, camera.up));
    float speed = camera.move_speed;
    bool moved = false;

    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
        speed *= 2;
    }
    
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.position += forward * speed;
        moved = true;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.position -= forward * speed;
        moved = true;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.position -= right * speed;
        moved = true;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.position += right * speed;
        moved = true;
    }

    if (moved || camera.moving) {
    camera.lookat  = camera.position + cameraForward();
    camera.moving  = false;
    uploadCamera();

    int target_w = config.moving_resolution;
    int target_h = glm::max(16, (int)(config.moving_resolution * (float)WINDOW_HEIGHT / (float)WINDOW_WIDTH));
    target_w = glm::max(16, (target_w / 16) * 16);
    target_h = glm::max(16, (target_h / 16) * 16);

    if (render_w != target_w || render_h != target_h) {
        render_w = target_w;
        render_h = target_h;
        glUseProgram(renderer.active_id);
        glUniform2f(renderer.loc_res, (float)render_w, (float)render_h);
        if (!USE_COMPUTE_SH) {
            glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
            glViewport(0, 0, render_w, render_h);
        }
        clearTextures();
        renderer.cur_f  = 0;
        renderer.prev_f = 1;
    }

    renderer.frame_id  = 0;
    frames_since_moved = 0;
}
    else {
        frames_since_moved++;
        
        if (frames_since_moved == 5 && render_w != WINDOW_WIDTH) {
            render_w = WINDOW_WIDTH;
            render_h = WINDOW_HEIGHT;
            glUseProgram(renderer.active_id);
            glUniform2f(renderer.loc_res, (float)render_w, (float)render_h);
            if(!USE_COMPUTE_SH) {
                glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
                glViewport(0, 0, render_w, render_h);
            }
            clearTextures();
            resetAccumulation();
        }
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
    glfwSetCursorPosCallback(window, onMouseMove);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetScrollCallback(window, onMouseScroll);
    
    //Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    if(!transferDataToGPU())
    return -1;
    
    printf("%s\n%s v%s\nResolution: %dx%d\n", txt_sep, WINDOW_TITLE, VERSION, WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("Shader: %s\nPress \tESC to quit\n \tF12 to screenshot render\n%s\n", USE_COMPUTE_SH ? "Compute" : "Fragment", txt_sep);

    //Time init
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics.start_time = ts.tv_sec + ts.tv_nsec * 1e-9;

    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
        //avoids render lock when in preview rendering and reaches max samples(moving camera)
        bool is_moving = (render_w != WINDOW_WIDTH);
        //Suspend the rendering after completion to avoid useless GPU processing
        if (MAX_SAMPLES > 0 && renderer.frame_id >= MAX_SAMPLES && !is_moving) {
            glfwWaitEvents(); //Gets input events and avoids program freezing
            processMovement();

            display();
            drawUI();
            glfwSwapBuffers(window);
            glfwPollEvents();
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

    renderer.loc_cam_pos = glGetUniformLocation(active, "camera_position");
    renderer.loc_cam_lookat = glGetUniformLocation(active, "camera_lookat");
    renderer.loc_cam_up = glGetUniformLocation(active, "camera_up");

    renderer.loc_display_render_res = glGetUniformLocation(renderer.display_id, "render_resolution");
    renderer.loc_display_res = glGetUniformLocation(renderer.display_id, "display_resolution");
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

void uploadCamera() {
    glUseProgram(renderer.active_id);
    glUniform3f(renderer.loc_cam_pos, camera.position.x, camera.position.y, camera.position.z);
    glUniform3f(renderer.loc_cam_lookat, camera.lookat.x, camera.lookat.y, camera.lookat.z);
    glUniform3f(renderer.loc_cam_up, camera.up.x, camera.up.y, camera.up.z);
}

void loadScene() {
    vector<GPUTriangle> tris; vector<GPUMaterial> mats;

    MeshBounds bounds;

    mat4 transform = translate(mat4(1.0f), vec3(0, 0, -1));
    transform = scale(transform, vec3(10.0f));
    transform = rotate(transform, radians(90.0f), vec3(1, 0, 0));
    //transform = rotate(transform, radians(180.0f), vec3(0, 1, 0));
    //transform = rotate(transform, radians(180.0f), vec3(0, 0, 1));

    loadMesh("../models/utah_teapot_pbr/scene.gltf", tris, mats, transform, &bounds);
    /*
    for (auto& mat : mats) { //temp test
        mat.albedo = vec4(0.8f, 0.3f, 0.1f, 1.0f);  // laranja
        mat.type = 2;
    }
    */
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
        glTextureStorage2D(renderer.tex[i], 1, GL_RGBA32F, render_w, render_h);
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
    uploadCamera();

    return true;
}

void cleanDataFromGPU() {
    glDeleteVertexArrays(1, &renderer.vao);
    glDeleteTextures(2, renderer.tex);
    glDeleteFramebuffers(2, renderer.fbo);
    
    glDeleteProgram(renderer.pathtr_comp_id);
    glDeleteProgram(renderer.pathtr_frag_id);
    glDeleteProgram(renderer.display_id);

    glDeleteBuffers(1, &triangle_ssbo);
    glDeleteBuffers(1, &material_ssbo);
    glDeleteBuffers(1, &bvh_ssbo);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void display(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glUseProgram(renderer.display_id);

    glUniform2f(renderer.loc_display_render_res, (float)render_w, (float)render_h);
    glUniform2f(renderer.loc_display_res, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
    
    glBindTextureUnit(0, renderer.tex[renderer.cur_f]);
    glUniform1i(renderer.loc_tex, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

}

void clearTextures() {
    float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(renderer.tex[0], 0, GL_RGBA, GL_FLOAT, zero);
    glClearTexImage(renderer.tex[1], 0, GL_RGBA, GL_FLOAT, zero);
}

void resetAccumulation() {
    renderer.frame_id = 0;
    renderer.cur_f = 0;
    renderer.prev_f = 1;
    clearTextures();

    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics.start_time = ts.tv_sec + ts.tv_nsec * 1e-9;
    metrics.fps = 0.0;
    metrics.samples_per_s = 0.0;
    snprintf(metrics.time_buf, sizeof(metrics.time_buf), "0.0s");
}
/*----------------------------------------------------------
  Draw to GPU
*/
void draw(void) {
    struct timespec ts_now;
    double time_now, time_elapsed;

    processMovement();
    
    //Step 1 Path Tracing
    glUseProgram(renderer.active_id);
    if(USE_COMPUTE_SH) {
        //Compute pass, no fbo, no quad screen
        glUniform2f(renderer.loc_res, (float)render_w, (float)render_h);
        glUniform1i(renderer.loc_frame, renderer.frame_id);

        glBindImageTexture(0, renderer.tex[renderer.cur_f], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(1, renderer.tex[renderer.prev_f], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

        //Dispatch 16x16 work groups
        int groups_x = (render_w + COMPUTE_LOCAL_X - 1) / 16;
        int groups_y = (render_h + COMPUTE_LOCAL_Y - 1) / 16;
        glDispatchCompute(groups_x, groups_y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
    else {
        //Fragment pass
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
        glViewport(0, 0, render_w, render_h);
        glUniform2f(renderer.loc_res, (float)render_w, (float)render_h);
        glUniform1i(renderer.loc_frame, renderer.frame_id);
        glBindTextureUnit(0, renderer.tex[renderer.prev_f]);
        glUniform1i(renderer.loc_prev, 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    int tmp = renderer.cur_f; renderer.cur_f = renderer.prev_f; renderer.prev_f = tmp;
    renderer.frame_id++;

    if(renderer.frame_id == (int)MAX_SAMPLES)
        printf("\n%s\nRender complete - %d samples in %.1fs\n", txt_sep, renderer.frame_id, time_elapsed);
    
    //Step 2 Display : accumulated texture to screen
    display();

    //Step 3 UI
    drawUI();

    glfwSwapBuffers(window);
    glfwPollEvents();

    // Metrics
    if (renderer.frame_id % 10 == 0) {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        time_now = ts_now.tv_sec + ts_now.tv_nsec * 1e-9;
        time_elapsed = time_now - metrics.start_time;
        
        metrics.fps = renderer.frame_id / time_elapsed;
        metrics.samples_per_s = (double)renderer.frame_id * render_w * render_h / time_elapsed;
        metrics.ms_frame = time_elapsed / renderer.frame_id * 1000.0;
        
        formatTime(time_elapsed, metrics.time_buf, sizeof(metrics.time_buf));

        printf("\rSamples/pixel: %d | FPS: %.1f | %.1fms/frame | %.1f | Time:%s",
            renderer.frame_id, metrics.fps, metrics.ms_frame, metrics.samples_per_s / 1e6, metrics.time_buf);
        fflush(stdout);
    }
}

/*----------------------------------------------------------
  Render Extras
*/
void saveScreenshot() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    char filename[64];
    snprintf(filename, sizeof(filename), "render_%04d%02d%02d_%02d%02d%02d_%ds.png",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec, renderer.frame_id);

    // Reads directly from texture (maybe avoids problems with Wayland or X11 windows)
    vector<float> pixels_float(WINDOW_WIDTH * WINDOW_HEIGHT * 4);
    glGetTextureImage(renderer.tex[renderer.cur_f], 0, GL_RGBA, GL_FLOAT, pixels_float.size() * sizeof(float), pixels_float.data());

    vector<unsigned char> pixels(WINDOW_WIDTH * WINDOW_HEIGHT * 3);
    for (int i = 0; i < WINDOW_WIDTH * WINDOW_HEIGHT; i++) {
        pixels[i*3+0] = (unsigned char)(pow(aces_approx(pixels_float[i*4+0]), 1.0f/2.2f) * 255.0f);
        pixels[i*3+1] = (unsigned char)(pow(aces_approx(pixels_float[i*4+1]), 1.0f/2.2f) * 255.0f);
        pixels[i*3+2] = (unsigned char)(pow(aces_approx(pixels_float[i*4+2]), 1.0f/2.2f) * 255.0f);
    }

    //flip y
    for (int y = 0; y < WINDOW_HEIGHT / 2; y++) {
        int y2 = WINDOW_HEIGHT - 1 - y;
        for (int x = 0; x < WINDOW_WIDTH * 3; x++)
            swap(pixels[y * WINDOW_WIDTH * 3 + x], pixels[y2 * WINDOW_WIDTH * 3 + x]);
    }
    stbi_write_png(filename, WINDOW_WIDTH, WINDOW_HEIGHT, 3, pixels.data(), WINDOW_WIDTH * 3);
    printf("\nSaved screenshot %s\n", filename);
}

/*----------------------------------------------------------
  UI Draw
*/
void drawUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(280, 180), ImGuiCond_FirstUseEver);
    
    //-- Metrics -------------
    if(ImGui::Begin("Metrics")) {
        ImGui::Text("SPP:     %d / %d", renderer.frame_id, MAX_SAMPLES);
        ImGui::Text("FPS:\t%.1f (%.1fms)", metrics.fps, metrics.ms_frame);
        ImGui::Text("Time:\t%s", metrics.time_buf);
        ImGui::Text("Shader:  %s", USE_COMPUTE_SH ? "Compute" : "Fragment");
        ImGui::Text("Res:     %dx%d", render_w, render_h);

        if (ImGui::Button("Reset Accumulation"))
            resetAccumulation();
        ImGui::SameLine();
        if (ImGui::Button("Screenshot"))
            saveScreenshot();
    }

    ImGui::End();

    //-- Render Settings -------------
    ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH - 310, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 400), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Render")) {
        //-- Preview Config -------------
        if(ImGui::CollapsingHeader("Preview")) {
            int res = config.moving_resolution;
            if(ImGui::SliderInt("Preview Res", &res, 32, 512)) {
                res = (res / 16) * 16;
                res = glm::max(32, res);
                config.moving_resolution = res;
            }
        }

        if(ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool changed = false;
            RenderConfig defaults;
    
            auto resetBtn = [&](const char* id, auto& field, auto default_val) -> bool {
                ImGui::SameLine();
                ImGui::PushItemWidth(-1);
                bool r = ImGui::SmallButton(id);
                ImGui::SetItemTooltip("Reset to default");
                if(r && field != default_val) { field = default_val; return true; }
                ImGui::PopItemWidth();
                return false;
            };
            
            int samples = (int)MAX_SAMPLES;
            if(ImGui::InputInt("Max Samples", &samples, 1, 5000)) {
                samples = glm::max(1, samples);
                MAX_SAMPLES = (uint)samples;
            }
            ImGui::SetItemTooltip("Max progressive samples per pixel");
            changed |= ImGui::SliderInt("Depth", &config.depth, 1, 50);
            changed |= resetBtn("*##depth", config.depth, defaults.depth);
    
            changed |= ImGui::SliderInt("Rays/Pixel", &config.samples_per_pixel, 1, 16);
            changed |= resetBtn("*##spp", config.samples_per_pixel, defaults.samples_per_pixel);
    
            ImGui::SeparatorText("Russian Roulette");
    
            bool rr_enabled = (config.rr_min_bounces > 0);
            if(ImGui::Checkbox("Enable RR", &rr_enabled)) {
                config.rr_min_bounces = rr_enabled ? defaults.rr_min_bounces : 0;
                changed = true;
            }
            
            changed |= ImGui::SliderInt("Min Bounces", &config.rr_min_bounces, 0, 10);
            ImGui::SetItemTooltip("Minimum bounces before enabling Russian Roulette\n0 = off");
            changed |= resetBtn("*##rrmin", config.rr_min_bounces, defaults.rr_min_bounces);
    
            changed |= ImGui::SliderFloat("Survival Chance", &config.rr_max_survival, 0.1f, 1.0f, "%.2f");
            ImGui::SetItemTooltip("Probability of ray survival after each bounce");
            changed |= resetBtn("*##rrsur", config.rr_max_survival, defaults.rr_max_survival);
    
            ImGui::SeparatorText("Environment");
    
            const char* bg_names[] = {"Black", "White"};
            changed |= ImGui::Combo("Background", &config.background, bg_names, 2);
    
            const char* tm_names[] = {"None", "Reinhard", "ACES"};
            changed |= ImGui::Combo("Tone Map", &config.tone_mapping, tm_names, 3);
    
            if(changed) applyConfig();
        }
    }

    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}