/**
 * \file display.frag
 * \author Leonel Matos
 * \date 2026-03-24
 * \brief Screen Display Fragment Shader
 * \copyright Copyright (c) 2026
 */

#version 460 core

#include "globals.glsl"

in vec2 vUV;
out vec4 frag_color;
uniform sampler2D tex;

uniform vec2 render_resolution;
uniform vec2 display_resolution;

/**
\note Aces adapted from https://github.com/TheRealMJP/BakingLab/blob/master/BakingLab/ACES.hlsl
*/
vec3 toneMap(vec3 color) {
    switch(TONE_MAPPING) {
        case TM_REINHARD: {
            return color / (color + vec3(1.0));
        }
        case TM_ACES: {
            const mat3 inputMat = mat3(
                0.59719, 0.07600, 0.02840,
                0.35458, 0.90834, 0.13383,
                0.04823, 0.01566, 0.83777
            );
            const mat3 outputMat = mat3(
                1.60475, -0.10208, -0.00327,
                -0.53108,  1.10813, -0.07276,
                -0.07367, -0.00605,  1.07602
            );
            vec3 v = inputMat * color;
            vec3 a = v * (v + 0.0245786) - 0.000090537;
            vec3 b = v * (vec3(0.983729) * v + vec3(0.4329510)) + vec3(0.238081);
            
            return clamp(outputMat * (a/b), 0.0, 1.0);
        }
        default:
            return clamp(color, 0.0, 1.0);
    }
}

void main() {
    vec2 uv = vUV * (render_resolution / display_resolution);
    vec3 linear = texture(tex, uv).rgb * EXPOSURE;
    float alpha = texture(tex, uv).a;
    vec3 tone_map = pow(toneMap(linear), vec3(1.0/2.2));

    //Vignette
    if(VIGNETTE_STRENGTH > 0.0) {
        vec2 centered = vUV - 0.5;
        float dist = length(centered) * 1.4142135; //normalized for the corners
        float vignette = 1.0 - VIGNETTE_STRENGTH * dist * dist;
        tone_map *= clamp(vignette, 0.0, 1.0);
    }

    vec3 bg = vec3(1.0);
    vec3 composited = mix(bg, tone_map, alpha);
    frag_color = vec4(composited, 1.0);
}