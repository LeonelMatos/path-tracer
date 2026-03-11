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

#include "common/shader.hpp"

using namespace std;

//Global Variables
GLuint program_id;
GLFWwindow* window;

static const int WINDOW_WIDTH = 1000, WINDOW_HEIGHT = 1000;

bool transferDataToGPU(void);
void cleanupDataFromGPU();
void draw(void);

int main(void) {
    if (!glfwInit()) { fprintf(stderr, "Failed to init GLFW\n"); return -1; }
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Path Tracer - Brute-Force", NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);

    glewExperimental = GL_TRUE;
    glewInit();

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    if(!transferDataToGPU())
        return -1;

    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
        
        draw();
    }
    cleanupDataFromGPU();
    glfwTerminate();

    return 0;
}

bool transferDataToGPU(void) {
    program_id = LoadShaders({
        { GL_VERTEX_SHADER,   "shaders/common.vertexshader"    },
        { GL_FRAGMENT_SHADER, "shaders/display.fragmentshader" },
    });
    if(!program_id) { glfwTerminate(); return false; }
    return true;
}

void cleanDataFromGPU() {
    glDeleteProgram(program_id);
}

void draw(void) {

}