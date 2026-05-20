#pragma once
#include <string>
#include <filesystem>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <glm/gtc/matrix_transform.hpp>

///\brief Program window resolution
inline int WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720;

/**Switches between using fragment or compute shaders
for the path tracer
\note false = fragment; true = compute*/
const bool USE_COMPUTE_SH = true;

struct SceneModel {
    std::string path = "";
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    int tri_count = 0;
    int mat_count = 0;
};

struct RenderConfig {
    int depth = 5;
    int samples_per_pixel = 1;
    int rr_min_bounces = 3;
    float rr_max_survival = 0.75;
    
    int background = 1;
    
    int tone_mapping = 2;
    
    float cam_aperture = 0.00;
    float cam_focal_distance = 4.7;
    float cam_fov = 30.0f;
    
    bool focal_debug = false;
    float focal_band_debug = 0.05;
    
    //Not part of the shader config
    ///render resolution when moving the camera
    int moving_resolution = 256;
    ///controls the progressive resolution scaling up to the original
    bool progressive_refine = true;

    ///Enables Next Event Estimation
    bool use_nee = true;

    //Sun
    bool sun_enabled = true;
    float sun_elevation = 45.0f;
    float sun_azimuth = 180.0f;
    float sun_intensity = 5.0f;
    glm::vec3 sun_color = glm::vec3(1.0f, 0.95f, 0.8f);

    glm::vec3 sunDirection() {
        float el = glm::radians(sun_elevation);
        float az = glm::radians(sun_azimuth);
        return normalize(glm::vec3(cos(el) * sin(az), cos(el) * cos(az), sin(el)));
    }
};

struct Renderer {
    int render_w = WINDOW_WIDTH, render_h = WINDOW_HEIGHT;

    int v_sync = 0;

    uint MAX_SAMPLES = 200;

    SceneModel current_model;
    
    GLuint display_id, pathtr_frag_id, pathtr_comp_id;
    GLuint active_id;
    GLuint tex[2], fbo[2], vao;
    GLint loc_res, loc_frame, loc_prev, loc_tex;
    int frame_id = 0, cur_f = 0, prev_f = 1;
    
    GLint loc_depth, loc_spp, loc_rr_min, loc_rr_max, loc_aperture, loc_cam_fov;
    GLint loc_focal_dist, loc_focal_debug, loc_focal_band, loc_background, loc_tone_map;
    
    GLint loc_cam_pos, loc_cam_lookat, loc_cam_up;
    
    GLint loc_display_render_res;
    GLint loc_display_res;
    
    bool show_grid = false;
    GLuint grid_id = 0;
    GLint grid_loc_view, grid_loc_proj, grid_loc_near, grid_loc_far, grid_loc_cam_pos;

    GLuint triangle_ssbo;
    GLuint material_ssbo;
    ///Triangle count fixed value passed pre-calculated
    GLint loc_tri_count;
    GLint loc_aabb_min, loc_aabb_max;

    //BVH
    GLuint bvh_ssbo;
    GLint loc_bvh_root;

    //NEE
    GLint loc_use_nee;

    //Light
    //memory buffers
    GLuint light_ssbo;
    GLint loc_light_count;
    GLuint analytic_light_ssbo;
    GLint loc_analytic_light_count;
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

/*----------------------------------------------------------
  Screenshots
*/

inline std::string screenshot_msg = "";
inline double screenshot_msg_time = 0.0;

///ACES tone map on the CPU side, an approximation of the shader ACES
///\see saveScreenshot
inline auto aces_approx = [](float x) -> float {
    float a = 2.51f, b = 0.03f, c = 2.43f, d = 0.59f, e = 0.14f;
    return glm::clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
};