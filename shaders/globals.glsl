
const float PI  = 3.14159265359;
const float TWO_PI = 2.0 * PI;
const float INV_PI = 1.0 / PI;
const float INF = 1e30;
const float EPS = 0.001;
const float EPS_TRI = 0.0001;
const int DEPTH = 10;

///\defgroup camera Camera Settings
///\{
///Lens aperture `0.0` = pinhole, normal values up to `0.3`
const float CAM_APERTURE = 0.00;

///Worldspace distance between camera and focal plane
const float CAM_FOCAL_DISTANCE = 4.7;

///Enables debug focal plane viewer
const bool FOCAL_DEBUG = false;

///Thickness of the focal plane line
const float FOCAL_BAND_DEBUG = 0.05;

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
const int BACKGROUND = BG_WHITE;
///\}

///\defgroup tone_map Tone Mapping
///\{
const int TM_NONE = 0;
const int TM_REINHARD = 1;
const int TM_ACES = 2;

const int TONE_MAPPING = TM_ACES;
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

const int SAMPLES_PER_PIXEL = 4;
                                                                                                                    
/**\brief Russian Roulette, minimum bounces before enabling.
Changing to more or less gives minimal performance changes
Avoids ending paths too early.
Adds noise to the image if turned on.
\note Turns off RR if `0`
\note Default value `3`.
*/
const int RR_MIN_BOUNCES = 0;

/**Russian Roulette, maximum chance at surviving to avoid excessive throughput.
The less the value, the more noise appears for cutted rays, but increases performance.
\note Default value `0.95`. Safest number without noise is ~0.75.
*/
const float RR_MAX_SURVIVAL = 0.95;

///\}

///\defgroup materials Materials
///\{
const int MAT_DIFFUSE = 0;
const int MAT_MIRROR = 1;
const int MAT_GLASS = 2;
const int MAT_TINTED_GLASS = 3;
///\}

///\defgroup mesh mesh
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