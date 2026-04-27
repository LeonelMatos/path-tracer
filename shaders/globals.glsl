
const float PI  = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float INV_PI = 1.0 / PI;
const float INF = 1e30;
const float EPS = 0.001;
const float EPS_TRI = 0.0001;

uniform int DEPTH;

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

///Camera FOV converted to radians
const float CAM_FOV_RAD = 30.0 * PI / 180.0;
///\}

///\defgroup ambient Ambient Settings
///\{
//Background types
const int BG_BLACK = 0;
const int BG_WHITE = 1;
const int BG_GRADIENT = 2;

///Color result if the ray doesn't reach the light
uniform int BACKGROUND;
///\}

///\defgroup tone_map Tone Mapping
///\{
const int TM_NONE = 0;
const int TM_REINHARD = 1;
const int TM_ACES = 2;

uniform int TONE_MAPPING;
///\}

///\defgroup path Path Tracer
///\{
struct Ray {
    vec3 origin;
    vec3 direction;
};

///ior = index of refraction (only used with MAT_GLASS)
struct Hit {
    float t;
    vec3 pos, normal, albedo, emission;
    int material;
    float ior;
};

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

///\}

///\defgroup materials Materials
///\{
const int MAT_DIFFUSE = 0;
const int MAT_MIRROR = 1;
const int MAT_GLASS = 2;
const int MAT_TINTED_GLASS = 3;
///\}

///\defgroup mesh Mesh
///\{
struct GPUVertex {
    vec3 position;
    float _pad0;
    vec3 normal;
    float _pad1;
    vec2 texcoord;
    vec2 _pad2;
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
    float _pad[2];
};

///Shader Storage Buffer Objects SSBO (GL 4.3+)
///See https://ktstephano.github.io/rendering/opengl/ssbos
layout(std430, binding = 2) buffer TriangleBuffer {
    GPUTriangle triangles[];
};

layout(std430, binding = 3) buffer MaterialBuffer {
    GPUMaterial gpu_materials[];
};

///\}

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

///\}