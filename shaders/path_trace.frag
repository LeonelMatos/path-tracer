#version 460 core

in vec2 vUV;
out vec4 frag_color;

uniform vec2 resolution;
uniform int frame_id;
uniform sampler2D prev_frame;

const float PI  = 3.14159265359;
const float INF = 1e30;
const float EPS = 0.001;
const int DEPTH = 50;

const vec3 camera_position = vec3(0.0, -5.0, 0.0);
const vec3 camera_lookat   = vec3(0.0,  0.0, 0.0);
const vec3 camera_up       = vec3(0.0,  0.0, 1.0);

const vec3 WHITE = vec3(0.90, 0.90, 0.90);
const vec3 RED   = vec3(0.90, 0.05, 0.05);
const vec3 GREEN = vec3(0.05, 0.90, 0.05);

//Adaptado de https://www.scribd.com/document/582697876/Jarzynski2020Hash
vec3 pcg3d(uvec3 v) {
    v = v * 1664525u + 1013904223u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    v ^= v >> 16u;
    v.x += v.y*v.z; v.y += v.z*v.x; v.z += v.x*v.y;
    return vec3(v) * (1.0 / float(0xFFFFFFFFu));
}

vec3 rand3(int bounce) {
    uvec2 px = uvec2(gl_FragCoord.xy);
    uint fid = uint(frame_id);
    return pcg3d(uvec3(
        px.x ^ (fid * 2654435761u),
        px.y ^ (fid * 2246822519u),
        uint(bounce + 1)
    ));
}

mat3 onb(vec3 n) {
    vec3 up = abs(n.x) > 0.9 ? vec3(0,1,0) : vec3(1,0,0);
    vec3 t = normalize(cross(up, n));
    return mat3(t, cross(n, t), n);
}

vec3 cameraRay(vec2 uv) {
    float aspect = resolution.x / resolution.y;
    float f_len = 1.0 / tan(0.5 * 30.0 * PI / 180.0);
    vec2 p = 2.0 * uv - 1.0;
    vec3 ray_cam = vec3(p.x * aspect, p.y, -f_len);

    vec3 cam_z = normalize(camera_position - camera_lookat);
    vec3 cam_x = normalize(cross(camera_up, cam_z));
    vec3 cam_y = cross(cam_z, cam_x);

    return normalize(cam_x * ray_cam.x + cam_y * ray_cam.y + cam_z * ray_cam.z);
}

struct Hit { float t; vec3 pos, normal, albedo, emission; };

/*
 Interseção raio-esfera
    ||o + t·v - c||² = r²
 Expande para
    t²(v·v) + 2t(v·(o-c)) + (||o-c||² - r²) = 0
 Onde
    o = ray_origin
    v = ray_dir
    c = center
    r = radius
 Solução
    t = (-b ±sqrt(b² - 4ac)) / (2a)
 Em que coeficientes são
    a = v·v
    b = 2v·(o - c)
    c = ||o - c||² - r²
 Simplificado para
    b' = v·(o - c)          → float b
    t = -b' ±sqrt(b'² - c)  → float t
 Assumindo que ray_dir está normalizado v·v=1 → a=1
*/
float sphereT(vec3 ray_origin, vec3 ray_dir, vec3 center, float radius) {
    //vetor centro da esfera até origem do raio
    vec3 origin_to_ctr = ray_origin - center;
    
    //projeção do vetor origin_to_ctr na direção ray_dir
    float b = dot(origin_to_ctr, ray_dir);

    //constante da equação quadrática
    float c = dot(origin_to_ctr, origin_to_ctr) - radius * radius;

    float disc = b*b - c;
    if (disc < 0.0) return INF;

    float sqrt_disc = sqrt(disc);

    //soluções de interseção à entrada ou saída da esfera
    // (t>EPS) evita auto-interseção e t negativo
    float t;
    //interseção próxima (entrada da esfera)
    t = -b - sqrt_disc;
    if (t > EPS) return t;

    //interseção longe (saída da esfera)
    t = -b + sqrt_disc;
    return (t > EPS) ? t : INF;
}

/// \todo simplify environment build
bool intersects(vec3 ray_origin, vec3 ray_dir, out Hit h) {
    h.t = INF;
    h.pos = vec3(0);
    h.normal = vec3(0,0,1);
    h.albedo = WHITE;
    h.emission = vec3(0);

    float ray_dist; vec3 hit_p;

    //Definição do ambiente hardcoded//

    //Chão (z=-1), teto (z=+1)
    if (abs(ray_dir.z) > 1e-8) {
        ray_dist = (-1.0 - ray_origin.z) / ray_dir.z;
        if (ray_dist > EPS && ray_dist < h.t) {
            hit_p = ray_origin + ray_dist * ray_dir;
            if (abs(hit_p.x) <= 1.0 && hit_p.y >= -1.0 && hit_p.y <= 1.0) {
                h.t=ray_dist;
                h.pos=hit_p;
                h.normal=vec3(0,0,1);
                h.albedo=WHITE;
                h.emission=vec3(0);
                }
            }
        ray_dist = (1.0 - ray_origin.z) / ray_dir.z;
        if (ray_dist > EPS && ray_dist < h.t) {
            hit_p = ray_origin + ray_dist * ray_dir;
            if (abs(hit_p.x) <= 1.0 && hit_p.y >= -1.0 && hit_p.y <= 1.0) {
                h.t=ray_dist;
                h.pos=hit_p;
                h.normal=vec3(0,0,-1);
                h.albedo=WHITE;
                h.emission=vec3(0);
            }
        }
    }

    //Parede traseira (y=+1)
    if (abs(ray_dir.y) > 1e-8) {
        ray_dist = (1.0 - ray_origin.y) / ray_dir.y;
        if (ray_dist > EPS && ray_dist < h.t) {
            hit_p = ray_origin + ray_dist * ray_dir;
            if(abs(hit_p.x) <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
                h.t = ray_dist;
                h.pos = hit_p;
                h.normal = vec3(0, -1, 0);
                h.albedo = WHITE;
                h.emission = vec3(0);
            }
        }
    }

    //Paredes esquerda (x=-1), direita(x=-1)
    if (abs(ray_dir.x) > 1e-8) {
        ray_dist = (-1.0 - ray_origin.x) / ray_dir.x;
        if (ray_dist > EPS && ray_dist < h.t) {
            hit_p = ray_origin + ray_dist * ray_dir;
            if(hit_p.y >= -1.0 && hit_p.y <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
                h.t = ray_dist;
                h.pos = hit_p;
                h.normal = vec3(1,0,0);
                h.albedo = RED;
                h.emission = vec3(0);
            }
        }
        ray_dist = (1.0 - ray_origin.x) / ray_dir.x;
        if (ray_dist > EPS && ray_dist < h.t) {
            hit_p = ray_origin + ray_dist * ray_dir;
            if(hit_p.y >= -1.0 && hit_p.y <= 1.0 && hit_p.z >= -1.0 && hit_p.z <= 1.0) {
                h.t = ray_dist;
                h.pos = hit_p;
                h.normal = vec3(-1,0,0);
                h.albedo = GREEN;
                h.emission = vec3(0);
            }
        }
        
    }

    // Esfera de luz
    ray_dist = sphereT(ray_origin, ray_dir, vec3(0.0, 0.0, 0.80), 0.20);
    if (ray_dist < h.t) {
        h.t=ray_dist;
        h.pos=ray_origin + ray_dist * ray_dir;
        h.normal=normalize(h.pos-vec3(0,0,0.78));
        h.albedo=vec3(0);
        h.emission=vec3(10.0);
    }

    // Esfera de base
    ray_dist = sphereT(ray_origin, ray_dir, vec3(-0.5, 0.0, -0.65), 0.35);
    if (ray_dist < h.t) {
        h.t=ray_dist;
        h.pos=ray_origin + ray_dist * ray_dir;
        h.normal=normalize(h.pos-vec3(0,0,0.78));
        h.albedo=GREEN;
        h.emission=vec3(0);
    }
    ray_dist = sphereT(ray_origin, ray_dir, vec3(0.3, 0.5, -0.50), 0.50);
    if (ray_dist < h.t) {
        h.t=ray_dist;
        h.pos=ray_origin + ray_dist * ray_dir;
        h.normal=normalize(h.pos-vec3(0,0,0.78));
        h.albedo=RED;
        h.emission=vec3(0);
    }


    return h.t < INF;
}

vec3 pathTrace(vec2 uv) {
    vec3 ro = camera_position;
    vec3 rd = cameraRay(uv);
    vec3 color = vec3(0);
    vec3 throughput = vec3(1);

    for (int b = 0; b < DEPTH; b++) {
        Hit h;
        if (!intersects(ro, rd, h)) break;

        color += throughput * h.emission;
        if (dot(h.emission, h.emission) > 0.0) break;

        vec3 r = rand3(b);
        float cosT = sqrt(r.x);
        float sinT = sqrt(1.0 - r.x);
        float phi = 2.0 * PI * r.y;
        rd = onb(h.normal) * vec3(sinT*cos(phi), sinT*sin(phi), cosT);
        throughput *= h.albedo;
        ro = h.pos + h.normal * EPS;
    }
    return color;
}

void main() {
    vec2 jitter = (rand3(-1).xy - 0.5) / resolution;
    vec3 linear = pathTrace(vUV + jitter);

    if (frame_id == 0) {
        frag_color = vec4(pow(linear, vec3(1.0/2.2)), 1.0);
    } else {
        vec3 prev = pow(texture(prev_frame, vUV).rgb, vec3(2.2));
        vec3 new_avg = mix(prev, linear, 1.0 / float(frame_id + 1));
        frag_color = vec4(pow(new_avg, vec3(1.0/2.2)), 1.0);
    }
}