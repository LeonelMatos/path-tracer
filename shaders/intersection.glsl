///General plane function using normal vector to give facing direction
///\note needs dot product for normal orientation, slower.
float planeT(Ray ray, vec3 normal, float offset) {
   float denom = dot(normal, ray.direction);
   if (abs(denom) < 1e-8) return INF;
   float t = (offset - dot(normal, ray.origin)) / denom;
   return (t > EPS) ? t : INF;
}

///Specific Z-axis plane, with only offset
///\note More optimized than planeT that needs extra normal vec operations
float planeT_Z(Ray ray, float offset) {
   if (abs(ray.direction.z) < 1e-8) return INF;
   float t = (offset - ray.origin.z) / ray.direction.z;
   return (t > EPS) ? t : INF;
}

///Specific X-axis plane, with only offset
float planeT_X(Ray ray, float offset) {
   if (abs(ray.direction.x) < 1e-8) return INF;
   float t = (offset - ray.origin.x) / ray.direction.x;
   return (t > EPS) ? t : INF;
}

///Specific Y-axis plane, with only offset
float planeT_Y(Ray ray, float offset) {
   if (abs(ray.direction.y) < 1e-8) return INF;
   float t = (offset - ray.origin.y) / ray.direction.y;
   return (t > EPS) ? t : INF;
}


/**
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
 Assumindo que ray.direction está normalizado v·v=1 → a=1
*/
float sphereT(Ray ray, vec3 center, float radius) {
    //vetor centro da esfera até origem do raio
    vec3 origin_to_ctr = ray.origin - center;
    
    //projeção do vetor origin_to_ctr na direção ray.direction
    float b = dot(origin_to_ctr, ray.direction);

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

/**AABBIntersect from https://alelievr.github.io/Modern-Rendering-Introduction/AABBIntersection/ \n
   Also based on knightcrawler25's GLSL-PATHTRACER.
   But applied OBB for rotation.
   Rotation is calculated from pitch and yaw for simplification
*/
float boxT(Ray ray, vec3 center, vec3 half_size, float pitch, float yaw, out vec3 out_normal) {

   //calculate local axis from angles
   float c_yaw = cos(radians(yaw)), s_yaw = sin(radians(yaw));
   float c_pitch = cos(radians(pitch)), s_pitch = sin(radians(pitch));
   vec3 axis_x = vec3(c_yaw, s_yaw, 0);
   vec3 axis_y = vec3(-s_yaw * c_pitch, c_yaw * c_pitch, s_pitch);
   vec3 axis_z = vec3(s_yaw * s_pitch, c_yaw * s_pitch, c_pitch);

   //ray to OBB local space
   vec3 local_origin = vec3(
      dot(ray.origin - center, axis_x),
      dot(ray.origin - center, axis_y),
      dot(ray.origin - center, axis_z)
   );
   vec3 local_dir = vec3(
      dot(ray.direction, axis_x),
      dot(ray.direction, axis_y),
      dot(ray.direction, axis_z)
   );

   //AABB intersect box
   
   vec3 inv_dir = 1.0 / local_dir;
   vec3 t_min = (-half_size - local_origin) * inv_dir;
   vec3 t_max = (half_size - local_origin) * inv_dir;

   vec3 t_near = min(t_min, t_max);
   vec3 t_far = max(t_min, t_max);

   float t0 = max(t_near.x, max(t_near.y, t_near.z));
   float t1 = min(t_far.x, min(t_far.y, t_far.z));

   if (t1 < t0 || t1 < EPS) return INF;
   float t = (t0 > EPS) ? t0 : t1;

   //normal in local space, then converts to world space
   vec3 hit_local = local_origin + t * local_dir;
   vec3 abs_hit = abs(hit_local) / half_size;

   vec3 local_normal;
   if (abs_hit.x > abs_hit.y && abs_hit.x > abs_hit.z)
      local_normal = vec3(sign(hit_local.x), 0, 0);
   else if (abs_hit.y > abs_hit.z)
      local_normal = vec3(0, sign(hit_local.y), 0);
   else 
      local_normal = vec3(0, 0, sign(hit_local.z));

      out_normal = normalize(local_normal.x * axis_x + local_normal.y * axis_y + local_normal.z * axis_z);

      return t;
}

/**Möller-Trumbore ray-triangle intersect.
*/
float triangleT(Ray ray, vec3 v0, vec3 v1, vec3 v2, out vec3 out_normal, out vec3 out_bary) {
   vec3 edge1 = v1 - v0;
   vec3 edge2 = v2 - v0;
   vec3 h = cross(ray.direction, edge2);
   float a = dot(edge1, h);

   if (abs(a) < EPS_TRI) return INF;

   float f = 1.0 / a;
   vec3 s = ray.origin - v0;
   float u = f * dot(s, h);
   if (u < 0.0 || u > 1.0) return INF;

   vec3 q = cross(s, edge1);
   float v = f * dot(ray.direction, q);
   if (v < 0.0 || u + v > 1.0) return INF;

   float t = f * dot(edge2, q);
   if (t < EPS_TRI) return INF;

   out_normal = normalize(cross(edge1, edge2));
   if (dot(ray.direction, out_normal) > 0.0)
      out_normal = -out_normal;
   out_bary = vec3(u, v, 1.0 - u - v);
   return t;
}

float triangleT(Ray ray, vec3 v0, vec3 v1, vec3 v2, out vec3 out_normal) {
   vec3 bary;
   return triangleT(ray, v0, v1, v2, out_normal, bary);
}
