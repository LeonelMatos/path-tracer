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
#include "texture.hpp"
#include "denoiser.hpp"

#define VERSION "1.7.3"
#define VERSION_NOTE ""

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
Denoiser denoiser;

double mouse_last_x = 0.0, mouse_last_y = 0.0;
bool mouse_first = true;
bool mouse_captured = false;

Metrics metrics;

RenderConfig config;

#define WINDOW_TITLE "Path Tracer"

#define WINDOW_TITLE_VERSION WINDOW_TITLE " v" VERSION "  " VERSION_NOTE

static const int COMPUTE_LOCAL_X = 16;
static const int COMPUTE_LOCAL_Y = 16;

const char* txt_sep = "---------------------------------------------";


vector<GPULight> analytic_lights;
int sun_light_index = -1;

/*----------------------------------------------------------
  Function Declarations
*/
void onKeyPress(GLFWwindow* /*w*/, int key, int /*scancode*/, int action, int /*mods*/);
void onMouseMove(GLFWwindow* /*w*/, double x, double y);
void onMouseButton(GLFWwindow* w, int button, int action, int /*mods*/);
void onMouseScroll(GLFWwindow* w, double /*xoffset*/, double yoffset);
void onWindowResize(GLFWwindow* /*w*/, int width, int height);
void toggleFullscreen();
vec3 cameraForward();
void zoomToFit();
void setPreviewResolution();
void setFullResolution();
void setPreviewMode(bool enabled);
void processMovement();
void formatTime(double seconds, char*buf, int buf_size);
bool initShaders();
void reloadShaders();
void initUniforms();
void loadScene();
void resetModel();
void uploadConfig();
void applyConfig();
void uploadCamera();
void uploadCornellTopLight();
void uploadPrismDemoLight();
void uploadSun();
bool transferDataToGPU(void);
void cleanDataFromGPU();
void display(void);
void clearTextures();
void setDenoiseCheckpoints();
void createDenoisedTex(int w, int h);
void resetAccumulation();
void draw(void);
void saveScreenshot();
float computeTargetExposureEV();
void updateAutoExposure(float speed);
void drawGrid();
void drawCompositionGuides();
void syncResolutionDropdown();
void drawUI();

/*----------------------------------------------------------
  Input handle
*/
void onKeyPress(GLFWwindow* /*w*/, int key, int /*scancode*/, int action, int /*mods*/) {
    if (ImGui::GetIO().WantCaptureKeyboard) return;
    if (action != GLFW_PRESS) return;

    switch (key) {
        case GLFW_KEY_F12:
            saveScreenshot();
        break;
        case GLFW_KEY_F11:
            toggleFullscreen();
        break;
        case GLFW_KEY_F5:
            reloadShaders();
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
        case GLFW_KEY_F:
            zoomToFit();
        break;
        case GLFW_KEY_SPACE:
            setPreviewMode(false);
        break;
    }
}

void onMouseMove(GLFWwindow* /*w*/, double x, double y) {
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

void onMouseButton(GLFWwindow* w, int button, int action, int /*mods*/) {
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

///Simulate Unity's camera control speed multiplier
///Use SHIFT + Mouse Scroll to change camera speed
void onMouseScroll(GLFWwindow* w, double /*xoffset*/, double yoffset) {
    if(glfwGetKey(w, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS || glfwGetKey(w, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    camera.move_speed *= (yoffset > 0) ? 1.2f : 0.8f;
    camera.move_speed = std::clamp(camera.move_speed, 0.001f, 10.0f);
}

///\return vec3 camera forward vector from pitch and yaw 
vec3 cameraForward() {
    return normalize(vec3(cos(camera.pitch) * cos(camera.yaw), cos(camera.pitch) * sin(camera.yaw), sin(camera.pitch)));
}

///\brief Repositions the camera to frame the model on scene
///\note Uses renderer.current_bounds
///\see current_bounds_min, current_bounds_max
void zoomToFit() {
    vec3 center = (renderer.current_bounds_min + renderer.current_bounds_max) * 0.5f;
    float radius = length(renderer.current_bounds_max - renderer.current_bounds_min) * 0.5f;
    radius = glm::max(radius, 0.01f);

    float half_fov = glm::radians(config.cam_fov) * 0.5f;
    float distance = (radius / sinf(half_fov)) * 1.1f; //1.1 margin separating the model form edges

    vec3 forward_horizontal = cameraForward();
    forward_horizontal.z = 0.0f;
    if(length(forward_horizontal) < 0.001f) forward_horizontal = vec3(1.0f, 0.0f, 0.0f);
    forward_horizontal = normalize(forward_horizontal);

    camera.position = center - forward_horizontal * distance;
    camera.position.z += radius * 0.4f;

    vec3 look_dir = normalize(center - camera.position);
    camera.yaw = atan2(look_dir.y, look_dir.x);
    camera.pitch = asin(glm::clamp(look_dir.z, -1.0f, 1.0f));

    camera.lookat = camera.position + cameraForward();
    uploadCamera();
    resetAccumulation();
}

void applyResolution(int w, int h) {
    if (renderer.render_w == w && renderer.render_h == h) return;

    renderer.render_w = w;
    renderer.render_h = h;
    glUseProgram(renderer.active_id);
    glUniform2f(renderer.loc_res, (float)w, (float)h);
    if(!USE_COMPUTE_SH) {
        glBindFramebuffer(GL_FRAMEBUFFER, renderer.fbo[renderer.cur_f]);
        glViewport(0, 0, w, h);
    }
}

///Switches the render resolution to the low-res mode
void setPreviewResolution() {
    int target_w = config.moving_resolution;
    int target_h = glm::max(16, (int)(config.moving_resolution * (float)WINDOW_HEIGHT / (float)WINDOW_WIDTH));
    target_w = glm::max(16, (target_w / 16) * 16);
    target_h = glm::max(16, (target_h / 16) * 16);
    
    if(renderer.render_w == target_w && renderer.render_h == target_h) return;

    applyResolution(target_w, target_h);
    clearTextures();
    renderer.denoiser_active = false;
    renderer.cur_f  = 0;
    renderer.prev_f = 1;
}

///Switches the render resolution to full render mode
void setFullResolution() {
    if(config.lock_preview_res) return;
    
    applyResolution(WINDOW_WIDTH, WINDOW_HEIGHT);
    clearTextures();
}

///\brief Toggle Preview Mode 
///Pins the renderer at the low preview resolution and stops it from auto-upgrading to full res
///until disabling and starting a fresh render
void setPreviewMode(bool enabled) {
    config.lock_preview_res = enabled;
    if(enabled) {
        setPreviewResolution();
        renderer.frame_id = 0;
        renderer.denoiser_active = false;
    }
    else {
        setFullResolution();
        resetAccumulation();
    }
}

///\brief Checks if the user pressed the WASD keys
///\return true if any WASD key pressed
bool useMoveKeys() {
    return (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
    glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
    glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
    glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS);
}

///Process WASD movement
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
            setPreviewMode(true);
            
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
        setPreviewMode(true);
        return;
    }
}

void onWindowResize(GLFWwindow* /*w*/, int width, int height) {
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
    for(int i = 0; i < 2; i++) {
        glNamedFramebufferTexture(renderer.fbo[i], GL_COLOR_ATTACHMENT0, renderer.tex[i], 0);
    }

    createDenoisedTex(width, height);
    syncResolutionDropdown();

    if(config.lock_preview_res) {
        setPreviewResolution();
    }
    else {
        applyResolution(width, height);
    }
    resetAccumulation();
}

void toggleFullscreen() {
    if(!is_fullscreen) {
        glfwGetWindowPos(window, &windowed_x, &windowed_y);
        glfwGetWindowSize(window, &windowed_w, &windowed_h);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        is_fullscreen = true;
    }
    else {
        glfwSetWindowMonitor(window, nullptr, windowed_x, windowed_y, windowed_w, windowed_h, 0);
        is_fullscreen = false;
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

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE_VERSION, NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(renderer.v_sync);
    
    glewExperimental = GL_TRUE;
    glewInit();
    //Clear init errors
    while(glGetError() != GL_NO_ERROR) {}

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
    
    printf("%s\n%s v%s\nResolution: %dx%d", txt_sep, WINDOW_TITLE, VERSION, WINDOW_WIDTH, WINDOW_HEIGHT);
    printf("\tUsing %s shader", USE_COMPUTE_SH ? "Compute" : "Fragment");
    printf("\n\tESC   Quit\n\tF12   Screenshot render\n\tF5    Recompile shaders\n\tH/0   Center camera\n\
        R     Reset accumulation\n\tF     Zoom to fit object\n\tE     Toggle background\n%s\n", txt_sep);

    //Check GPU's SSBO size limite
    GLint max_ssbo;
    glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &max_ssbo);
    printf("[OPENGL] Max SSBO size: %.0f MB\n", max_ssbo / 1e6f);

    //Time init
    struct timespec ts, ts_start, ts_end;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    metrics.render_start_time = ts.tv_sec + ts.tv_nsec * 1e-9;

    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
        //avoids render lock when in preview rendering and reaches max samples(moving camera)
        bool is_moving = !config.lock_preview_res && (renderer.render_w != WINDOW_WIDTH);

        //Checks if needs new model loading
        //Handling of mesh, textures and env maps running in parallel
        //Each pass goes through a checkpoint checkGL to catch errors
        if(loader.upload_pending) {
            checkGL("before useProgram");
            glUseProgram(renderer.active_id);
            checkGL("useProgram");
            loader.stage = STAGE_UPLOAD_MESH;
            uploadMesh(loader.pending_tris, loader.pending_mats, renderer.triangle_ssbo, renderer.material_ssbo);
            uploadBVH(loader.pending_bvh, renderer.bvh_ssbo);

            int light_count = uploadLights(loader.pending_tris, loader.pending_mats, renderer.light_ssbo);
            analytic_lights.clear();
            uploadCornellTopLight();
            uploadPrismDemoLight();
            uploadSun();

            MeshBounds& b = loader.pending_bounds;
            glUniform1i(renderer.loc_tri_count, (int)loader.pending_tris.size());
            glUniform1i(renderer.loc_light_count, light_count);
            glUniform1i(renderer.loc_bvh_root, 0);
            glUniform3f(renderer.loc_aabb_min, b.min_bound.x, b.min_bound.y, b.min_bound.z);
            glUniform3f(renderer.loc_aabb_max, b.max_bound.x, b.max_bound.y, b.max_bound.z);
            renderer.current_bounds_min = b.min_bound;
            renderer.current_bounds_max = b.max_bound;
            checkGL("uniforms");

            loader.stage = STAGE_UPLOAD_TEXTURES;
            uploadTexture(loader.pending_cpu_mats, loader.pending_mats, renderer);
            checkGL("uploadTextures");

            glNamedBufferData(renderer.material_ssbo, loader.pending_mats.size() * sizeof(GPUMaterial), loader.pending_mats.data(), GL_STATIC_DRAW);

            loader.upload_pending = false;
            renderer.is_model_loading = false;
            resetAccumulation();
        }
        //Suspend the rendering after completion to avoid useless GPU processing
        if (renderer.MAX_SAMPLES > 0 && renderer.frame_id >= (int)renderer.MAX_SAMPLES && !is_moving) {
            clock_gettime(CLOCK_MONOTONIC, &ts_start);

            glfwSwapInterval(1);

            glfwWaitEvents(); //Gets input events and avoids program freezing
            processMovement();

            //Auto Exposure still active while suspended in Preview Mode
            //Without touching the compute dispatch (resulting texture is already there)
            //So we're touching an already-static image, old work.
            //What this is doing is reading this static image every 0.5s while in Preview, toggle on.
            static double last_auto_exposure_time = 0.0;
            if(config.auto_exposure_enabled && config.lock_preview_res) {
                double now = glfwGetTime();
                if (now - last_auto_exposure_time > 0.5) {
                    updateAutoExposure(config.auto_exposure_speed);
                    last_auto_exposure_time = now;
                }
            }

            display();
            drawGrid();
            drawUI();
            glfwSwapBuffers(window);
            glfwPollEvents();

            clock_gettime(CLOCK_MONOTONIC, &ts_end);
            metrics.last_frame_time = (ts_end.tv_sec - ts_start.tv_sec) + (ts_end.tv_nsec - ts_start.tv_nsec) * 1e-9;
            continue;
        }
        glfwSwapInterval(renderer.v_sync);
        draw();
    }
    cleanDataFromGPU();
    glfwTerminate();

    return 0;
}

///Formats a duration in seconds as "Xs" or "XmYs"
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

///Compiles and links all shaders 
///\return true on success
bool initShaders() {
    bool ok = true;

    ok &= renderer.display_id.load({
        { GL_VERTEX_SHADER,   "shaders/common.vert"    },
        { GL_FRAGMENT_SHADER, "shaders/display.frag" },
    });
    ok &= renderer.pathtr_frag_id.load({
        { GL_VERTEX_SHADER,   "shaders/common.vert"       },
        { GL_FRAGMENT_SHADER, "shaders/path_trace.frag" },
    });
    ok &= renderer.pathtr_comp_id.load({
        { GL_COMPUTE_SHADER, "shaders/path_trace.comp" }
    });
    ok &= renderer.grid_id.load({
        { GL_VERTEX_SHADER, "shaders/grid.vert" },
        { GL_FRAGMENT_SHADER, "shaders/grid.frag" }
    });
    return ok;
}

void reloadShaders() {
    printf("\n[SHADERS] Reloading shaders...\n");
    bool ok = initShaders();

    renderer.active_id = USE_COMPUTE_SH ? renderer.pathtr_comp_id.id() : renderer.pathtr_frag_id.id();
    initUniforms();

    uploadConfig();
    uploadCamera();
    uploadSun();
    resetAccumulation();

    printf(ok ? "[SHADERS] Reload OK\n" : "[SHADERS] Reload FAILED for one or more programs.\n\
        Kept the previous working version\n");
}

///\brief Caches every uniform location of the active shader into Renderer
void initUniforms() {
    GLuint active = renderer.active_id;

    renderer.loc_res = glGetUniformLocation(active, "resolution");
    renderer.loc_frame = glGetUniformLocation(active, "frame_id");
    renderer.loc_tile_offset = glGetUniformLocation(active, "tile_offset");
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
        renderer.loc_prev = glGetUniformLocation(renderer.pathtr_frag_id.id(), "prev_frame");
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

    renderer.loc_lateral_ca = glGetUniformLocation(active, "CAM_LATERAL_CA");
    renderer.loc_axial_ca = glGetUniformLocation(active, "CAM_AXIAL_CA");
    renderer.loc_aperture_blades = glGetUniformLocation(active, "CAM_APERTURE_BLADES");
    renderer.loc_blade_rotation = glGetUniformLocation(active, "CAM_BLADE_ROTATION");
    renderer.loc_anamorphic_squeeze = glGetUniformLocation(active, "CAM_ANAMORPHIC_SQUEEZE");
    renderer.loc_cateye_strength = glGetUniformLocation(active, "CAM_CATEYE_STRENGTH");
    renderer.loc_distortion_k1 = glGetUniformLocation(active, "CAM_DISTORTION_K1");
    renderer.loc_distortion_k2 = glGetUniformLocation(active, "CAM_DISTORTION_K2");
    renderer.loc_projection_mode = glGetUniformLocation(active, "CAM_PROJECTION_MODE");
    renderer.loc_cam_tilt = glGetUniformLocation(active, "CAM_TILT");

    renderer.loc_glass_dispersion = glGetUniformLocation(active, "GLASS_DISPERSION");

    renderer.loc_cam_pos    = glGetUniformLocation(active, "camera_position");
    renderer.loc_cam_lookat = glGetUniformLocation(active, "camera_lookat");
    renderer.loc_cam_up = glGetUniformLocation(active, "camera_up");

    renderer.loc_display_render_res = glGetUniformLocation(renderer.display_id.id(), "render_resolution");
    renderer.loc_display_res = glGetUniformLocation(renderer.display_id.id(), "display_resolution");
    renderer.loc_display_cam_fov = glGetUniformLocation(renderer.display_id.id(), "CAM_FOV");
    
    renderer.loc_display_tone_map = glGetUniformLocation(renderer.display_id.id(), "TONE_MAPPING");
    renderer.loc_vignette = glGetUniformLocation(renderer.display_id.id(), "VIGNETTE_STRENGTH");
    renderer.loc_exposure = glGetUniformLocation(renderer.display_id.id(), "EXPOSURE");

    renderer.loc_use_nee = glGetUniformLocation(active, "USE_NEE");

    renderer.loc_firefly_clamp = glGetUniformLocation(active, "FIREFLY_CLAMP");

    renderer.grid_loc_view = glGetUniformLocation(renderer.grid_id.id(), "view");
    renderer.grid_loc_proj = glGetUniformLocation(renderer.grid_id.id(), "projection");
    renderer.grid_loc_near = glGetUniformLocation(renderer.grid_id.id(), "near_plane");
    renderer.grid_loc_far = glGetUniformLocation(renderer.grid_id.id(), "far_plane");
    renderer.grid_loc_cam_pos = glGetUniformLocation(renderer.grid_id.id(), "camera_pos");

    renderer.loc_scene_preset = glGetUniformLocation(renderer.active_id, "SCENE_PRESET");

    renderer.loc_env_map = glGetUniformLocation(active, "env_map");
    renderer.loc_use_env_map = glGetUniformLocation(active, "USE_ENV_MAP");

    renderer.loc_tex_array = glGetUniformLocation(active, "tex_albedo");
    renderer.loc_use_textures = glGetUniformLocation(active, "USE_TEXTURES");

    renderer.loc_force_material = glGetUniformLocation(active, "FORCE_MATERIAL");   

    renderer.loc_use_ground_plane = glGetUniformLocation(active, "USE_GROUND_PLANE");
    renderer.loc_ground_elevation = glGetUniformLocation(active, "GROUND_ELEVATION");
    renderer.loc_ground_albedo = glGetUniformLocation(active, "GROUND_ALBEDO");
    renderer.loc_ground_radius = glGetUniformLocation(active, "GROUND_RADIUS");
    renderer.loc_ground_shadow_catcher = glGetUniformLocation(active, "GROUND_SHADOW_CATCHER");
    renderer.loc_ground_shadow_opacity = glGetUniformLocation(active, "GROUND_SHADOW_OPACITY");
}

///\brief Updates every RenderConfig parameter, called when any changed
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
    glUniform1f(renderer.loc_lateral_ca, config.cam_lateral_ca);
    glUniform1f(renderer.loc_axial_ca, config.cam_axial_ca);
    glUniform1i(renderer.loc_aperture_blades, config.cam_aperture_blades);
    glUniform1f(renderer.loc_blade_rotation, glm::radians(config.cam_blade_rotation));
    glUniform1f(renderer.loc_anamorphic_squeeze, config.cam_anamorphic_squeeze);
    glUniform1f(renderer.loc_cateye_strength, config.cam_cateye_strength);
    glUniform1f(renderer.loc_distortion_k1, config.cam_distortion_k1);
    glUniform1f(renderer.loc_distortion_k2, config.cam_distortion_k2);
    glUniform1i(renderer.loc_projection_mode, config.cam_projection_mode);
    glUniform1f(renderer.loc_cam_tilt, glm::radians(config.cam_tilt));
    glUniform1f(renderer.loc_glass_dispersion, config.glass_dispersion);
    glUniform1i(renderer.loc_use_nee, config.use_nee ? 1 : 0);
    glUniform1i(renderer.loc_scene_preset, config.scene_preset);
    glUniform1f(renderer.loc_firefly_clamp, config.firefly_clamp);
    glUniform1i(renderer.loc_use_textures, config.use_textures ? 1 : 0);
    glUniform1i(renderer.loc_use_ground_plane, config.use_ground_plane ? 1 : 0);
    glUniform1f(renderer.loc_ground_elevation, config.ground_elevation);
    glUniform1f(renderer.loc_ground_albedo, config.ground_albedo);
    glUniform1f(renderer.loc_ground_radius, config.ground_radius);
    glUniform1i(renderer.loc_ground_shadow_catcher, config.ground_shadow_catcher ? 1 : 0);
    glUniform1f(renderer.loc_ground_shadow_opacity, config.ground_shadow_opacity);
}

///\brief Calls uploadConfig and resets the scene
void applyConfig() {
    uploadConfig();
    resetAccumulation();
    printf("\n[RENDERER] Config applied, resetting accumulation\n");
}

///\brief Copies a LensPreset into config and applies it
///\see LENS_PRESETS, applyConfig
void applyLensPreset(int idx) {
    const LensPreset& p = lensPresetAt(idx);
    config.cam_fov = p.fov;
    config.cam_aperture = p.aperture;
    config.cam_aperture_blades = p.aperture_blades;
    config.cam_blade_rotation = p.blade_rotation;
    config.cam_anamorphic_squeeze = p.anamorphic_squeeze;
    config.cam_cateye_strength = p.cateye_strength;
    config.cam_distortion_k1 = p.distortion_k1;
    config.cam_distortion_k2 = p.distortion_k2;
    config.cam_projection_mode = p.projection_mode;
    config.cam_tilt = p.tilt;
    config.cam_lateral_ca = p.lateral_ca;
    config.cam_axial_ca = p.axial_ca;
    applyConfig();
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
        sun.radius = glm::radians(config.sun_angular_radius);

        sun_light_index = (int)analytic_lights.size();
        analytic_lights.push_back(sun);
    }
    uploadAnalyticLights(analytic_lights, renderer.analytic_light_ssbo);
    glUniform1i(renderer.loc_analytic_light_count, (int)analytic_lights.size());
}

///Brute-forced way to set the Cornell's top light to follow
///the light types emissions, since now plain geometry aren't considered LIGHT types
void uploadCornellTopLight() {
    analytic_lights.clear();

    if(config.scene_preset == 1 || config.scene_preset == 2) {
        GPULight cornell_light{};
        cornell_light.position = vec4(0.0f, 0.0f, 0.9f, 0.0f);
        cornell_light.emission = vec4(5.0f, 5.0f, 5.0f, 0.0f);
        cornell_light.type = LIGHT_POINT;
        cornell_light.radius = 0.5f;
        analytic_lights.push_back(cornell_light);
    }
}

void uploadPrismDemoLight() {
    if(config.scene_preset != 3) return;
    analytic_lights.clear();

    GPULight prism_light{};
    prism_light.position = vec4(5.0f, 0.0f, 2.0f, 0.0f);
    prism_light.direction = vec4(normalize(vec3(-3.475f, 0.0f, -2.0125f)), 0.0f);
    prism_light.emission = vec4(150.0f, 150.0f, 150.0f, 0.0f);
    prism_light.type = LIGHT_SPOT;
    prism_light.spot_inner = cos(radians(25.0f));
    prism_light.spot_outer = cos(radians(35.0f));
    analytic_lights.push_back(prism_light);
}

///\brief Recursively lists supported model files under the /models directory
///\param models_dir Directory to scan
///\return Sorted list of file paths
vector<string> scanModels(const string& models_dir) {
    vector<string> paths;

    if(!filesystem::exists(models_dir)) {
        printf("\nModels directory not found: %s\n", models_dir.c_str());
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

///\brief Recursively lists HDRI files under the /hdri directory
/// @param hdri_dir Directory to scan
/// @return Sorted list of file paths
vector<string> scanHDRI(const string& hdri_dir) {
    vector<string> paths;

    if(!filesystem::exists(hdri_dir)) {
        printf("\nHDRI directory not found: %s\n", hdri_dir.c_str());
        return paths;
    }

    for(auto& entry : filesystem::recursive_directory_iterator(hdri_dir)) {
        string ext = entry.path().extension().string();
        if(ext == ".hdr" || ext == ".exr")
            paths.push_back(entry.path().string());
    }
    sort(paths.begin(), paths.end());
    return paths;
}

///\brief Loads the scene and imports the model on a secondary thread
void loadScene() {
    //Empty scene -> reset counters
    if(renderer.current_model.path.empty()) {
        clearMesh();
        glUseProgram(renderer.active_id);
        glUniform1i(renderer.loc_tri_count, 0);
        glUniform1i(renderer.loc_light_count, 0);
        glUniform1i(renderer.loc_bvh_root, 0);
        glUniform3f(renderer.loc_aabb_min, 0.0f, 0.0f, 0.0f);
        glUniform3f(renderer.loc_aabb_max, 0.0f, 0.0f, 0.0f);
        resetAccumulation();
        return;
    }
    renderer.is_model_loading = true;
    loader.stage = STAGE_PARSING;
    //keeps the program responsive while loading models
    setPreviewMode(true);

    std::thread([]{
        vector<GPUTriangle> tris; vector<GPUMaterial> mats; vector<CPUMaterial> cpu_mats;
        SceneModel& model = renderer.current_model;
        MeshBounds bounds;

        
        //Hardcoded model position better facing the camera
        mat4 transform = translate(mat4(1.0f), model.position);
        transform = translate(transform, vec3(0, 0, -1.0f));
        transform = rotate(transform, radians(model.rotation.x + 90.0f), vec3(1,0,0));
        transform = rotate(transform, radians(model.rotation.y + 180.0f), vec3(0,1,0));
        transform = rotate(transform, radians(model.rotation.z), vec3(0,0,1));
        transform = scale(transform, model.scale);
        
        if (!loadMesh(model.path, tris, mats, cpu_mats, transform, &bounds)) {
            renderer.is_model_loading = false;
            return;
        }
        
        loader.stage = STAGE_BVH;
        vector<BVHNode> bvh_nodes;
        buildBVH(tris, bvh_nodes);
        
        model.tri_count = (int)tris.size();
        model.mat_count = (int)mats.size();

        loader.pending_tris = tris;
        loader.pending_mats = mats;
        loader.pending_cpu_mats = cpu_mats;
        loader.pending_bvh  = bvh_nodes;
        loader.pending_bounds = bounds;
        loader.upload_pending = true;
        //is_model_loading stays true after ending this thread
        //the main thread ends it
    }).detach();
}

void resetModel() {
    renderer.current_model.path = "";
    renderer.current_model.position = vec3(0.0f);
    renderer.current_model.rotation = vec3(0.0f);
    renderer.current_model.scale = vec3(1.0f);
    loadScene();
    resetAccumulation();
}

///\brief First time GPU setup
///\return true on success
bool transferDataToGPU(void) {
    if(!initShaders()) {
        fprintf(stderr, "[FATAL] Shader compilation failed: see log above\n");
        return false;
    }

    //Select shader
    renderer.active_id = USE_COMPUTE_SH ? renderer.pathtr_comp_id.id() : renderer.pathtr_frag_id.id();
    
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

    //Denoised
    createDenoisedTex(renderer.render_w, renderer.render_h);

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
    if(renderer.denoised_tex)
        glDeleteTextures(1, &renderer.denoised_tex);

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
    renderer.display_id.use();

    glUniform2f(renderer.loc_display_render_res, (float)renderer.render_w, (float)renderer.render_h);
    glUniform2f(renderer.loc_display_res, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
    glUniform1i(renderer.loc_display_tone_map, config.tone_mapping);
    glUniform1f(renderer.loc_vignette, config.vignette_strength);
    glUniform1f(renderer.loc_exposure, glm::exp2(config.exposure_ev));
    glUniform1f(renderer.loc_display_cam_fov, glm::radians(config.cam_fov));

    GLuint tex_to_show = (renderer.denoiser_active && renderer.denoised_tex && renderer.frame_id > 10) ? renderer.denoised_tex : renderer.tex[renderer.cur_f];

    glBindTextureUnit(0, tex_to_show);
    glUniform1i(renderer.loc_tex, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

}

/*----------------------------------------------------------
  Denoiser
*/

///\brief Computes at which samples the denoiser should run
void setDenoiseCheckpoints() {
    renderer.denoise_checkpoints.clear();
    renderer.next_denoise_idx = 0;
    renderer.denoiser_active = false;

    int n = (int)renderer.MAX_SAMPLES;
    if (n <= 32)
        renderer.denoise_checkpoints = {n};
    else if (n <= 128)
        renderer.denoise_checkpoints = {n * 3/4, n};
    else
        renderer.denoise_checkpoints = { n/2, n * 3/4, n};
}

///Allocates the denoised output texture at a width w and height h
void createDenoisedTex(int w, int h) {
    if(renderer.denoised_tex)
        glDeleteTextures(1, &renderer.denoised_tex);
    glCreateTextures(GL_TEXTURE_2D, 1, &renderer.denoised_tex);
    glTextureStorage2D(renderer.denoised_tex, 1, GL_RGBA32F, w, h);
    glTextureParameteri(renderer.denoised_tex, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(renderer.denoised_tex, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(renderer.denoised_tex, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(renderer.denoised_tex, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

///\brief Reads back the accumulation texture, runs OIDN and sends the result
/// @param checkpoint_num index of checkpoint
/// @param total Total number of checkpoints
void runDenoiser(int checkpoint_num, int total) {
    int w = renderer.render_w, h = renderer.render_h;

    if(!denoiser.initialized || denoiser.width != w || denoiser.height != h)
        denoiser.init(w, h);
    
    std::vector<float> rgba(w * h * 4);
    glGetTextureImage(renderer.tex[renderer.cur_f], 0, GL_RGBA, GL_FLOAT, rgba.size() * sizeof(float), rgba.data());

    //revert RGBA to RGB before running denoiser
    for (int i = 0; i < w * h; i++) {
        denoiser.input_buf[i*3 +0] = rgba[i*4 +0];
        denoiser.input_buf[i*3 +1] = rgba[i*4 +1];
        denoiser.input_buf[i*3 +2] = rgba[i*4 +2];
    }
    denoiser.run();

    //revert RGB to RGBA to upload on OpenGL tex
    std::vector<float> out_rgba(w*h *4);
    for(int i = 0; i < w*h; i++) {
        out_rgba[i*4 +0] = denoiser.output_buf[i*3 +0];
        out_rgba[i*4 +1] = denoiser.output_buf[i*3 +1];
        out_rgba[i*4 +2] = denoiser.output_buf[i*3 +2];
        out_rgba[i*4 +3] = 1.0f;
    }
    glTextureSubImage2D(renderer.denoised_tex, 0, 0, 0, w, h, GL_RGBA, GL_FLOAT, out_rgba.data());
    renderer.denoiser_active = true;

    char buf[64];
    snprintf(buf, sizeof(buf), "Denoised %dx%d", checkpoint_num, total);
    renderer.denoise_status_msg = buf;
    renderer.denoise_msg_time = glfwGetTime();

    printf("\n[DENOISER] %s at sample %d\n", buf, renderer.frame_id);
}
//----------------------------------------------------------

void clearTextures() {
    if(!renderer.tex[0] || !renderer.tex[1]) return;
    float zero[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    glClearTexImage(renderer.tex[0], 0, GL_RGBA, GL_FLOAT, zero);
    glClearTexImage(renderer.tex[1], 0, GL_RGBA, GL_FLOAT, zero);
}

///\brief Restarts progressive accumulation back to 0
void resetAccumulation() {
    renderer.frame_id = 0;
    renderer.cur_f = 0;
    renderer.prev_f = 1;
    renderer.denoiser_active = false;
    clearTextures();
    setDenoiseCheckpoints();

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

///\brief Renders one full frame in the order: path trace, denoise, autoEV, display, grid, UI
void draw(void) {
    struct timespec ts_now;
    double time_now, time_elapsed = 0.0;

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
        int groups_x = (renderer.render_w + COMPUTE_LOCAL_X - 1) / COMPUTE_LOCAL_X;
        int total_groups_y = (renderer.render_h + COMPUTE_LOCAL_Y - 1) / COMPUTE_LOCAL_Y;

        //dispatch in vertical tiles with sync between each other; avoid watchdog timeout crash on slower GPUs
        if(config.tile_dispatch) {
            for(int gy = 0; gy < total_groups_y; gy += config.tile_rows) {
                int rows = std::min(config.tile_rows, total_groups_y - gy);
                glUniform2i(renderer.loc_tile_offset, 0, gy * COMPUTE_LOCAL_Y);
                glDispatchCompute(groups_x, rows, 1);
                glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
                glFinish();
            }
        }
        else {
            glUniform2i(renderer.loc_tile_offset, 0, 0);
            glDispatchCompute(groups_x, total_groups_y, 1);
            glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
        }
        
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

    //Denoiser phase (skipped in Preview Mode)
    if(!config.lock_preview_res && renderer.denoiser_enabled && renderer.next_denoise_idx < (int)renderer.denoise_checkpoints.size() &&
        renderer.frame_id == renderer.denoise_checkpoints[renderer.next_denoise_idx]) {
            int idx = renderer.next_denoise_idx +1;
            int total = (int)renderer.denoise_checkpoints.size();
            runDenoiser(idx, total);
            renderer.next_denoise_idx++;
        }

    //Auto Exposure: periodic, not every frame, runs in preview too
    if(config.auto_exposure_enabled && renderer.frame_id > 0 && 
        (config.lock_preview_res || renderer.frame_id % 8 == 0))
        updateAutoExposure(config.auto_exposure_speed);

    if(renderer.frame_id == (int)renderer.MAX_SAMPLES) {
        clock_gettime(CLOCK_MONOTONIC, &ts_now);
        time_now = ts_now.tv_sec + ts_now.tv_nsec * 1e-9;
        time_elapsed = time_now - metrics.render_start_time;
        printf("\n%s\n|Render complete| %d samples in %.01fs\n%s\n", txt_sep, renderer.frame_id, time_elapsed, txt_sep);
    }
    
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

        printf("\r\tSample Count: %d | FPS: %.1f : %.1fms/frame | %.1fMS/s | Time:%s",
            renderer.frame_id, metrics.fps, metrics.ms_frame, metrics.samples_per_s / 1e6, metrics.time_buf);
        fflush(stdout);
    }
}

/*----------------------------------------------------------
  Render Extras
*/

///\brief Mirror of display.frag's per-pixel pipeline
///exposure → tone map + gamma → vignette → composite
///\param linear_color raw HDR color sampled from render texture
///\param alpha render texture's alpha channel
///\param uv pixel-center position in the frame[0,1]
///\return vec3 pixel with all pipeline applied
///\see toneMapCPU, computeAspectCropRect, saveScreenshot, display.frag
inline glm::vec3 displayPixelCPU(glm::vec3 linear_color, float alpha, glm::vec2 uv, float exposure_ev,
int tone_mapping, float vignette_strength) {
    linear_color *= glm::exp2(exposure_ev);
    glm::vec3 mapped = glm::pow(toneMapCPU(linear_color, tone_mapping), glm::vec3(1.0f/2.2f));
    
    if(vignette_strength > 0.0f) {
        glm::vec2 centered = uv - 0.5f;
        float dist = glm::length(centered) * 1.4142135f;
        float vignette = 1.0f - vignette_strength * dist * dist;
        mapped *= glm::clamp(vignette, 0.0f, 1.0f);
    }
    glm::vec3 bg(1.0f);
    return glm::mix(bg, mapped, alpha);
}

inline void computeAspectCropRect(int frame_w, int frame_h, int aspect_frame,
int& out_x0, int& out_y0, int& out_w, int& out_h) {
    out_x0 = 0; out_y0 = 0; out_w = frame_w; out_h = frame_h;
    if(aspect_frame <= 0 || aspect_frame >= ASPECT_RATIO_COUNT) return;

    float target_ratio = ASPECT_RATIOS[aspect_frame].ratio;
    float frame_ratio = (float)frame_w / (float)frame_h;

    if (target_ratio > frame_ratio) {
        out_w = frame_w;
        out_h = (int)(frame_w / target_ratio);
    }
    else {
        out_h = frame_h;
        out_w = (int)(frame_h * target_ratio);
    }
    out_x0 = (frame_w - out_w) / 2;
    out_y0 = (frame_h - out_h) / 2;
}

///\brief Takes a screenshot of the render
void saveScreenshot() {
    time_t now = time(nullptr);
    struct tm* t = localtime(&now);

    char filename[64];
    snprintf(filename, sizeof(filename), "render_%04d%02d%02d_%02d%02d%02d_%ds.png",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec, renderer.frame_id);

    GLuint src_tex = (renderer.denoiser_active && renderer.denoised_tex && renderer.frame_id > 10) ? renderer.denoised_tex : renderer.tex[renderer.cur_f];

    const int w = renderer.render_w;
    const int h = renderer.render_h;

    vector<float> pixels_float(w * h * 4);
    glGetTextureImage(src_tex, 0, GL_RGBA, GL_FLOAT, pixels_float.size() * sizeof(float), pixels_float.data());

    int crop_x0, crop_y0, crop_w, crop_h;
    computeAspectCropRect(w, h, renderer.aspect_frame, crop_x0, crop_y0, crop_w, crop_h);

    vector<unsigned char> pixels(crop_w * crop_h * 3);

    for (int cy = 0; cy < crop_h; cy++) {
        for (int cx = 0; cx < crop_w; cx++) {
            int x = crop_x0 + cx;
            int y = crop_y0 + cy;
            int i = y * w + x;

            vec2 uv = (vec2((float)x, (float)y) + 0.5f) / vec2((float)w, (float)h);

            vec3 linear_color(pixels_float[i*4+0], pixels_float[i*4+1], pixels_float[i*4+2]);
            float alpha = pixels_float[i*4+3];

            vec3 composited = displayPixelCPU(linear_color, alpha, uv,
                config.exposure_ev, config.tone_mapping, config.vignette_strength);
            
            int out_i = cy * crop_w + cx;
            pixels[out_i*3+0] = (unsigned char)(composited.r * 255.0f);
            pixels[out_i*3+1] = (unsigned char)(composited.g * 255.0f);
            pixels[out_i*3+2] = (unsigned char)(composited.b * 255.0f);
        }
    }

    //flip y
    for (int y = 0; y < crop_h / 2; y++) {
        int y2 = crop_h - 1 - y;
        for (int x = 0; x < crop_w * 3; x++)
            swap(pixels[y * crop_w * 3 + x], pixels[y2 * crop_w * 3 + x]);
    }

    stbi_write_png(filename, crop_w, crop_h, 3, pixels.data(), crop_w * 3);
    screenshot_msg = string("Saved ") + filename;
    screenshot_msg_time = glfwGetTime();
    printf("\n[SCREENSHOT] Saved %s\n", filename);
}

float computeTargetExposureEV() {
    GLuint src_tex = (renderer.denoiser_active && renderer.denoised_tex && renderer.frame_id > 10) ?
        renderer.denoised_tex : renderer.tex[renderer.cur_f];

    const int w = renderer.render_w;
    const int h = renderer.render_h;

    vector<float> pixels_float(w * h * 4);
    glGetTextureImage(src_tex, 0, GL_RGBA, GL_FLOAT, pixels_float.size() * sizeof(float), pixels_float.data());

    double log_sum = 0.0;
    double log_sum_sq = 0.0;
    int counted = 0;

    const int target_samples = 4096;
    ///subsampling area, defines an estimate average for performance
    ///Not a fixed number because different resolutions might not apply well
    ///So adaptive stride looks for a constant sample regardless of resolution or Render Mode
    const int stride = glm::max(1, (int)glm::sqrt((double)(w * h) / target_samples));

    ///pixels below this are excluded
    const float min_lum = 1e-3f;

    for (int y = 0; y < h; y+= stride) {
        for (int x = 0; x < w; x += stride) {
            int i = (y * w + x) * 4;
            float lum = 0.2126f * pixels_float[i+0] + 0.7152f * pixels_float[i+1] + 0.0722f * pixels_float[i+2];
            if(glm::isnan(lum) || glm::isinf(lum) || lum <= min_lum) continue;
            double log_lum = glm::log(double(lum));
            log_sum += log_lum;
            log_sum_sq += log_lum * log_lum;
            counted++;
        }
    }

    if (counted == 0) return config.exposure_ev;

    double mean_log = log_sum / counted;

    //Fixing Flat-scene guard for nearly uniform sampled luminance
    double variance = (log_sum_sq / counted) - (mean_log * mean_log);
    const double min_variance = 0.03;
    if(variance < min_variance || glm::isnan(variance) || glm::isinf(variance)) return config.exposure_ev;

    double log_avg = glm::exp(mean_log);
    ///standart photographic middle-gray target, 18%
    const double target_luminance = 0.18;

    float target_ev = (float)glm::log2(target_luminance / glm::max(log_avg, 1e-4));
    target_ev = glm::clamp(target_ev, -8.0f, 8.0f);
    return (glm::isnan(target_ev) || glm::isinf(target_ev)) ? config.exposure_ev : target_ev;
}

///Handler for computeTargetExposureEV, gradually changes EV value based on speed
/// @param speed How quick the EV change applies, 0 = frozen, 1 = instant, 0-1 = gradual
void updateAutoExposure(float speed) {
    float target = computeTargetExposureEV();
    if(glm::isnan(target) || glm::isinf(target)) return;
    config.exposure_ev = glm::mix(config.exposure_ev, target, glm::clamp(speed, 0.0f, 1.0f));
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

    renderer.grid_id.use();
    glUniformMatrix4fv(renderer.grid_loc_view, 1, GL_FALSE, value_ptr(view));
    glUniformMatrix4fv(renderer.grid_loc_proj, 1, GL_FALSE, value_ptr(proj));
    glUniform1f(renderer.grid_loc_near, 0.01f);
    glUniform1f(renderer.grid_loc_far, 100.0f);
    glUniform3f(renderer.grid_loc_cam_pos, camera.position.x, camera.position.y, camera.position.z);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisable(GL_BLEND);
    //glEnable(GL_DEPTH_TEST);
}

///\brief Draws a line that shows clearly in dark and light backgrounds
void addContrastLine(ImDrawList* dl, ImVec2 p1, ImVec2 p2, float thickness) {
    dl->AddLine(p1, p2, IM_COL32(0, 0, 0, 160), thickness + 1.5f);
    dl->AddLine(p1, p2, IM_COL32(255, 255, 255, 210), thickness);
}

///\brief Draws photography-style framing guides and an aspect-ratio crop preview mask over the viewport
///Doesn't touch the shader, render or camera, only a viewfinder to position the camera
///\note Drawn on ImGui's background draw list, so it stays on top ot the render and beneath the Scene Editor window 
void drawCompositionGuides() {
    if (renderer.composition_guide == 0 && renderer.aspect_frame == 0) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    const float W = (float)WINDOW_WIDTH, H = (float)WINDOW_HEIGHT;
    //const ImU32 line_col = IM_COL32(255, 255, 255, 200);
    const float thickness = 5.0f;

    switch(renderer.composition_guide) {
        //Rule of thirds
        case 1: {
            for (int i = 1; i <= 2; i++) {
                float x = W * i / 3.0f;
                float y = H * i / 3.0f;
                addContrastLine(dl, ImVec2(x, 0), ImVec2(x, H), thickness);
                addContrastLine(dl, ImVec2(0, y), ImVec2(W, y), thickness);
            }
            break;
        }
        //Golden Ratio
        case 2: {
            const float inv_phi = 0.6180339887f;
            float x0 = W * (1.0f - inv_phi), x1 = W * inv_phi;
            float y0 = H * (1.0f - inv_phi), y1 = H * inv_phi;
            addContrastLine(dl, ImVec2(x0, 0), ImVec2(x0, H), thickness);
            addContrastLine(dl, ImVec2(x1, 0), ImVec2(x1, H), thickness);
            addContrastLine(dl, ImVec2(0, y0), ImVec2(W, y0), thickness);
            addContrastLine(dl, ImVec2(0, y1), ImVec2(W, y1), thickness);
            break;
        }
        //Center Cross
        case 3: {
            addContrastLine(dl, ImVec2(W * 0.5f, 0), ImVec2(W * 0.5f, H), thickness);
            addContrastLine(dl, ImVec2(0, H * 0.5f), ImVec2(W, H * 0.5f), thickness);
            break;
        }
        //Diagonal
        case 4: {
            addContrastLine(dl, ImVec2(0, 0), ImVec2(W, H), thickness);
            addContrastLine(dl, ImVec2(W, 0), ImVec2(0, H), thickness);
            break;
        }
    }

    //Aspect-ratio crop
    if(renderer.aspect_frame > 0 && renderer.aspect_frame < ASPECT_RATIO_COUNT) {
        float target_ratio = ASPECT_RATIOS[renderer.aspect_frame].ratio;
        float window_ratio = W / H;

        float crop_w, crop_h;
        if(target_ratio > window_ratio) { //crop wider than window
            crop_w = W;
            crop_h = W / target_ratio;
        }
        else { //crop narrower than window
            crop_h = H;
            crop_w = H * target_ratio;
        }
        float x0 = (W - crop_w) * 0.5f;
        float y0 = (H - crop_h) * 0.5f;

        const ImU32 mask_col = IM_COL32(0, 0, 0, 240);
        if(y0 > 0.5f) {
            dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, y0), mask_col);
            dl->AddRectFilled(ImVec2(0, y0 + crop_h), ImVec2(W, H), mask_col);
        }
        if(x0 > 0.5f) {
            dl->AddRectFilled(ImVec2(0, 0), ImVec2(x0, H), mask_col);
            dl->AddRectFilled(ImVec2(x0 + crop_w, 0), ImVec2(W, H), mask_col);
        }
        dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + crop_w, y0 + crop_h), IM_COL32(255, 255, 255, 180), 0.0f, 0, 1.5f);
    }
}

/*----------------------------------------------------------
  UI Draw
*/
static vector<string> model_list;
static bool model_list_loaded = false;

static vector<string> hdri_list;
static bool hdri_list_loaded = false;
static bool show_hdri_selector = false;
static bool show_lens_studio = false;

inline int current_res_idx = 0;
const int res_w[] = {1280, 1920, 2560, 4096, 0, 0};
const int res_h[] = {720, 1080, 1440, 2160, 0, 0};

///\brief Syncs the UI to the current window size
void syncResolutionDropdown() {
    if(is_fullscreen) {
        current_res_idx = 4;
        return;
    }
    current_res_idx = 5;
    for(int i = 0; i < 4; i++) {
        if(WINDOW_WIDTH == res_w[i] && WINDOW_HEIGHT == res_h[i]) {
            current_res_idx = i;
            break;
        }
    }
}

void drawUI() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    drawCompositionGuides();

    ImGuiIO& io = ImGui::GetIO();
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    RenderConfig render_defaults;
    CameraConfig camera_defaults;
    bool changed = false;

    auto resetBtn = [&](const char* id, auto& field, auto default_val) -> bool {
                ImGui::SameLine();
                bool button = ImGui::SmallButton(id);
                ImGui::SetItemTooltip("Reset to default");
                if(button && field != default_val) { field = default_val; return true; }
                return false;
            };

    //-----------------------------
    //-- Left Window --------------
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

            ImGui::Separator();
            if(config.lock_preview_res) {
                ImGui::TextColored(ImVec4(0.40f, 0.80f, 1.0f, 1.0f), "PREVIEW MODE");
                ImGui::TextWrapped("Interact freely: render is in Preview Mode");
                if(ImGui::Button("Full Render [SPACE]", ImVec2(-1, 0)))
                    setPreviewMode(false);
            }
            else {
                bool converged = renderer.MAX_SAMPLES > 0 && renderer.frame_id >= (int)renderer.MAX_SAMPLES;
                if(converged)
                    ImGui::TextColored(ImVec4(0.55f, 1.0f, 0.55f, 1.0f), "Render Complete");
                else {
                    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Rendering...");
                    float progress = renderer.MAX_SAMPLES > 0 ? (float)renderer.frame_id / (float)renderer.MAX_SAMPLES : 0.0f;
                    ImGui::ProgressBar(progress, ImVec2(-1, 0));
                }
                if(ImGui::Button("Return to Preview", ImVec2(-1, 0)))
                    setPreviewMode(true);
            }
            ImGui::Separator();
            
            if (!screenshot_msg.empty()) {
                double time_elapsed = glfwGetTime() - screenshot_msg_time;
                if(time_elapsed < 4.0) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", screenshot_msg.c_str());
                }
                else screenshot_msg = "";
            }
        }

        //----- Window -------------
        if(ImGui::CollapsingHeader("Window")) {
            const char* res_names[] = {
                "1280x720", "1920x1080", "2560x1440", "4096x2160", "Fullscreen", "Custom"
            };
            if(ImGui::Combo("Resolution", &current_res_idx, res_names, 6)) {
                //Fullsreen
                if(current_res_idx == 4)
                    toggleFullscreen();
                //Window
                else if (current_res_idx < 4) {
                    if(is_fullscreen)
                        toggleFullscreen();
                    glfwSetWindowMonitor(window, nullptr, 100, 100, res_w[current_res_idx], res_h[current_res_idx], 0);
                }
            }
            if(current_res_idx == 5)
                ImGui::TextDisabled("%dx%d", WINDOW_WIDTH, WINDOW_HEIGHT);
        }


        //----- Camera -------------
        if(ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            ImGui::Text("x = %.2f, y = %.2f, z = %.2f", camera.position.x, camera.position.y, camera.position.z);

            float fov_max = camFovCap(config.cam_projection_mode);
            changed |= ImGui::SliderFloat("FOV", &config.cam_fov, 10.0f, fov_max, "%.1f°");
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

            ImGui::Spacing();
            if(ImGui::Button("Lens Studio", ImVec2(-1, 0))) {
                show_lens_studio = true;
            }
            ImGui::SetItemTooltip("Lens shape, Chromatic Aberration, Distortion, Fisheye, Tilt-Shift");

            //------ Composition and Aspect Ratio -----------
            ImGui::SeparatorText("Composition");
            ImGui::Combo("Guide", &renderer.composition_guide, COMPOSITION_GUIDE_NAMES, COMPOSITION_GUIDE_COUNT);
            ImGui::SetItemTooltip("Viewfinder overlay to frame the shot\n(doesn't affect the render)");

            const char* aspect_names[ASPECT_RATIO_COUNT];
            for(int i = 0; i < ASPECT_RATIO_COUNT; i++)
                aspect_names[i] = ASPECT_RATIOS[i].name;
            ImGui::Combo("Crop Frame", &renderer.aspect_frame, aspect_names, ASPECT_RATIO_COUNT);
            ImGui::SetItemTooltip("Preview a different aspect-ratio crop\n(doesn't affect the render)");

            if(changed) applyConfig();
        }
        //----- Models -------------
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
                resetModel();
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
        //----- Transform -------------
        if(ImGui::CollapsingHeader("Transform/Mesh")) {
            static bool t_changed = false;
            SceneModel& model = renderer.current_model;

            t_changed |= ImGui::DragFloat3("Position", value_ptr(model.position), 0.01f);
            t_changed |= ImGui::DragFloat3("Rotation", value_ptr(model.rotation), 1.0f, -360.0f, 360.0f, "%.1f°");
            t_changed |= ImGui::DragFloat("Scale", &model.scale.x, 0.01f, 0.001f, 100.0f);
            if(t_changed && model.scale.x != model.scale.y) model.scale.y = model.scale.z = model.scale.x;
            
            if(ImGui::Button("Apply")) {
                if(t_changed && !model.path.empty()) {
                    loadScene();
                    resetAccumulation();
                    t_changed = false;
                }
            }
            
            ImGui::SameLine();
            
            if(ImGui::Button("Reset##transform")) {
                model.position = vec3(0.0f);
                model.rotation = vec3(0.0f);
                model.scale = vec3(1.0f);
                loadScene();
                resetAccumulation();
            }
            ImGui::Separator();
            const char* mat_names[] = {"Disabled", "Diffuse", "Mirror", "Glass", "Tinted Glass"};
            int mat_idx = config.force_material + 1;
            if(ImGui::Combo("Force Material", &mat_idx, mat_names, 5)) {
                config.force_material = mat_idx - 1;
                glUseProgram(renderer.active_id);
                glUniform1i(renderer.loc_force_material, config.force_material);
                resetAccumulation();
            }

            ImGui::Text("%d triangles", model.tri_count);
            ImGui::Text("%d materials", model.mat_count);
            ImGui::Text("%s", filesystem::path(model.path).filename().string().c_str());
        }
        for (int i = 0; i < (int)loader.pending_cpu_mats.size(); i++) {
            const CPUMaterial& cpu_mat = loader.pending_cpu_mats[i];
            const GPUMaterial& gpu_mat = loader.pending_mats[i];

            const char* type_name = "Diffuse";
            if(gpu_mat.type == 1) type_name = "Mirror";
            if(gpu_mat.type == 2) type_name = "Glass";
            if(gpu_mat.type == 3) type_name = "Tinted Glass";

            ImGui::Text("[%d] %s | tex:%d | ior:%.2f", i, type_name, gpu_mat.tex_index, gpu_mat.ior);
            if(cpu_mat.has_texture)
                ImGui::TextDisabled("  %s", cpu_mat.tex_path.c_str());
        }
    }
    ImGui::End();

    //-----------------------------
    //-- Lens Studio Window -------
    if(show_lens_studio) {
        ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH * 0.5f, WINDOW_HEIGHT * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(380, 560), ImGuiCond_Appearing);

        if(ImGui::Begin("Lens Studio", &show_lens_studio)) {
            ImGui::SeparatorText("Presets");
            static int selected_preset = 0;
            int preset_count = lensPresetCount();
            vector<const char*> preset_names(preset_count);
            for (int i = 0; i < preset_count; i++) preset_names[i] = lensPresetAt(i).name;

            ImGui::Combo("##lenspreset", &selected_preset, preset_names.data(), preset_count);
            ImGui::SetItemTooltip("%s", lensPresetAt(selected_preset).description);
            ImGui::SameLine();
            if(ImGui::Button("Apply")) {
                applyLensPreset(selected_preset);
            }

            bool dof_enabled = (config.cam_aperture > 0.0f);

            ImGui::SeparatorText("Bokeh Shape");
            if(!dof_enabled) ImGui::TextDisabled("Needs Depth of Field enabled (Camera panel)");

            ImGui::BeginDisabled(!dof_enabled);
            changed |= ImGui::SliderInt("Aperture Blades", &config.cam_aperture_blades, 0, 12);
            resetBtn("*##blades", config.cam_aperture_blades, 0);
            ImGui::SetItemTooltip("Shape of the aperture disk.\n0-2 = perfectly circular (default), 5-9 = typical lens polygon bokeh");

            if(config.cam_aperture_blades >= 3) {
                changed |= ImGui::SliderFloat("Blade Rotation", &config.cam_blade_rotation, 0.0f, 60.0f, "%.1f°");
                resetBtn("*##bladerot", config.cam_blade_rotation, 0.0f);
            }

            changed |= ImGui::SliderFloat("Anamorphic Squeeze", &config.cam_anamorphic_squeeze, 1.0f, 2.5f, "%.2fx");
            resetBtn("*##squeeze", config.cam_anamorphic_squeeze, 1.0f);
            ImGui::SetItemTooltip("Stretches the bokeh horizontally.\n1.0 = off/circular, ~1.3-2.0 for an anamorphic look");

            changed |= ImGui::SliderFloat("Cat's-Eye", &config.cam_cateye_strength, 0.0f, 1.0f, "%.2f");
            resetBtn("*##cateye", config.cam_cateye_strength, 0.0f);
            ImGui::SetItemTooltip("Mechanical vignetting: clips bokeh shapes near the frame edges,\nlike a lens barrel occluding part of the aperture");
            ImGui::EndDisabled();

            ImGui::SeparatorText("Tilt-Shift");
            ImGui::BeginDisabled(!dof_enabled);
            changed |= ImGui::SliderFloat("Tilt", &config.cam_tilt, -15.0f, 15.0f, "%.1f°");
            resetBtn("*##tilt", config.cam_tilt, 0.0f);
            ImGui::SetItemTooltip("Pivots the focal plane around the horizontal axis (Scheimpflug).\n'Miniature' look at strong angles");
            ImGui::EndDisabled();

            ImGui::SeparatorText("Chromatic Aberration");
            bool lateral_ca_enabled = (config.cam_lateral_ca > 0.0f);
            if(ImGui::Checkbox("Enable##lateralca", &lateral_ca_enabled)) {
                config.cam_lateral_ca = lateral_ca_enabled ? 0.01f : 0.0f;
                changed = true;
            }
            ImGui::SetItemTooltip("Traces one full path per color channel (3x render cost)\nShares the trace with Axial CA and Glass Dispersion");

            if(lateral_ca_enabled) {
                changed |= ImGui::SliderFloat("Lateral CA", &config.cam_lateral_ca, 0.001f, 0.05f, "%.3f");
                resetBtn("*##lateralca", config.cam_lateral_ca, 0.01f);
                ImGui::SetItemTooltip("Per-channel magnification shift.\nZero at frame center, worse toward the corners - the classic red/cyan edge fringing");
            }

            bool axial_ca_enabled = (config.cam_axial_ca > 0.0f);
            if(ImGui::Checkbox("Enable##axialca", &axial_ca_enabled)) {
                config.cam_axial_ca = axial_ca_enabled ? 0.01f : 0.0f;
                changed = true;
            }
            ImGui::SetItemTooltip("Traces one full path per color channel (3x render cost)\nShares the trace with Lateral CA and Glass Dispersion");

            if(axial_ca_enabled) {
                changed |= ImGui::SliderFloat("Axial CA", &config.cam_axial_ca, 0.001f, 0.05f, "%.3f");
                resetBtn("*##axialca", config.cam_axial_ca, 0.01f);
                ImGui::SetItemTooltip("Per-channel focus shift (bokeh color fringing).\nOnly visible with Depth of Field enabled, same as a real lens");
            }

            bool dispersion_enabled = (config.glass_dispersion > 0.0f);
            if(ImGui::Checkbox("Enabled Glass Dispersion", &dispersion_enabled)) {
                config.glass_dispersion = dispersion_enabled ? 0.02f : 0.0f;
                changed = true;
            }
            ImGui::SetItemTooltip("Prism effect. White lights splits into color when refracting through glass\nTraces one path per color channel (3x render cost)\nShares the trace with Chromatic Aberration, so uses the same traces when both are on");

            if(dispersion_enabled) {
                changed |= ImGui::SliderFloat("Disp. Strength", &config.glass_dispersion, 0.001f, 0.1f, "%.3f");
                resetBtn("*##dispersion", config.glass_dispersion, 0.02f);
                ImGui::SetItemTooltip("Higher gives more visible color separation through glass");
            }

            ImGui::SeparatorText("Distortion & Projection");
            if(ImGui::Combo("Projection", &config.cam_projection_mode, CAM_PROJECTION_NAMES, CAM_PROJECTION_COUNT)) {
                float cap = camFovCap(config.cam_projection_mode);
                if(config.cam_fov > cap) config.cam_fov = cap;
                changed = true;
            }
            resetBtn("*##projmode", config.cam_projection_mode, 0);
            ImGui::SetItemTooltip("Rectilinear = normal lens (straight lines stay straight)\nFisheye modes cover a much wider FOV with curved lines");

            changed |= ImGui::SliderFloat("Distortion K1", &config.cam_distortion_k1, -0.5f, 0.5f, "%.3f");
            resetBtn("*##distk1", config.cam_distortion_k1, 0.0f);
            ImGui::SetItemTooltip("Negative = barrel (wide-angle look)\nPositive = pincushion (tele/zoom look)");

            changed |= ImGui::SliderFloat("Distortion K2", &config.cam_distortion_k2, -0.5f, 0.5f, "%.3f");
            resetBtn("*##distk2", config.cam_distortion_k2, 0.0f);
            ImGui::SetItemTooltip("Higher-order term.\nOpposite sign from K1 gives 'mustache' distortion");

            if(changed) applyConfig();
        }
        ImGui::End();
    }

    //-----------------------------
    //-- Right Window -------------
    ImGui::SetNextWindowPos(ImVec2(WINDOW_WIDTH - WINDOW_WIDTH * 0.22f - 10, 10), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(WINDOW_WIDTH * 0.22f, WINDOW_HEIGHT - 20.0f), ImGuiCond_Always);
    changed = false;
    //----- Render Settings -------------
    if(ImGui::Begin("Render Inspector")) {
        //----- Scene Presets --------------
        const char* preset_names[] = {"Mesh Only", "Cornell Box + Mesh", "Cornell Box", "Prism Demo"};
        if(ImGui::Combo("Scene Preset", &config.scene_preset, preset_names, 4)) {
            glUseProgram(renderer.active_id);
            glUniform1i(renderer.loc_scene_preset, config.scene_preset);
            
            //Revert every setting when changing presets
            RenderConfig defaults;
            config.use_ground_plane = defaults.use_ground_plane;
            config.ground_elevation = defaults.ground_elevation;
            config.ground_radius = defaults.ground_radius;
            config.background = defaults.background;
            config.force_material = defaults.force_material;
            config.glass_dispersion = defaults.glass_dispersion;
            config.cam_fov = defaults.cam_fov;
            glUniform1i(renderer.loc_force_material, config.force_material);
            if(renderer.use_env_map) {
                renderer.use_env_map = false;
                glUniform1i(renderer.loc_use_env_map, 0);
            }
            resetModel();

            if(config.scene_preset == 2) {
                renderer.MAX_SAMPLES = 100;
                camera.position = vec3(0.0f, -5.0f, 0.0f);
                camera.yaw = glm::radians(90.0f);
                camera.pitch = 0.0f;
                camera.lookat = camera.position + cameraForward();
                config.cam_fov = 30.0f;
                uploadConfig();
                uploadCamera();
            }
            else if(config.scene_preset == 3) {
                const string prism_path = "../models/prism/prism.obj";
                const string hdri_path = "../hdri/autumn_field_puresky_4k.hdr";
                bool model_found = filesystem::exists(prism_path);
                bool hdri_found = filesystem::exists(hdri_path);
                
                if (!model_found)
                    printf("\n[PRESET] Prism Demo: model not found as %s\n", prism_path.c_str());
                if (!hdri_found)
                    printf("\n[PRESET] Prism Demo: HDRI not found as %s\nFallback to a black background.\n", hdri_path.c_str());

                
                if(model_found) {
                    renderer.current_model.path = prism_path;
                    renderer.current_model.position = vec3(0.0f);
                    renderer.current_model.rotation = vec3(0.0f, 90.0f, 0.0f);
                    renderer.current_model.scale = vec3(2.5f);
                    loadScene();
                }
                renderer.MAX_SAMPLES = 100;

                config.use_ground_plane = true;
                config.ground_elevation = -1.0f;
                config.ground_radius = 8.0f;
                config.background = 0;

                config.force_material = 2;
                config.glass_dispersion = 0.04f;
                glUniform1i(renderer.loc_force_material, config.force_material);

                config.sun_enabled = false;
                camera.position = vec3(-3.0f, -6.0f, 1.5f);
                camera.yaw = glm::radians(60.0f);
                camera.pitch = glm::radians(-15.0f);
                camera.lookat = camera.position + cameraForward();
                config.cam_fov = 80.0f;
                uploadCamera();

                if(hdri_found) {
                    loadEnvMap("../hdri/autumn_field_puresky_4k.hdr", renderer);
                }
            }
            else {
                config.cam_fov = 80.0f;
            }
            uploadConfig();
            uploadCornellTopLight();
            uploadPrismDemoLight();
            uploadSun();
            resetAccumulation();
        }
        //----- Preview Config -------------
        if(ImGui::CollapsingHeader("Preview", ImGuiTreeNodeFlags_DefaultOpen)) {
            int res = config.moving_resolution;
            bool preview_locked = config.lock_preview_res;
            if(ImGui::Checkbox("##lock_preview", &preview_locked))
                setPreviewMode(preview_locked);
            ImGui::SetItemTooltip("Lock preview mode (same as Full Render/Return to Preview Mode)");
            
            ImGui::SameLine();
            if(ImGui::SliderInt("Preview Res", &res, 32, 512)) {
                res = (res / 16) * 16;
                res = glm::max(32, res);
                config.moving_resolution = res;
            }

            ImGui::Checkbox("Tile Dispatch", &config.tile_dispatch);
            ImGui::SetItemTooltip("Splits the render into smaller tiles per dispatch\nUse with weaker GPUs, avoids timeout crash");
            if(config.tile_dispatch) {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(80);
                ImGui::SliderInt("Rows", &config.tile_rows, 1, 16);
                ImGui::SetItemTooltip("Work groups per tile (16px each)\nLess = safe, but slower");
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
        //----- RENDER -------------
        if(ImGui::CollapsingHeader("Render", ImGuiTreeNodeFlags_DefaultOpen)) {
            
            int samples = (int)renderer.MAX_SAMPLES;
            if(ImGui::InputInt("Max Samples", &samples, 1, 5000)) {
                samples = glm::max(1, samples);
                renderer.MAX_SAMPLES = (uint)samples;
            }
            ImGui::SetItemTooltip("Max progressive samples per pixel");

            ImGui::Checkbox("Auto Denoise", &renderer.denoiser_enabled);
            ImGui::SetItemTooltip("Applies OIDN denoiser automatically");

            if(!renderer.denoise_status_msg.empty()) {
                double elapsed = glfwGetTime() - renderer.denoise_msg_time;
                if(elapsed < 4.0)
                    ImGui::TextDisabled("%s", renderer.denoise_status_msg.c_str());
                else  
                    renderer.denoise_status_msg = "";
            }

            changed |= ImGui::SliderInt("Depth", &config.depth, 1, 50);
            changed |= resetBtn("*##depth", config.depth, render_defaults.depth);
    
            changed |= ImGui::SliderInt("Rays/Pixel", &config.samples_per_pixel, 1, 16);
            changed |= resetBtn("*##spp", config.samples_per_pixel, render_defaults.samples_per_pixel);
    
            changed |= ImGui::Checkbox("NEE", &config.use_nee);
            ImGui::SetItemTooltip("Next Event Estimation\nDisable to compare with brute force");

            changed |= ImGui::Checkbox("Textures", &config.use_textures);
            ImGui::SetItemTooltip("Enable textures on model");

            bool hide_fireflies = (config.firefly_clamp > 0.0f);
            if(ImGui::Checkbox("Hide Fireflies", &hide_fireflies)) {
                config.firefly_clamp = hide_fireflies ? 10.0f : 0.0f;
                changed = true;
            }
            ImGui::SetItemTooltip("Reduces bright noise spots in dark areas");

            //----- RR -------------
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
    
            
            //----- Environment -------------
            if(ImGui::CollapsingHeader("Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
                //Simple Background
                const char* bg_names[] = {"Black", "White"};
                changed |= ImGui::Combo("Background", &config.background, bg_names, 2);

                //Exposure (display-only, in stops)
                ImGui::SliderFloat("Exposure", &config.exposure_ev, -5.0f, 5.0f, "%.2f EV");
                ImGui::SetItemTooltip("Brightness in stops. \
                    \nApplied before tone mapping.\n+1 EV doubles brightness, -1 EV halves it.");
                resetBtn("*##exposure", config.exposure_ev, 0.0f);

                if(ImGui::Checkbox("Auto Exposure", &config.auto_exposure_enabled))
                    resetAccumulation();
                ImGui::SetItemTooltip("Automatically adapts Exposure toward middle-gray in real time. \
                    \nBehaves gradually, like a camera's metering");
                if(config.auto_exposure_enabled) {
                    ImGui::SliderFloat("Speed##autoexp", &config.auto_exposure_speed, 0.02f, 1.0f, "%.2f");
                    ImGui::SetItemTooltip("How quick the Exposure updates");
                    resetBtn("*##autoexpspeed", config.auto_exposure_speed, 0.2f);
                }
                if(ImGui::Button("Snap Now")) {
                    updateAutoExposure(1.0f);
                }
                ImGui::SetItemTooltip("Instantly sets Exposure to the current target");

                //Tone Mapping
                const char* tm_names[] = {"None", "Reinhard", "ACES", "Filmic"};
                changed |= ImGui::Combo("Tone Map", &config.tone_mapping, tm_names, 4);
                
                //Vignette (display-only)
                ImGui::SliderFloat("Vignette", &config.vignette_strength, 0.0f, 1.0f, "%.2f");
                ImGui::SetItemTooltip("Darkens the corners of the frame");
                resetBtn("*##vignette", config.vignette_strength, 0.0f);

                if(changed) applyConfig();

                /// Environment Map HDRI
                if(ImGui::Checkbox("Enable Env Map", &renderer.use_env_map)) {
                    glUseProgram(renderer.active_id);
                    glUniform1i(renderer.loc_use_env_map, renderer.use_env_map ? 1 : 0);
                    resetAccumulation();
                }
                //Current HDRI file
                if(!renderer.current_env_map.empty()) {
                    string env_name = filesystem::path(renderer.current_env_map).filename().string();
                    ImGui::TextDisabled("%s", env_name.c_str());
                }
                else ImGui::TextDisabled("No env map loaded");

                //Select env map
                if(ImGui::SmallButton("Select Texture")) {
                    show_hdri_selector = true;
                    if(!hdri_list_loaded) {
                        hdri_list = scanHDRI("../hdri");
                        hdri_list_loaded = true;
                    }
                }

                if(!renderer.current_env_map.empty()) {
                    ImGui::SameLine();
                    if(ImGui::SmallButton("Clear")) {
                        renderer.current_env_map = "";
                        renderer.use_env_map = false;
                        glUseProgram(renderer.active_id);
                        glUniform1i(renderer.loc_use_env_map, 0);
                        resetAccumulation();
                    }
                }
                ///See external window for env map selector, outside of the side windows

                if(ImGui::CollapsingHeader("Ground Plane")) {
                    bool gp_changed = false;
                    bool shadow_catch = config.ground_shadow_catcher;

                    gp_changed |= ImGui::Checkbox("Enable plane", &config.use_ground_plane);
                    if(config.use_ground_plane) {
                        gp_changed |= ImGui::SliderFloat("Elevation##ground", &config.ground_elevation, -5.0f, 5.0f, "%.2f");
                        gp_changed |= ImGui::SliderFloat("Albedo##ground", &config.ground_albedo, 0.0f, 1.0f, "%.2f");
                        gp_changed |= ImGui::SliderFloat("Radius##ground", &config.ground_radius, 1.0f, 30.0f, "%.1f");
                        if(ImGui::Checkbox("Shadow Catcher##ground", &shadow_catch)) {
                            config.ground_shadow_catcher = shadow_catch;
                            gp_changed = true;
                        }
                        gp_changed |= ImGui::SliderFloat("Shadow Opacity##ground", &config.ground_shadow_opacity, 0.0f, 1.0f, "%.2f");
                    }
                    if(gp_changed) applyConfig();
                }
            }
        }
        if(ImGui::CollapsingHeader("Sun", ImGuiTreeNodeFlags_DefaultOpen)) {
            bool sun_changed = false;
            sun_changed |= ImGui::Checkbox("Enable##sun", &config.sun_enabled);
            sun_changed |= ImGui::SliderFloat("Elevation##sun", &config.sun_elevation, 0.0f, 90.0f, "%.1f°");
            sun_changed |= ImGui::SliderFloat("Horizontal##sun", &config.sun_azimuth, 0.0f, 360.0f, "%.1f°");
            sun_changed |= ImGui::SliderFloat("Intensity##sun", &config.sun_intensity, 0.0f, 20.0f, "%.1f");
            sun_changed |= ImGui::SliderFloat("Angular Size##sun", &config.sun_angular_radius, 0.1f, 10.0f, "%.2fº");
            ImGui::SetItemTooltip("Sets how visible the sun is in mirror/glass reflections\nReal sun is ~0.27º");
            sun_changed |= ImGui::ColorEdit3("Color##sun", value_ptr(config.sun_color));

            if(sun_changed) {
                uploadSun();
                resetAccumulation();
            }
        }
    }
    ImGui::End();

    ImVec2 screen = io.DisplaySize;

    //-- Model Loading Window -------------
    if(renderer.is_model_loading) {

        ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 100), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.80f);
        ImGui::Begin("##loading", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoMove);

        ImGui::SetWindowFontScale(1.5f);
        const char* stage_text = LOAD_STAGE_TEXT[loader.stage.load()];
        float text_w = ImGui::CalcTextSize(stage_text).x;
        ImGui::SetCursorPosX((400 - text_w) * 0.5f);
        ImGui::Text("%s", stage_text);

        ImGui::SetWindowFontScale(1.0f);
        string model_name = filesystem::path(renderer.current_model.path).string();
        float model_name_w = ImGui::CalcTextSize(model_name.c_str()).x;
        ImGui::SetCursorPosX((400 - model_name_w) * 0.5f);
        ImGui::TextDisabled("%s", model_name.c_str());

        ImGui::End();
    }

    //-- Env Map Selector Window -------------
    if(show_hdri_selector) {
        ImGui::SetNextWindowPos(ImVec2(screen.x * 0.5f, screen.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_Appearing);
        
        if(ImGui::Begin("Select Environment Map", &show_hdri_selector)) {
            if(ImGui::SmallButton("Scan")) {
                hdri_list = scanHDRI("../hdri");
                hdri_list_loaded = true;
            }
            ImGui::SameLine();
            ImGui::Text("%zu maps", hdri_list.size());
            ImGui::Separator();

            //HDRI list interface
            ImGui::BeginChild("hdri_list", ImVec2(0, -30), true);
            for(int i = 0; i < (int)hdri_list.size(); i++) {
                const string& path = hdri_list[i];
                string folder = filesystem::path(path).parent_path().filename().string();
                string stem = filesystem::path(path).stem().string();
                string name = folder + "/" + stem;
                string label = name + "###hdri" + to_string(i);

                bool selected = (filesystem::path(path).lexically_normal() == filesystem::path(renderer.current_env_map).lexically_normal());

                if(ImGui::Selectable(label.c_str(), selected)) {
                    loadEnvMap(path, renderer);
                    renderer.current_env_map = path;
                    renderer.use_env_map = true;
                    glUseProgram(renderer.active_id);
                    glUniform1i(renderer.loc_use_env_map, 1);
                    resetAccumulation();
                    show_hdri_selector = false;
                }
                if(ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s", path.c_str());
                }
            }
            ImGui::EndChild();

            if(ImGui::Button("Close", ImVec2(-1, 0)))
                show_hdri_selector = false;
        }
        ImGui::End();
    }
    

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}