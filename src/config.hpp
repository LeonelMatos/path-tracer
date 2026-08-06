#pragma once
#include "gl/program.hpp"
#include <string>
#include <vector>
#include <filesystem>
#include <glm/glm.hpp>
#include <GL/glew.h>
#include <atomic>
#include <glm/gtc/matrix_transform.hpp>

///\brief Program window resolution
inline int WINDOW_WIDTH = 1280, WINDOW_HEIGHT = 720;
inline bool is_fullscreen = false;
inline int windowed_x = 100, windowed_y = 100;
inline int windowed_w = 1280, windowed_h = 720;

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
    int scene_preset = 0;

    int depth = 5;
    int samples_per_pixel = 1;
    int rr_min_bounces = 2;
    float rr_max_survival = 0.60;
    
    int background = 1;
    
    int tone_mapping = 2;

    ///Vignette strength for display shader
    ///\note 0 = off (default)
    float vignette_strength = 0.0f;

    ///Exposure in stops (EV), converted to a linear mult
    ///\note 0 = no change
    float exposure_ev = 0.0f;

    ///Runs updateAutoExposure() periodically while true
    bool auto_exposure_enabled = false;

    ///Speed of the exposure adjustement
    ///\note 0 = frozen, 1 = instant, 0-1 = moves gradually
    float auto_exposure_speed = 0.2f;

    bool use_ground_plane = false;
    float ground_elevation = -1.0f;
    float ground_albedo = 0.8f;
    float ground_radius = 5.0f;
    
    float cam_aperture = 0.00;
    float cam_focal_distance = 4.7;
    float cam_fov = 80.0f;
    
    bool focal_debug = false;
    float focal_band_debug = 0.05;

    ///Aperture blade count for bokeh shape
    ///\note <3 = perfect circular (default)
    int cam_aperture_blades = 0;

    ///Rotates the polygon aperture shape, in degrees
    float cam_blade_rotation = 0.0f;

    ///Anamorphic sqeeze ratio for oval bokeh
    float cam_anamorphic_squeeze = 1.0f;

    ///Mechanical vignette on the bokeh shape near frame edges, like cat's-eye
    float cam_cateye_strength = 0.0f;

    ///Radial lens distortion. Negative K1 = barrel, Positive K1 = pincushion
    float cam_distortion_k1 = 0.0f;
    float cam_distortion_k2 = 0.0f;

    ///Camera projection
    ///0=rectilinear, 1=equidistant fisheye, 2=stereographic fisheye, 3=equisolid fisheye
    int cam_projection_mode = 0;

    ///Tilt-shift, tilts the focal plane around the camera's local x-axis, in degrees
    float cam_tilt = 0.0f;
    
    ///Lateral (transverse) Chromatic Aberration strength, per-channel shift
    float cam_lateral_ca = 0.0f;

    //Axial (longitudinal) Chromatic Aberration, per-channel focus shift
    float cam_axial_ca = 0.0f;

    ///Glass dispersion strength (prism effect). Perturbs the IOR of MAT_GLASS
    float glass_dispersion = 0.0f;

    //Not part of the shader config
    ///render resolution when moving the camera and while Preview Mode is active
    int moving_resolution = 256;
    //Locks the viewport into preview pode while active
    ///Persistent preview mode, pins the renderer at moving_resolution to keep the program
    ///responsive. Interacted via the Render button
    ///\see setPreviewMode, setFullResolution
    bool lock_preview_res = true;
    ///controls the progressive resolution scaling up to the original
    bool progressive_refine = true;

    ///Splits the dispatch of the compute shader in smaller tiles
    ///\note necessary method on weak GPUs to avoid watchdog driver timeout if one dispatch takes too long.
    bool tile_dispatch = false;
    ///Number of work groups per tile. Less is more safe, but more sync overhead
    int tile_rows = 4;

    ///Enables Next Event Estimation
    bool use_nee = true;

    ///Forces one material on the mesh
    ///\note -1 = disabled
    int force_material = -1;

    ///Texture enable
    bool use_textures = true;

    /**Firefly clamping
    \note 0 is disabled\n10 is default
    */
    float firefly_clamp = 10.0f;

    ///Shadow catcher for ground to only have shadows visible
    bool ground_shadow_catcher = false;

    float ground_shadow_opacity = 0.6f;

    //Sun
    bool sun_enabled = true;
    float sun_elevation = 45.0f;
    float sun_azimuth = 180.0f;
    float sun_intensity = 2.5f;
    glm::vec3 sun_color = glm::vec3(1.0f, 0.95f, 0.8f);
    float sun_angular_radius = 2.0f;

    glm::vec3 sunDirection() {
        float el = glm::radians(sun_elevation);
        float az = glm::radians(sun_azimuth);
        return normalize(glm::vec3(cos(el) * sin(az), cos(el) * cos(az), sin(el)));
    }
};

struct Renderer {
    int render_w = WINDOW_WIDTH, render_h = WINDOW_HEIGHT;

    int v_sync = 0;

    uint MAX_SAMPLES = 50;

    SceneModel current_model;

    bool is_model_loading = false;

    ///Loaded model's world-space bounds, used for zoom-to-fit.
    ///Defines a small box around the origin fitting the model.
    glm::vec3 current_bounds_min = glm::vec3(-1.0f);
    glm::vec3 current_bounds_max = glm::vec3(1.0f);
    
    GLProgram display_id, pathtr_frag_id, pathtr_comp_id;
    GLuint active_id;
    GLuint tex[2], fbo[2], vao;
    GLint loc_res, loc_frame, loc_prev, loc_tex;
    int frame_id = 0, cur_f = 0, prev_f = 1;

    GLint loc_tile_offset;
    
    GLint loc_depth, loc_spp, loc_rr_min, loc_rr_max, loc_aperture, loc_cam_fov;
    GLint loc_focal_dist, loc_focal_debug, loc_focal_band, loc_background, loc_tone_map;
    
    GLint loc_cam_pos, loc_cam_lookat, loc_cam_up;
    
    GLint loc_display_render_res;
    GLint loc_display_res;

    GLint loc_lateral_ca, loc_axial_ca;
    GLint loc_aperture_blades, loc_blade_rotation;
    GLint loc_anamorphic_squeeze;
    GLint loc_cateye_strength;
    GLint loc_distortion_k1, loc_distortion_k2;
    GLint loc_projection_mode;
    GLint loc_cam_tilt;
    GLint loc_display_cam_fov;
    
    GLint loc_glass_dispersion;

    ///TONE_MAPPING is declared separately here because it must be uploaded to
    ///renderer.display_id specifically, toneMap() only runs in display.frag
    ///Uploading it to the path-trace program (as loc_tone_map) has no effect on what is shown,
    ///since the program never calls toneMap().
    GLint loc_display_tone_map;
    GLint loc_vignette;
    GLint loc_exposure;
    
    bool show_grid = false;
    GLProgram grid_id;
    GLint grid_loc_view, grid_loc_proj, grid_loc_near, grid_loc_far, grid_loc_cam_pos;

    ///Photo framing overlay (viewfinder, screen-space only)
    ///\note 0=off, 1=thirds, 2=golden ration, 3=center cross, 4=diagonal
    ///\see drawCompositionGuides
    int composition_guide = 0;

    ///Aspect-ratio crop preview mask on top of the render
    ///\note index into ASPECT_RATIOS; 0=off
    ///\see drawCompositionGuides
    int aspect_frame = 0;

    GLuint triangle_ssbo;
    GLuint material_ssbo;
    ///Triangle count fixed value passed pre-calculated
    GLint loc_tri_count;
    GLint loc_aabb_min, loc_aabb_max;

    //BVH
    GLuint bvh_ssbo;
    GLint loc_bvh_root;
    GLint loc_bvh_heatmap, loc_bvh_heatmap_scale;
    bool bvh_heatmap = false;
    int heatmap_scale = 30;

    //NEE
    GLint loc_use_nee;

    // Firefly Clamp
    GLint loc_firefly_clamp;

    //Light
    //memory buffers
    GLuint light_ssbo;
    GLint loc_light_count;
    GLuint analytic_light_ssbo;
    GLint loc_analytic_light_count;

    //Scene Presets
    GLint loc_scene_preset;

    //HDRI Environment Map
    GLuint env_map_tex = 0;
    GLint loc_env_map;
    GLint loc_use_env_map;
    bool use_env_map = false;
    std::string current_env_map = "";

    //Textures
    GLuint tex_array = 0;
    GLint loc_tex_array;
    GLint loc_use_textures;

    //Materials
    GLint loc_force_material;

    //Ground plane
    GLint loc_use_ground_plane;
    GLint loc_ground_elevation;
    GLint loc_ground_albedo;
    GLint loc_ground_radius;

    GLint loc_ground_shadow_catcher;
    GLint loc_ground_shadow_opacity;

    //Denoiser
    GLuint denoised_tex = 0;
    //denoiser status showing the result
    bool denoiser_active = false;
    //enable toggle on UI
    bool denoiser_enabled = true;
    int next_denoise_idx = 0;
    std::vector<int> denoise_checkpoints;
    std::string denoise_status_msg = "";
    double denoise_msg_time = 0.0;
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

///\note must stay synced with TONE_MAPPING consts in globals.glsl
const int TM_NONE = 0;
const int TM_REINHARD = 1;
const int TM_ACES = 2;
const int TM_FILMIC = 3;

///\brief CPU-side tone mapping, mirrors display frag's toneMap(), so screenshots match what's shown
///in render output, and not an estimation
///\see display.frag, saveScreenshot
inline glm::vec3 toneMapCPU(glm::vec3 color, int tone_mapping) {
    switch(tone_mapping) {
        case TM_REINHARD:
            return color / (color + glm::vec3(1.0f));
        case TM_ACES: {
            const glm::mat3 inputMat(
                0.59719f, 0.07600f, 0.02840f,
                0.35458f, 0.90834f, 0.13383f,
                0.04823f, 0.01566f, 0.83777f
            );
            const glm::mat3 outputMat(
                1.60475f, -0.10208f, -0.00327f,
                -0.53108f,  1.10813f, -0.07276f,
                -0.07367f, -0.00605f,  1.07602f
            );
            glm::vec3 v = inputMat * color;
            glm::vec3 a = v * (v + 0.0245786f) - 0.000090537f;
            glm::vec3 b = v * (0.983729f * v + 0.4329510f) + 0.238081f;
            return glm::clamp(outputMat * (a / b), 0.0f, 1.0f);
        }
        case TM_FILMIC: {
            //Matches display.frag's uncharted2ToneMapPartial,uncharted2Filmic
            const float A = 0.15f, B = 0.50f, C = 0.10f, D = 0.20f, E = 0.02f, F = 0.30f;
            auto partial = [&](glm::vec3 x) -> glm::vec3 {
                return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F)) - glm::vec3(E/F);
            };
            const float exposure_bias = 2.0f;
            glm::vec3 curr = partial(color * exposure_bias);
            glm::vec3 white_scale = glm::vec3(1.0f) / partial(glm::vec3(11.2f));
            return glm::clamp(curr * white_scale, 0.0f, 1.0f);
        }
        default:
            return glm::clamp(color, 0.0f, 1.0f);
    }
}

///\brief Aspect-ratio crop preview/screenshot options
struct AspectRatioOption { const char* name; float ratio; };

const AspectRatioOption ASPECT_RATIOS[] = {
    {"Off", 0.0f},
    {"1:1 Square", 1.0f},
    {"4:5 Portrait", 4.0f/5.0f},
    {"5:4", 5.0f/4.0f},
    {"3:2 Photo", 3.0f/2.0f},
    {"4:3", 4.0f/3.0f},
    {"16:9 Widescreen", 16.0f/9.0f},
    {"1.85:1 Cinema", 1.85f},
    {"2.35:1 Cinemascope", 2.35f}
};

const int ASPECT_RATIO_COUNT = sizeof(ASPECT_RATIOS) / sizeof(ASPECT_RATIOS[0]);

///Names for the compositions dropdown, indexed by renderer.composition_guide
///\see drawCompositionGuides
inline const char* COMPOSITION_GUIDE_NAMES[] = {
    "Off", "Rule of Thirds", "Golden Ratio", "Center Cross", "Diagonal"
};

const int COMPOSITION_GUIDE_COUNT = sizeof(COMPOSITION_GUIDE_NAMES) / sizeof(COMPOSITION_GUIDE_NAMES[0]);

///Names for the lens projection dropdown, indexed by config.cam_projection_mode
///\note must stay synced with PROJ_* constants in globals.glsl
inline const char* CAM_PROJECTION_NAMES[] = {
    "Rectilinear", "Fisheye (Equidistant)", "Fisheye (Stereographic)", "Fisheye (Equisolid)"
};

const int CAM_PROJECTION_COUNT = sizeof(CAM_PROJECTION_NAMES) / sizeof(CAM_PROJECTION_NAMES[0]);