
//Adapted from https://www.scribd.com/document/582697876/Jarzynski2020Hash
vec3 pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    v ^= v >> 16u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    return vec3(v) * (1.0 / float(0xFFFFFFFFu));
}

///\note using frame_id as seed gives same output
vec3 rand3(int bounce, int seed) {
    uvec2 px = uvec2(gl_FragCoord.xy);
    uint fid = uint(frame_id);
    uint sid = uint(seed);
    return pcg3d(uvec3(
        px.x ^ (fid * 2654435761u) ^ (sid * 3266489917u),
        px.y ^ (fid * 2246822519u) ^ (sid * 2246822519u),
        uint(bounce + 1)
    ));
}

mat3 onb(vec3 n) {
    vec3 up = abs(n.x) > 0.9 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 t = normalize(cross(up, n));
    return mat3(t, cross(n, t), n);
}