
float planeT(vec3 ray_origin, vec3 ray_direction,
                      vec3 plane_normal, float plane_offset) {
    float denom = dot(plane_normal, ray_direction);
    if (abs(denom) < 1e-8) return INF;
    float t = (plane_offset - dot(plane_normal, ray_origin)) / denom;
    return (t > EPS) ? t : INF;
}

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