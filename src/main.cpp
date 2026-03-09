// ============================================================
//  Brute-Force Path Tracer – OpenGL 3.3 / GLSL 330
//
//  Reproduz a mesma cena do shader da aula:
//    • Cornell box analítica (5 paredes axis-aligned)
//    • 1 esfera emissiva junto ao tecto (área de luz, emission=15)
//    • 2 esferas difusas no chão
//    • Mesma câmera: pos=(0,-4.75,0), lookAt=origem, up=Z, FOV=30°
//
//  1 sample/pixel/frame, acumulação progressiva por ping-pong FBOs.
//  ESC sai.
//
//  Build Linux : g++ main.cpp -o pt -lGL -lGLEW -lglfw
//  Build macOS : g++ main.cpp -o pt -lGLEW -lglfw \
//                  -framework OpenGL -DGL_SILENCE_DEPRECATION
// ============================================================

#include <stdio.h>
#include <stdlib.h>
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>

static const int W = 800, H = 800;
