
const float PI  = 3.14159265359;
const float INF = 1e30;
const float EPS = 0.001;
const int DEPTH = 8;

//Ambient Settings
//Background types
const int BG_BLACK = 0;
const int BG_WHITE = 1;
const int BG_GRADIENT = 2;

const int BACKGROUND = BG_BLACK;

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

//Materials
const int MAT_DIFFUSE = 0;
const int MAT_MIRROR = 1;
const int MAT_GLASS = 2;

