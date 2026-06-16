/**
 * \file path_trace.frag
 * \author Leonel Matos
 * \date 2026-03-24
 * \brief Path Tracer Main
 * \copyright Copyright (c) 2026
 */

#version 460 core

in vec2 vUV;
out vec4 frag_color;

uniform vec2 resolution;
uniform int frame_id;
uniform sampler2D prev_frame;

#include "globals.glsl"
#include "sampling.glsl"
#include "intersection.glsl"
#include "scene.glsl"
#include "path_trace_core.glsl"

void main() {
    vec4 linear = vec4(0);
    uvec2 px = uvec2(gl_FragCoord.xy);

    for (int s = 0; s < SAMPLES_PER_PIXEL; s++) {
        linear += pathTrace(vUV, s, px);
    }
    linear /= float(SAMPLES_PER_PIXEL);

    vec4 accumulated;
    if (frame_id == 0) {
        accumulated = linear;
    } else {
        vec4 prev = texture(prev_frame, vUV);
        accumulated = mix(prev, linear, 1.0 / float(frame_id + 1));
    }
    frag_color = accumulated;
}