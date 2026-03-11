// ============================================================
//  Brute-Force Path Tracer
//
// Leonel Matos
//
//  Reproduz a mesma cena do shader da aula:
//    • Cornell box analítica (5 paredes axis-aligned)
//    • 1 esfera emissiva junto ao tecto (área de luz, emission=15)
//    • 2 esferas difusas no chão
//    • Mesma câmera: pos=(0,-4.75,0), lookAt=origem, up=Z, FOV=30°
//
//  1 sample/pixel/frame, acumulação progressiva 
// ============================================================

/* ----------------------------------------------
    Brute-Force Path Tracer
    Leonel Matos 48284

    Recriar a Cornell box com emissive sphere, base no exemplo de 
    Image Synthesis (Summer Term 2026) Prof. Dr. Thorsten Thormählen
    Capítulo 3.2 Path Tracing, Brute-Force Evaluation of the Rendering Equation (8/32)
    (https://www.uni-marburg.de/en/fb12/research-groups/grafikmultimedia/lectures/graphics2)
    Adaptado para OpenGL.

    Cornell box, 1 esfera emissiva perto do topo, 2 esferas no chão, acumulação progressiva de frames

    Dependências
    shaders/
        common
        display
        path_trace
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>

static const int W = 1000, H = 1000;



