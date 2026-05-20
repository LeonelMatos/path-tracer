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
#include <algorithm>
#include <thread>
#include <atomic>
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

#include "common/shader.hpp"
#include "mesh.hpp"
#include "bvh.hpp"
#include "config.hpp"
#include "loader.hpp"

#define VERSION "1.3.2"

using namespace std;
using namespace glm;

/*----------------------------------------------------------
Global Variables
*/

struct Metrics {
    //Time metrics
    double render_start_time = 0.0;
    double fps, samples_per_s;
    double ms_frame;
    /// \note also known as delta time
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

#define WINDOW_TITLE "Path Tracer"

#define WINDOW_TITLE_VERSION WINDOW_TITLE " v" VERSION

static const int COMPUTE_LOCAL_X = 16;
static const int COMPUTE_LOCAL_Y = 16;

const char* txt_sep = "----------------------------";


vector<GPULight> analytic_lights;
int sun_light_index = -1;

/*----------------------------------------------------------
  Function Declarations
*/
void onKeyPress(GLFWwindow* window, int key, int scancode, int action, int mods);
void onMouseMove(GLFWwindow* w, double x, double y);
void onMouseButton(GLFWwindow* w, int button, int action, int mods);
void onMouseScroll(GLFWwindow* w, double xoffset, double yoffset);
void onWindowResize(GLFWwindow* w, int width, int height);
vec3 cameraForward();
void setPreviewResolution();
void setFullResolution();
void startMoving();
void processMovement();
void formatTime(double seconds, char*buf, int buf_size);
bool initShaders();
void loadScene();
void uploadConfig();
void applyConfig();
void uploadCamera();
void uploadSun();
bool transferDataToGPU(void);
void cleanDataFromGPU();
void display(void);
void clearTextures();
void resetAccumulation();
void draw(void);
void saveScreenshot();
void drawGrid();
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
        case GLFW_KEY_H:
        case GLFW_KEY_0:
            camera.returning_home = true;
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

    camera.pitch = std::clamp(camera.pitch, -1.5f, 1.5f);

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
    camera.move_speed = std::clamp(camera.move_speed, 0.001f, 10.0f);

    printf("\nMove speed: %.3f\n", camera.move_speed);
}

vec3 cameraForward() {
    return normalize(vec3(cos(camera.pitch) * cos(camera.yaw), cos(camera.pitch) * sin(camera.yaw), sin(camera.pitch)));
}

int frames_since_moved = 9999;

/// Changes the render resolution to preview mode, aux function
void setPreviewResolution() {
    int target_w = config.moving_resolution;
    int target_h = glm::max(16, (int)(config.moving_resolution * (float)WINDOW_HEIGHT / (float)WINDOW_WIDTH));
    target_w = glm::max(16, (target_w / 16) * 16);
    target_h = glm::max(16, (target_h / 16) * 16);

    if (renderer.render_w == target_w || renderer.render_h == target_h) return;

    renderer.render_w = target_w;
    renderer.render_h = target_h;
    glUseProgram(renderer.active_id);
    glUniform2f(renderer.loc_res, (float)renderer.render_w, (float)renderer.render_h);
    if (!USE_COMPUTE_SH) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
        glViewport(0, 0, renderer.render_w, renderer.render_h);
    }
    clearTextures();
    renderer.cur_f  = 0;
    renderer.prev_f = 1;
}

/// Changes the render resolution to full mode
void setFullResolution() {
    if (renderer.render_w == WINDOW_WIDTH && renderer.render_h == WINDOW_HEIGHT) return;

    renderer.render_w = WINDOW_WIDTH;
    renderer.render_h = WINDOW_HEIGHT;
    glUseProgram(renderer.active_id);
    glUniform2f(renderer.loc_res, (float)renderer.render_w, (float)renderer.render_h);
    if(!USE_COMPUTE_SH) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
        glViewport(0, 0, renderer.render_w, renderer.render_h);
    }
    clearTextures();
    resetAccumulation();
}

void startMoving() {
    setPreviewResolution();
    renderer.frame_id = 0;
    frames_since_moved = 0;
}

/// \brief Checks if the user pressed the WASD keys
/// \return true if any WASD key pressed
bool useMoveKeys() {
    return (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);
}

void processMovement() {
    //Reset camera position to start
    if(camera.returning_home) {
        if(useMoveKeys() || camera.moving) {
            camera.returning_home = false;
        }
        else {
            CameraConfig camera_default;
            float t = glm::clamp((float)(metrics.last_frame_time * camera.return_speed), 0.0f, 0.15f);

            camera.position = mix(camera.position, camera_default.position, t);
            camera.pitch = glm::mix(camera.pitch, camera_default.pitch, t);
            camera.yaw = glm::mix(camera.yaw, camera_default.yaw, t);
            camera.lookat = camera.position + cameraForward();
            uploadCamera();
            startMoving();
            
            //Reached position
            if(length(camera.position - camera_default.position) < 0.001f && abs(camera.yaw - camera_default.yaw) < 0.001f &&
            abs(camera.pitch - camera_default.pitch) < 0.001f) {
                camera.position = camera_default.position;
                camera.pitch = camera_default.pitch;
                camera.yaw = camera_default.yaw;
                camera.returning_home = false;
                uploadCamera();
                resetAccumulation();
            }
            return;
        }
    }
        
    vec3 forward = cameraForward();
    vec3 right = normalize(cross(forward, camera.up));
    float speed = camera.move_speed;
    bool moved = false;

    if(glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) speed *= 2;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        camera.position += forward * speed; moved = true;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        camera.position -= forward * speed; moved = true;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        camera.position -= right * speed; moved = true;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        camera.position += right * speed; moved = true;
    }

    if (moved || camera.moving) {
        camera.lookat  = camera.position + cameraForward();
        camera.moving  = false;
        uploadCamera();
        startMoving();
        return;
    }
    frames_since_moved++;
    if (frames_since_moved == 5) setFullResolution();
}

void onWindowResize(GLFWwindow* window, int width, int height) {
    //minimized
    if (width == 0 || height == 0) return;

    WINDOW_WIDTH = width;
    WINDOW_HEIGHT = height;

    glDeleteTextures(2, renderer.tex);
    glDeleteFramebuffers(2, renderer.fbo);

    glCreateTextures(GL_TEXTURE_2D, 2, renderer.tex);
    for (int i = 0; i < 2; i++) {
        glTextureStorage2D(renderer.tex[i], 1, GL_RGBA32F, width, height);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(renderer.tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    glCreateFramebuffers(2, renderer.fbo);
    for(int i = 0; i < 2; i++)
        glNamedFramebufferTexture(renderer.fbo[i], GL_COLOR_ATTACHMENT0, renderer.tex[i], 0);

    renderer.render_w = width;
    renderer.render_h = height;

    glUseProgram(renderer.active_id);
    glUniform2f(renderer.loc_res, (float)width, (float)height);

    resetAccumulation();
}

//----------------------------------------------------------
int main(void) {
    if (!glfwInit()) { fprintf(stderr, "Failed to init GLFW\n"); return -1; }
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE_VERSION, NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(renderer.v_sync);
    
    glewExperimental = GL_TRUE;
    glewInit();

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    glfwSetKeyCallback(window, onKeyPress);
    glfwSetCursorPosCallback(window, onMouseMove);
    glfwSetMouseButtonCallback(window, onMouseButton);
    glfwSetScrollCallback(window, onMouseScroll);
    glfwSetFramebufferSizeCallback(window, onWindowResize);
    
    //Init ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;
    io.ConfigWindowsResizeFromEdges = true;

    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

    if(!transferDataToGPU())
        return -1;
    
    printf("%s\n%s v%s\nResolution: %dx%d\n", txt_sep, WINDOW_TITLE, VERSION, WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("Shader: %s\nPress \tESC to quit\n \tF12 to screenshot render\n \t0 or H to center camera\n%s\n", USE_COMPUTE_SH ? "Compute" : "Fragment", txt_sep);

    //Time init
    struct timespec ts, ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics.render_start_time = ts.tv_sec + ts.tv_nsec * 1e-9;

    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
        //avoids render lock when in preview rendering and reaches max samples(moving camera)
        bool is_moving = (renderer.render_w != WINDOW_WIDTH);
        //Checks if needs new model loading
        if(loader.upload_pending) {
            glUseProgram(renderer.active_id);
            uploadMesh(loader.pending_tris, loader.pending_mats, renderer.triangle_ssbo, renderer.material_ssbo);
            uploadBVH(loader.pending_bvh, renderer.bvh_ssbo);

            int light_count = uploadLights(loader.pending_tris, loader.pending_mats, renderer.light_ssbo);
            analytic_lights.clear();
            uploadSun();

            MeshBounds& b = loader.pending_bounds;
            glUniform1i(renderer.loc_tri_count, (int)loader.pending_tris.size());
            glUniform1i(renderer.loc_light_count, light_count);
            glUniform1i(renderer.loc_bvh_root, 0);
            glUniform3f(renderer.loc_aabb_min, b.min_bound.x, b.min_bound.y, b.min_bound.z);
            glUniform3f(renderer.loc_aabb_max, b.max_bound.x, b.max_bound.y, b.max_bound.z);

            loader.upload_pending = false;
            resetAccumulation();
        }
        //Suspend the rendering after completion to avoid useless GPU processing
        if (renderer.MAX_SAMPLES > 0 && renderer.frame_id >= renderer.MAX_SAMPLES && !is_moving) {
            clock_gettime(CLOCK_MONOTONIC, &ts_start);

            glfwWaitEvents(); //Gets input events and avoids program freezing
            processMovement();

            display();
            drawGrid();
            drawUI();
            glfwSwapBuffers(window);
            glfwPollEvents();

            clock_gettime(CLOCK_MONOTONIC, &ts_end);
            metrics.last_frame_time = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) * 1e-9;
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
    renderer.grid_id = LoadShaders({
        { GL_VERTEX_SHADER, "shaders/grid.vert" },
        { GL_FRAGMENT_SHADER, "shaders/grid.frag" }
    });
    if(!renderer.display_id || !renderer.pathtr_frag_id || !renderer.pathtr_comp_id || !renderer.grid_id) {
        glfwTerminate(); return false;
    }
    return true;
}

void initUniforms() {
    GLuint active = renderer.active_id;

    renderer.loc_res = glGetUniformLocation(active, "resolution");
    renderer.loc_frame = glGetUniformLocation(active, "frame_id");
    renderer.loc_tex = glGetUniformLocation(active, "tex");
    renderer.loc_tri_count = glGetUniformLocation(active, "triangle_count");
    renderer.loc_aabb_min = glGetUniformLocation(active, "mesh_aabb_min");
    renderer.loc_aabb_max = glGetUniformLocation(active, "mesh_aabb_max");
    renderer.loc_bvh_root = glGetUniformLocation(active, "bvh_root");
    renderer.loc_bvh_heatmap = glGetUniformLocation(active, "USE_BVH_HEATMAP");
    renderer.loc_bvh_heatmap_scale = glGetUniformLocation(active, "BVH_HEATMAP_SCALE");
    renderer.loc_light_count = glGetUniformLocation(active, "light_count");
    renderer.loc_analytic_light_count = glGetUniformLocation(active, "analytic_light_count");

    if(!USE_COMPUTE_SH) {
        renderer.loc_prev = glGetUniformLocation(renderer.pathtr_frag_id, "prev_frame");
    }
    renderer.loc_depth  = glGetUniformLocation(active, "DEPTH");
    renderer.loc_spp    = glGetUniformLocation(active, "SAMPLES_PER_PIXEL");
    renderer.loc_rr_min = glGetUniformLocation(active, "RR_MIN_BOUNCES");
    renderer.loc_rr_max = glGetUniformLocation(active, "RR_MAX_SURVIVAL");
    renderer.loc_cam_fov    = glGetUniformLocation(active, "CAM_FOV");
    renderer.loc_aperture   = glGetUniformLocation(active, "CAM_APERTURE");
    renderer.loc_focal_dist = glGetUniformLocation(active, "CAM_FOCAL_DISTANCE");
    renderer.loc_focal_debug = glGetUniformLocation(active, "FOCAL_DEBUG");
    renderer.loc_focal_band = glGetUniformLocation(active, "FOCAL_BAND_DEBUG");
    renderer.loc_background = glGetUniformLocation(active, "BACKGROUND");
    renderer.loc_tone_map   = glGetUniformLocation(active, "TONE_MAPPING");

    renderer.loc_cam_pos    = glGetUniformLocation(active, "camera_position");
    renderer.loc_cam_lookat = glGetUniformLocation(active, "camera_lookat");
    renderer.loc_cam_up = glGetUniformLocation(active, "camera_up");

    renderer.loc_display_render_res = glGetUniformLocation(renderer.display_id, "render_resolution");
    renderer.loc_display_res = glGetUniformLocation(renderer.display_id, "display_resolution");
    
    renderer.loc_use_nee = glGetUniformLocation(active, "USE_NEE");

    renderer.grid_loc_view = glGetUniformLocation(renderer.grid_id, "view");
    renderer.grid_loc_proj = glGetUniformLocation(renderer.grid_id, "projection");
    renderer.grid_loc_near = glGetUniformLocation(renderer.grid_id, "near_plane");
    renderer.grid_loc_far = glGetUniformLocation(renderer.grid_id, "far_plane");
    renderer.grid_loc_cam_pos = glGetUniformLocation(renderer.grid_id, "camera_pos");

    renderer.loc_scene_preset = glGetUniformLocation(renderer.active_id, "SCENE_PRESET");
}

void uploadConfig() {
    GLuint active = renderer.active_id;
    glUseProgram(active);
    glUniform1i(renderer.loc_depth, config.depth);
    glUniform1i(renderer.loc_spp, config.samples_per_pixel);
    glUniform1i(renderer.loc_rr_min, config.rr_min_bounces);
    glUniform1f(renderer.loc_rr_max, config.rr_max_survival);
    glUniform1f(renderer.loc_cam_fov, glm::radians(config.cam_fov));
    glUniform1f(renderer.loc_aperture, config.cam_aperture);
    glUniform1f(renderer.loc_focal_dist, config.cam_focal_distance);
    glUniform1i(renderer.loc_focal_debug, config.focal_debug ? 1 : 0);
    glUniform1f(renderer.loc_focal_band, config.focal_band_debug);
    glUniform1i(renderer.loc_background, config.background);
    glUniform1i(renderer.loc_tone_map, config.tone_mapping);
    glUniform1i(renderer.loc_use_nee, config.use_nee ? 1 : 0);
    glUniform1i(renderer.loc_scene_preset, config.scene_preset);
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

void uploadSun() {
    if(sun_light_index >= 0 && sun_light_index < (int)analytic_lights.size())
        analytic_lights.erase(analytic_lights.begin() + sun_light_index);
    
    sun_light_index = -1;

    if(config.sun_enabled) {
        GPULight sun{};
        sun.emission = vec4(config.sun_color * config.sun_intensity, 0.0f);
        sun.direction = vec4(-config.sunDirection(), 0.0f);
        sun.type = LIGHT_DIRECTIONAL;

        sun_light_index = (int)analytic_lights.size();
        analytic_lights.push_back(sun);
    }
    uploadAnalyticLights(analytic_lights, renderer.analytic_light_ssbo);
    glUniform1i(renderer.loc_analytic_light_count, (int)analytic_lights.size());
}

vector<string> scanModels(const string& models_dir) {
    vector<string> paths;

    if(!filesystem::exists(models_dir)) {
        printf("Models directory not found: %s\n", models_dir.c_str());
        return paths;
    }

    for (auto& entry: filesystem::recursive_directory_iterator(models_dir)) {
        string ext = entry.path().extension().string();
        if(ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx")
            paths.push_back(entry.path().string());
    }
    sort(paths.begin(), paths.end());
    return paths;
}

void loadScene() {
    //Empty scene -> reset counters
    if(renderer.current_model.path.empty()) {
        clearMesh();
        glUseProgram(renderer.active_id);
        glUniform1i(renderer.loc_tri_count, 0);
        glUniform1i(renderer.loc_light_count, 0);
        glUniform1i(renderer.loc_bvh_root, 0);
        resetAccumulation();
        return;
    }
    renderer.is_model_loading = true;

    std::thread([]{
        vector<GPUTriangle> tris; vector<GPUMaterial> mats;
        SceneModel& model = renderer.current_model;
        MeshBounds bounds;

        
        mat4 transform = translate(mat4(1.0f), model.position);
        transform = rotate(transform, radians(model.rotation.x + 90.0f), vec3(1,0,0));
        transform = rotate(transform, radians(model.rotation.y), vec3(0,1,0));
        transform = rotate(transform, radians(model.rotation.z), vec3(0,0,1));
        transform = scale(transform, model.scale);
        
        if (!loadMesh(model.path, tris, mats, transform, &bounds)) {
            renderer.is_model_loading = false;
            return;
        }
        
        vector<BVHNode> bvh_nodes;
        buildBVH(tris, bvh_nodes);
        
        model.tri_count = (int)tris.size();
        model.mat_count = (int)mats.size();

        loader.pending_tris = tris;
        loader.pending_mats = mats;
        loader.pending_bvh  = bvh_nodes;
        loader.pending_bounds = bounds;
        loader.upload_pending = true;
        renderer.is_model_loading = false;
    }).detach();
}

bool transferDataToGPU(void) {
    initShaders();

    //Select shader
    renderer.active_id = USE_COMPUTE_SH ? renderer.pathtr_comp_id : renderer.pathtr_frag_id;
    
    initUniforms();

    //Textures DSA
    glCreateTextures(GL_TEXTURE_2D, 2, renderer.tex);
    for (int i = 0; i < 2; i++) {
        glTextureStorage2D(renderer.tex[i], 1, GL_RGBA32F, renderer.render_w, renderer.render_h);
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

    analytic_lights.clear();
    uploadSun();

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

    glDeleteBuffers(1, &renderer.triangle_ssbo);
    glDeleteBuffers(1, &renderer.light_ssbo);
    glDeleteBuffers(1, &renderer.analytic_light_ssbo);
    glDeleteBuffers(1, &renderer.material_ssbo);
    glDeleteBuffers(1, &renderer.bvh_ssbo);
    
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void display(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glUseProgram(renderer.display_id);

    glUniform2f(renderer.loc_display_render_res, (float)renderer.render_w, (float)renderer.render_h);
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
    metrics.render_start_time = ts.tv_sec + ts.tv_nsec * 1e-9;
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
        glUniform2f(renderer.loc_res, (float)renderer.render_w, (float)renderer.render_h);
        glUniform1i(renderer.loc_frame, renderer.frame_id);

        glBindImageTexture(0, renderer.tex[renderer.cur_f], 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);
        glBindImageTexture(1, renderer.tex[renderer.prev_f], 0, GL_FALSE, 0, GL_READ_ONLY, GL_RGBA32F);

        //Dispatch 16x16 work groups
        int groups_x = (renderer.render_w + COMPUTE_LOCAL_X - 1) / 16;
        int groups_y = (renderer.render_h + COMPUTE_LOCAL_Y - 1) / 16;
        glDispatchCompute(groups_x, groups_y, 1);
        glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    }
    else {
        //Fragment pass
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
        glViewport(0, 0, renderer.render_w, renderer.render_h);
        glUniform2f(renderer.loc_res, (float)renderer.render_w, (float)renderer.render_h);
        glUniform1i(renderer.loc_frame, renderer.frame_id);
        glBindTextureUnit(0, renderer.tex[renderer.prev_f]);
        glUniform1i(renderer.loc_prev, 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }

    int tmp = renderer.cur_f; renderer.cur_f = renderer.prev_f; renderer.prev_f = tmp;
    renderer.frame_id++;

    if(renderer.frame_id == (int)renderer.MAX_SAMPLES)
        printf("\n%s\nRender complete - %d samples in %.1fs\n", txt_sep, renderer.frame_id, time_elapsed);
    
    //Step 2 Display: accumulated texture to screen
    display();

    //Step 3 Grid : World view grid
    drawGrid();

    //Step 4 UI: Dear ImGui
    drawUI();

    glfwSwapBuffers(window);
    glfwPollEvents();

    // Metrics
    if (renderer.frame_id % 10 == 0) {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        time_now = ts_now.tv_sec + ts_now.tv_nsec * 1e-9;
        time_elapsed = time_now - metrics.render_start_time;
        
        metrics.fps = renderer.frame_id / time_elapsed;
        metrics.samples_per_s = (double)renderer.frame_id * renderer.render_w * renderer.render_h / time_elapsed;
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

///\brief Takes a screenshot of the render
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
    screenshot_msg = string("Saved ") + filename;
    screenshot_msg_time = glfwGetTime();
    printf("\nSaved screenshot %s\n", filename);
}

/// @brief Renders the world view grid
void drawGrid() {
    if (!renderer.show_grid) return;

    mat4 view = lookAt(camera.position, camera.lookat, camera.up);
    //check if the fov is equal to the fov in the globals shader
    mat4 proj = perspective(radians(config.cam_fov), (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.01f, 100.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);

    glUseProgram(renderer.grid_id);
    glUniformMatrix4fv(renderer.grid_loc_view, 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(renderer.grid_loc_proj, 1, GL_FALSE, value_ptr(proj));
    glUniform1f(renderer.grid_loc_near, 0.01f);
    glUniform1f(renderer.grid_loc_far, 100.0f);
    glUniform3f(renderer.grid_loc_cam_pos, camera.position.x, camera.position.y, camera.position.z);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_BLEND);
    //glEnable(GL_DEPTH_TEST);
}

/*----------------------------------------------------------
  UI Draw
*/
static vector<string> model_list;
static bool model_list_loaded = false;

void drawUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    RenderConfig render_defaults;
    CameraConfig camera_defaults;
    bool changed = false;

    auto resetBtn = [&](const char* id, auto& field, auto default_val) -> bool {
                ImGui::SameLine();
                ImGui::PushItemWidth(-1);
                bool r = ImGui::SmallButton(id);
                ImGui::SetItemTooltip("Reset to default");
                if(r && field != default_val) { field = default_val; return true; }
                ImGui::PopItemWidth();
                return false;
            };

    //-- Left Window -------------
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always, ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH * 0.22f, WINDOW_HEIGHT - 20.0f), ImGuiCond_Always);
    
    //----- Metrics -------------
    ImGui::SetNextWindowSizeConstraints(ImVec2(200, 100), ImVec2(FLT_MAX, FLT_MAX));
    
    if(ImGui::Begin("Scene Editor")) {
        if(ImGui::CollapsingHeader("Metrics", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("SPP:     %d / %d", renderer.frame_id, renderer.MAX_SAMPLES);
            ImGui::Text("FPS:\t%.1f (%.1fms)", metrics.fps, metrics.ms_frame);
            ImGui::Text("Time:\t%s", metrics.time_buf);
            ImGui::Text("Shader:  %s", USE_COMPUTE_SH ? "Compute" : "Fragment");
            ImGui::Text("Res:     %dx%d", renderer.render_w, renderer.render_h);
            ImGui::TextDisabled("v%s", VERSION);

            if (ImGui::Button("Reset Accumulation[R]"))
                resetAccumulation();
            ImGui::SameLine();
            if (ImGui::Button("Screenshot[F12]"))
                saveScreenshot();
            if(ImGui::Button("Reset Camera[H]"))
                camera.returning_home = true;
            if (!screenshot_msg.empty()) {
                double time_elapsed = glfwGetTime() - screenshot_msg_time;
                if(time_elapsed < 4.0) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", screenshot_msg.c_str());
                }
                else screenshot_msg = "";
            }
        }
        if(ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("x = %.2f, y = %.2f, z = %.2f", camera.position.x, camera.position.y, camera.position.z);

            changed |= ImGui::SliderFloat("FOV", &config.cam_fov, 10.0f, 100.0f, "%.1f°");
            resetBtn("*##fov", config.cam_fov, render_defaults.cam_fov);
            ImGui::SetItemTooltip("Field of view (degrees)");

            ImGui::SeparatorText("Mouse");
            ImGui::SliderFloat("Speed", &camera.move_speed, 0.001f, 1.0f, "%.3f");
            resetBtn("*##speed", camera.move_speed, camera_defaults.move_speed);

            ImGui::SliderFloat("Sensitivity", &camera.mouse_sens, 0.0001f, 0.1f, "%.4f");
            resetBtn("*##sens", camera.mouse_sens, camera_defaults.mouse_sens);

            ImGui::SeparatorText("Depth of Field");

            bool dof_enabled = (config.cam_aperture > 0.0f);
            if(ImGui::Checkbox("Enable DoF", &dof_enabled)) {
                config.cam_aperture = dof_enabled ? 0.05f : 0.0f;
                changed = true;
            }

            if(dof_enabled) {
                changed |= ImGui::SliderFloat("Aperture", &config.cam_aperture, 0.001f, 0.5f, "%.3f");
                resetBtn("*##aperture", config.cam_aperture, 0.05f);
                ImGui::SetItemTooltip("Size of the lens aperture\nHigher = more blur");

                changed |= ImGui::SliderFloat("Focal Dist", &config.cam_focal_distance, 0.1f, 50.0f, "%.2f");
                resetBtn("*##focal", config.cam_focal_distance, render_defaults.cam_focal_distance);
                ImGui::SetItemTooltip("Distance to focal plane");

                if(ImGui::Checkbox("Draw Focus", &config.focal_debug)) changed = true;
                ImGui::SetItemTooltip("View the focal plane");

                if(config.focal_debug) {
                    changed |= ImGui::SliderFloat("Draw Size", &config.focal_band_debug, 0.01f, 0.5f, "%.2f");
                    resetBtn("*##fband", config.focal_band_debug, render_defaults.focal_band_debug);
                }
            }
            if(changed) applyConfig();
        }
        if(ImGui::CollapsingHeader("Model Explorer", ImGuiTreeNodeFlags_DefaultOpen)) {
            if(renderer.current_model.path.empty()) {
                ImGui::TextDisabled("Empty Scene\nSelect a model to load");
                ImGui::Spacing();
            }
            if(!model_list_loaded) {
                model_list = scanModels("../models");
                model_list_loaded = true;
            }
            if(ImGui::SmallButton("Scan")) {
                model_list = scanModels("../models");
            }
            ImGui::SameLine();
            ImGui::Text("%zu models", model_list.size());
            
            ImGui::SameLine();
            //Reset Scene
            if(ImGui::SmallButton("*")) {
                renderer.current_model.path = "";
                renderer.current_model.position = vec3(0.0f);
                renderer.current_model.rotation = vec3(0.0f);
                renderer.current_model.scale = vec3(1.0f);
                loadScene();
                resetAccumulation();
            }
            ImGui::Separator();

            //Model list interface
            float list_height = WINDOW_HEIGHT * 0.2f;
            ImGui::BeginChild("model_list", ImVec2(0, list_height), true);
            for (int i = 0; i < (int)model_list.size(); i++) {
                const string& path = model_list[i];
                string folder = filesystem::path(path).parent_path().filename().string();
                string stem = filesystem::path(path).stem().string();
                string name = folder + "/" + stem;
                string label = name + "##" + to_string(i);

                bool selected = (filesystem::path(path).lexically_normal() == filesystem::path(renderer.current_model.path).lexically_normal());

                if (ImGui::Selectable(label.c_str(), selected)) {
                    if(!selected) {
                        renderer.current_model.path = path;
                        renderer.current_model.position = vec3(0.0f);
                        renderer.current_model.rotation = vec3(0.0f);
                        renderer.current_model.scale = vec3(1.0f);
                        loadScene();
                        resetAccumulation();
                    }
                }
                if(ImGui::IsItemHovered()) {
                    string parent = filesystem::path(path).parent_path().filename().string();
                    ImGui::SetTooltip("%s/%s", parent.c_str(), name.c_str());
                }
            }
            ImGui::EndChild();
        }
        if(ImGui::CollapsingHeader("Transform")) {
            bool t_changed = false;
            SceneModel& model = renderer.current_model;

            t_changed |= ImGui::DragFloat3("Position", value_ptr(model.position), 0.01f);
            t_changed |= ImGui::DragFloat3("Rotation", value_ptr(model.rotation), 1.0f, -360.0f, 360.0f, "%.1f°");
            t_changed |= ImGui::DragFloat("Scale", &model.scale.x, 0.01f, 0.001f, 100.0f);
            if(t_changed) model.scale.y = model.scale.z = model.scale.x;

            ImGui::SameLine();
            if(ImGui::Button("Reset##transform")) {
                model.position = vec3(0.0f);
                model.rotation = vec3(0.0f);
                model.scale = vec3(1.0f);
                t_changed = true;
            }

            if(t_changed && !model.path.empty()) {
                loadScene();
                resetAccumulation();
            }

            ImGui::Separator();
            ImGui::Text("%d triangles", model.tri_count);
            ImGui::Text("%d materials", model.mat_count);
            ImGui::Text("%s", filesystem::path(model.path).filename().string().c_str());
        }
    }
    ImGui::End();

    //-- Right Window -------------
    ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH - WINDOW_WIDTH * 0.22f - 10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH * 0.22f, WINDOW_HEIGHT - 20.0f), ImGuiCond_Always);
    changed = false;
    //----- Render Settings -------------
    if(ImGui::Begin("Render Inspector")) {
        //----- Scene Presets --------------
        const char* preset_names[] = {"Mesh Only", "Cornell Box + Mesh", "Cornell Box"};
        if(ImGui::Combo("Scene Preset", &config.scene_preset, preset_names, 3)) {
            glUseProgram(renderer.active_id);
            glUniform1i(renderer.loc_scene_preset, config.scene_preset);
            resetAccumulation();
        }
        //----- Preview Config -------------
        if(ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            int res = config.moving_resolution;
            if(ImGui::SliderInt("Preview Res", &res, 32, 512)) {
                res = (res / 16) * 16;
                res = glm::max(32, res);
                config.moving_resolution = res;
            }
            bool vsync = renderer.v_sync == 1;
            if(ImGui::Checkbox("Enable V-Sync", &vsync)) {
                renderer.v_sync = vsync ? 1 : 0;
                glfwSwapInterval(renderer.v_sync);
            }
            ImGui::SetItemTooltip("Vertical sync\nLocks framerate at 60FPS");

            ImGui::Checkbox("Show Grid", &renderer.show_grid);
            ImGui::SetItemTooltip("Grid not visible on screenshots\nRendered on top of the path tracer");


            if(ImGui::Checkbox("BVH Heatmap", &renderer.bvh_heatmap)) {
                glUseProgram(renderer.active_id);
                glUniform1i(renderer.loc_bvh_heatmap, renderer.bvh_heatmap ? 1 : 0);
                resetAccumulation();
            }
            ImGui::SetItemTooltip("Map of bounding boxes heatmap on the mesh.\nRed = Nodes intersects by more rays.");
            if (renderer.bvh_heatmap) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(120);
                if(ImGui::SliderInt("Scale", &renderer.heatmap_scale, 5, 150)) {
                    glUseProgram(renderer.active_id);
                    glUniform1i(renderer.loc_bvh_heatmap_scale, renderer.heatmap_scale);
                    resetAccumulation();
                }
                ImGui::SetItemTooltip("Number of BVH nodes intersected per ray");
            }
        }

        if(ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            int samples = (int)renderer.MAX_SAMPLES;
            if(ImGui::InputInt("Max Samples", &samples, 1, 5000)) {
                samples = glm::max(1, samples);
                renderer.MAX_SAMPLES = (uint)samples;
            }
            ImGui::SetItemTooltip("Max progressive samples per pixel");
            changed |= ImGui::SliderInt("Depth", &config.depth, 1, 50);
            changed |= resetBtn("*##depth", config.depth, render_defaults.depth);
    
            changed |= ImGui::SliderInt("Rays/Pixel", &config.samples_per_pixel, 1, 16);
            changed |= resetBtn("*##spp", config.samples_per_pixel, render_defaults.samples_per_pixel);
    
            changed |= ImGui::Checkbox("NEE", &config.use_nee);
            ImGui::SetItemTooltip("Next Event Estimation\nDisable to compare with brute force");

            ImGui::SeparatorText("Russian Roulette");
    
            bool rr_enabled = (config.rr_min_bounces > 0);
            if(ImGui::Checkbox("Enable RR", &rr_enabled)) {
                config.rr_min_bounces = rr_enabled ? render_defaults.rr_min_bounces : 0;
                changed = true;
            }
            
            changed |= ImGui::SliderInt("Min Bounces", &config.rr_min_bounces, 0, 10);
            ImGui::SetItemTooltip("Minimum bounces before enabling Russian Roulette\n0 = off");
            changed |= resetBtn("*##rrmin", config.rr_min_bounces, render_defaults.rr_min_bounces);
    
            changed |= ImGui::SliderFloat("Survival Chance", &config.rr_max_survival, 0.1f, 1.0f, "%.2f");
            ImGui::SetItemTooltip("Probability of ray survival after each bounce");
            changed |= resetBtn("*##rrsur", config.rr_max_survival, render_defaults.rr_max_survival);
    
            ImGui::SeparatorText("Environment");
    
            const char* bg_names[] = {"Black", "White"};
            changed |= ImGui::Combo("Background", &config.background, bg_names, 2);
    
            const char* tm_names[] = {"None", "Reinhard", "ACES"};
            changed |= ImGui::Combo("Tone Map", &config.tone_mapping, tm_names, 3);

            if(changed) applyConfig();
        }
        if(ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool sun_changed = false;
            sun_changed |= ImGui::Checkbox("Enable", &config.sun_enabled);
            sun_changed |= ImGui::SliderFloat("Elevation", &config.sun_elevation, 0.0f, 90.0f, "%.1f°");
            sun_changed |= ImGui::SliderFloat("Horizontal", &config.sun_azimuth, 0.0f, 360.0f, "%.1f°");
            sun_changed |= ImGui::SliderFloat("Intensity", &config.sun_intensity, 0.0f, 20.0f, "%.1f");
            sun_changed |= ImGui::ColorEdit3("Color", value_ptr(config.sun_color));

            if(sun_changed) {
                uploadSun();
                resetAccumulation();
            }
        }
    }
    ImGui::End();

    //-- Model Loading Window -------------
    if(renderer.is_model_loading) {
        ImVec2 screen = io.DisplaySize;

        ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 100), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.80f);
        ImGui::Begin("##loading", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

        ImGui::SetWindowFontScale(1.5f);
        float text_w = ImGui::CalcTextSize("Loading Scene...").x;
        ImGui::SetCursorPosX((400 - text_w) * 0.5f);
        ImGui::Text("Loading Scene...");

        ImGui::SetWindowFontScale(1.0f);
        string model_name = filesystem::path(renderer.current_model.path).string();
        float model_name_w = ImGui::CalcTextSize(model_name.c_str()).x;
        ImGui::SetCursorPosX((400 - model_name_w) * 0.5f);
        ImGui::TextDisabled("%s", model_name.c_str());

        ImGui::End();
    }
    

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}