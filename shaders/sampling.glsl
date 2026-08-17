
//Adapted from https://www.scribd.com/document/582697876/Jarzynski2020Hash
vec3 pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    v ^= v >> 16u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    return vec3(v) * (1.0 / float(0xFFFFFFFFu));
}

///\note using frame_id as seed gives same output
///\note px coords passed from frag or comp shader
vec3 rand3(int bounce, int path_id, uvec2 px) {
    uint fid = uint(frame_id) * 2654435761u;
    uint sid = uint(path_id) * 3266489917u;
    return pcg3d(uvec3(
        px.x ^ fid ^ sid,
        px.y ^ fid ^ sid,
        uint(bounce + 1)
    ));
}

mat3 onb(vec3 n) {
    vec3 up = abs(n.x) > 0.9 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 t = normalize(cross(up, n));
    return mat3(t, cross(n, t), n);
}

///Samples a random point in the triangle surface uniformly distributed, based on R. Osada, Shape Distributions
vec3 sampleTriangle(vec3 v0, vec3 v1, vec3 v2, vec2 rnd) {
    float su0 = sqrt(rnd.x);
    float b0 = 1.0 - su0;
    float b1 = rnd.y * su0;
    return b0 * v0 + b1 * v1 + (1.0 - b0 - b1) * v2;
}

float triangleArea(vec3 v0, vec3 v1, vec3 v2) {
    return 0.5 * length(cross(v1 - v0, v2 - v0));
}

///Power Heuristic for combining two sampling strategies
///Returns the MIS weight for pdf_a 
float powerHeuristic(float pdf_a, float pdf_b) {
    float a2 = pdf_a * pdf_a;
    float b2 = pdf_b * pdf_b;
    float denom = a2 + b2;
    return denom > 0.0 ? a2 / denom : 0.0;
}

///PDF (solid-angle) of a cosine-weighted hemisphere for the diffuse BSDF
///\note Isolated on purpose: GGX only needs to replace this implementation, not the MIS logic
float bsdfPDF_diffuse(vec3 normal, vec3 dir) {
    return max(dot(normal, dir), 0.0) / PI;
}

///Converts triangle-light sampling into a solid-angle measure PDF at the shading point.
///Same formula sampleEmissiveTriangle() uses when it picks the light
float trianglelightPDF(float dist, float cos_light, float area) {
    if(cos_light <= 0.0 || area <= 0.0) return 0.0;
    return (dist * dist) / (cos_light * area * float(light_count));
}

