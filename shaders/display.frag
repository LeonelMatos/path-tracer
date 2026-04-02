/**
 * \file display.frag
 * \author Leonel Matos
 * \date 2026-03-24
 * \brief Screen Display Fragment Shader
 * \copyright Copyright (c) 2026
 */

#version 460 core

in vec2 vUV;
out vec4 frag_color;
uniform sampler2D tex;

void main() {
    frag_color = texture(tex, vUV);
}