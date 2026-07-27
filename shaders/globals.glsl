
const float PI  = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float INV_PI = 1.0 / PI;
const float INF = 1e30;
const float EPS = 1e-5;
const float EPS_TRI = 1e-10;
const float EPS_SHADOW = 1e-4;


//----------------------------------------------------------
///\defgroup camera Camera Settings
///\{
///Lens aperture `0.0` = pinhole (no DOF), normal values up to `0.3`
uniform float CAM_APERTURE;

///Worldspace distance between camera and focal plane
uniform float CAM_FOCAL_DISTANCE;

///Enables debug focal plane viewer
uniform bool FOCAL_DEBUG;

///Thickness of the focal plane line
uniform float FOCAL_BAND_DEBUG;

uniform float CAM_FOV;

///Camera position in world space
uniform vec3 camera_position;

///Camera direction
uniform vec3 camera_lookat;

///Camera's up vector
uniform vec3 camera_up;

/**Lens chromatic aberration strength.
Perturbs the effective focal length and focal distance per color channel
Consequence of the camera model, not screen-space post-processing.
\see cameraRayDOFChannel;
\note `0.0` = off (default)
*/
uniform float CAM_CHROMATIC_ABERRATION;

///Simple 3-point dispersion approx.
const float DISPERSION_COEFF[3] = float[3](-1.0, 0.0, 1.0); //R, G, B

///How wide a range each channel's per-sample jitter covers its DISPERSION_COEFF
///\note 1.3 gives a bit of overlap, so there's no visible seam between channels.
const float DISPERSION_JITTER_WIDTH = 1.3;

///\}


//----------------------------------------------------------
///\defgroup ambient Ambient Settings
///\{
//Background types
const int BG_BLACK = 0;
const int BG_WHITE = 1;
const int BG_GRADIENT = 2;

///Color result if the ray doesn't reach the light
uniform int BACKGROUND;

uniform sampler2D env_map;
uniform int USE_ENV_MAP;

//Ground
///Enable the ground
uniform int USE_GROUND_PLANE;
uniform float GROUND_ELEVATION;
uniform float GROUND_ALBEDO;
uniform float GROUND_RADIUS;

uniform int GROUND_SHADOW_CATCHER;
uniform float GROUND_SHADOW_OPACITY;
///\}


//----------------------------------------------------------
///\defgroup tone_map Tone Mapping
///\{
const int TM_NONE = 0;
const int TM_REINHARD = 1;
const int TM_ACES = 2;

uniform int TONE_MAPPING;

///Vignette strength for display shader
///\note 0.0 = 0ff (default)
uniform float VIGNETTE_STRENGTH;

///Exposure as linear multiplier applied before tone map.
///converted from Stops (exp2(EV)), so 0 EV is 1.0
uniform float EXPOSURE;
///\}


//----------------------------------------------------------
///\defgroup path Path Tracer
///\{
struct Ray {
    vec3 origin;
    vec3 direction;
};

///ior = index of refraction (only used with MAT_GLASS)
struct Hit {
    float t;
    vec3 pos, normal, geom_normal;
    vec3 albedo, emission;
    int material;
    float ior;
};

uniform int DEPTH;

uniform int SAMPLES_PER_PIXEL;
                                                                                                                    
/**\brief Russian Roulette, minimum bounces before enabling.
Changing to more or less gives minimal performance changes
Avoids ending paths too early.
Adds noise to the image if turned on.
\note Turns off RR if `0`
\note Default value `3`.
*/
uniform int RR_MIN_BOUNCES;

/**Russian Roulette, maximum chance at surviving to avoid excessive throughput.
The less the value, the more noise appears for cutted rays, but increases performance.
\note Default value `0.95`. Safest number without noise is ~0.75.
*/
uniform float RR_MAX_SURVIVAL;

///Enables NEE
uniform int USE_NEE;

/**Firefly Clamping. Env Maps can create residual fireflies from
samples with high color variance that converge slowly, specially in shadows
*/
uniform float FIREFLY_CLAMP;
///\}


//----------------------------------------------------------
///\defgroup materials Materials
///\{
const int MAT_DIFFUSE = 0;
const int MAT_MIRROR = 1;
const int MAT_GLASS = 2;
const int MAT_TINTED_GLASS = 3;
const int MAT_SHADOW_CATCHER = 4;

///Forces one material from the gui
///\note -1 = disabled
uniform int FORCE_MATERIAL;

///Glass dispersion strength (prism effect)
//It perturbs the IOR of glass material.
///\see pathTrace, DISPERSION_COEFF
uniform float GLASS_DISPERSION;
///\}


//----------------------------------------------------------
//\defgroup light Light Types
///\{
///\note values need to sync in mesh.hpp
const int LIGHT_POINT =  0;
const int LIGHT_DIRECTIONAL = 1;
const int LIGHT_SPOT = 2;
///\}


///\defgroup mesh Mesh
///\{
struct GPUVertex {
    vec3 position;
    float _pad0;
    vec3 normal;
    float _pad1;
    vec2 texcoord;
};

struct GPUTriangle {
    GPUVertex v0, v1, v2;
    int material_id;
    float _pad[3];
};

struct GPUMaterial {
    vec4 albedo;
    vec4 emission;
    int type;
    float ior;
    int tex_index;
    float _pad;
};

uniform sampler2DArray tex_albedo;

uniform int USE_TEXTURES;

struct GPULight {
    vec4 position;
    vec4 emission;
    vec4 direction;
    int type;
    float radius;
    float spot_inner;
    float spot_outer;
};

///Shader Storage Buffer Objects SSBO (GL 4.3+)
///See https://ktstephano.github.io/rendering/opengl/ssbos
///Triangle Buffer
layout(std430, binding = 2) buffer TriangleBuffer {
    GPUTriangle triangles[];
};

///Material Buffer
layout(std430, binding = 3) buffer MaterialBuffer {
    GPUMaterial gpu_materials[];
};

///Light Buffer
layout(std430, binding = 9) readonly buffer LightBuffer {
    int light_indices[];
};
uniform int light_count;

layout(std430, binding = 10) readonly buffer AnalytticLightBuffer {
    GPULight analytic_lights[];
};
uniform int analytic_light_count;

///\}


//----------------------------------------------------------
///\defgroup mesh_aabb AABB Early rejection
///\brief Bounding box of the loaded mesh to skip the triangle loop for
///rays that don't interact with the model. Improves performance
///\see intersects, loadMesh
///\{

uniform vec3 mesh_aabb_min;
uniform vec3 mesh_aabb_max;

///Pre-calculated count to avoid unnecessary operations in the GPU runtime
///Used in intersects triangles loop
uniform int triangle_count;

///\}


//----------------------------------------------------------
///\defgroup bvh BVH
///\{
struct BVHNode {
    vec3 aabb_min;
    float _pad0;
    vec3 aabb_max;
    float _pad1;
    int left_child;
    int right_child;
    int first_tri;
    int tri_count;
};

layout(std430, binding = 8) buffer BVHBuffer {
    BVHNode bvh_nodes[];
};

uniform int bvh_root;

///enables the BVH's heatmap view
uniform int USE_BVH_HEATMAP;

///nodes to saturate \note default 30
///\bug When enabling heatmap, the scale doesn't apply until setting it.
uniform int BVH_HEATMAP_SCALE;

///\}