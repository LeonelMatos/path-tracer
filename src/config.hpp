#pragma once
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

/// @brief Program window resolution
int WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720;

/**Switches between using fragment or compute shaders
for the path tracer
\note false = fragment; true = compute*/
const bool USE_COMPUTE_SH = true;

struct RenderConfig {
    int depth = 5;
    int samples_per_pixel = 1;
    int rr_min_bounces = 3;
    float rr_max_survival = 0.75;
    
    int background = 1;
    
    int tone_mapping = 2;
    
    float cam_aperture = 0.00;
    float cam_focal_distance = 4.7;
    
    bool focal_debug = false;
    float focal_band_debug = 0.05;
    
    //Not part of the shader config
    ///render resolution when moving the camera
    int moving_resolution = 128;
    ///controls the progressive resolution scaling up to the original
    bool progressive_refine = true;
};

struct Renderer {
    int render_w = WINDOW_WIDTH, render_h = WINDOW_HEIGHT;
    
    GLuint display_id, pathtr_frag_id, pathtr_comp_id;
    GLuint active_id;
    GLuint tex[2], fbo[2], vao;
    GLint loc_res, loc_frame, loc_prev, loc_tex;
    int frame_id = 0, cur_f = 0, prev_f = 1;
    
    GLint loc_depth, loc_spp, loc_rr_min, loc_rr_max, loc_aperture;
    GLint loc_focal_dist, loc_focal_debug, loc_focal_band, loc_background, loc_tone_map;
    
    GLint loc_cam_pos, loc_cam_lookat, loc_cam_up;
    
    GLint loc_display_render_res;
    GLint loc_display_res;
    
    bool show_grid = true;
    GLuint grid_id = 0;
    GLint grid_loc_view, grid_loc_proj, grid_loc_near, grid_loc_far;
};

struct CameraConfig {
    glm::vec3 position = glm::vec3(0.0f, -5.0f, 0.0f);
    glm::vec3 lookat = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);
    
    float move_speed = 0.01f;
    float mouse_sens = 0.002f;
    float pitch = 0.0f;
    float yaw = 1.57f;
    bool moving = false;
    bool returning_home = false;
    float return_speed = 3.0f;
};

///ACES tone map on the CPU side, an approximation of the shader ACES
///\see saveScreenshot
auto aces_approx = [](float x) -> float {
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return glm::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
};