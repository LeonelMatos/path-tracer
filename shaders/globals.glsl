
const float PI  = 3.14159265359;
const float INF = 1e30;
const float EPS = 0.001;
const int DEPTH = 20;

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
///\}

///\defgroup ambient Ambient Settings
///\{
//Background types
const int BG_BLACK = 0;
const int BG_WHITE = 1;
const int BG_GRADIENT = 2;

///Color result if the ray doesn't reach the light
const int BACKGROUND = BG_GRADIENT;
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

const int SAMPLES_PER_PIXEL = 10;

///\brief Russian Roulette, minimum bounces before enabling
///\note Avoids ending paths too early
///\note Turns off RR if `0`
const int RR_MIN_BOUNCES = 3;

///Russian Roulette, maximum chance at surviving to avoid excessive throughput
const float RR_MAX_SURVIVAL = 0.95;

///\}

///\defgroup materials Materials
///\{
const int MAT_DIFFUSE = 0;
const int MAT_MIRROR = 1;
const int MAT_GLASS = 2;
///\}
