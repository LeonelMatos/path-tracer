/**
 * \file main.cpp
 * \author Leonel Matos
 * \date 2026-03-24
 * \brief Main code
 * \copyright Copyright (c) 2026
 */
/*
    Brute-Force Path Tracer
    Leonel Matos 48284

    Recriar a Cornell box com emissive sphere, base no exemplo de 
    Image Synthesis (Summer Term 2026) Prof. Dr. Thorsten Thormählen
    Capítulo 3.2 Path Tracing, Brute-Force Evaluation of the Rendering Equation (8/32)
    (https://www.uni-marburg.de/en/fb12/research-groups/grafikmultimedia/lectures/graphics2)

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
#include <time.h>
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "common/shader.hpp"

#define VERSION "1.0.1"

using namespace std;

/*----------------------------------------------------------
  Global Variables
*/
GLuint program_id;
GLuint pathtr_id;
GLFWwindow* window;

static const int WINDOW_WIDTH = 700, WINDOW_HEIGHT = 700;

const int V_SYNC = 0;
const uint MAX_SAMPLES = 5000;

GLuint tex[2], fbo[2];
GLuint vao;

GLint loc_res, loc_frame, loc_prev, loc_tex;

int frame_id = 0;
int cur = 0, prev = 1;

//Time metrics
double start_time = 0.0;
uint total_frames = 0;

const char* txt_sep = "----------------------------";

/*----------------------------------------------------------
  Function Declarations
*/
bool transferDataToGPU(void);
void cleanDataFromGPU();
void display(void);
void draw(void);

//----------------------------------------------------------
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
    glfwSwapInterval(V_SYNC);

    glewExperimental = GL_TRUE;
    glewInit();

    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    if(!transferDataToGPU())
        return -1;

    printf("%s\nPathTracer v%s\nResolution: %dx%d\n\nPress ESC to quit\n%s\n", txt_sep, VERSION, WINDOW_WIDTH, WINDOW_HEIGHT, txt_sep);

    //Time init
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    start_time = ts.tv_sec + ts.tv_nsec * 1e-9;

    while (!glfwWindowShouldClose(window) && glfwGetKey(window, GLFW_KEY_ESCAPE) != GLFW_PRESS) {
        //Suspend the rendering after completion to avoid useless GPU processing
        if (MAX_SAMPLES > 0 && frame_id >= MAX_SAMPLES) {
            glfwWaitEvents(); //Gets input events and avoids program freezing
            continue;
        }
        draw();
    }
    cleanDataFromGPU();
    glfwTerminate();

    return 0;
}

bool transferDataToGPU(void) {
    program_id = LoadShaders({
        { GL_VERTEX_SHADER,   "shaders/common.vert"    },
        { GL_FRAGMENT_SHADER, "shaders/display.frag" },
    });
    pathtr_id = LoadShaders({
        { GL_VERTEX_SHADER,   "shaders/common.vert"       },
        { GL_FRAGMENT_SHADER, "shaders/path_trace.frag" },
    });
    if(!program_id || !pathtr_id)  { glfwTerminate(); return false; }

    loc_res = glGetUniformLocation(pathtr_id, "resolution");
    loc_frame = glGetUniformLocation(pathtr_id, "frame_id");
    loc_prev = glGetUniformLocation(pathtr_id, "prev_frame");
    loc_tex = glGetUniformLocation(program_id, "tex");

    //TODO Add uniform verifications

    //FBO ping-pong
    glGenTextures(2, tex);
    glGenFramebuffers(2, fbo);

    for (int i = 0; i < 2; i++) {
        glBindTexture(GL_TEXTURE_2D, tex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        glBindFramebuffer(GL_FRAMEBUFFER, fbo[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex[i], 0);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    return true;
}

void cleanDataFromGPU() {
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(2, tex);
    glDeleteFramebuffers(2, fbo);
    glDeleteProgram(pathtr_id);
    glDeleteProgram(program_id);
    
}

void display(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glUseProgram(program_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[cur]);
    glUniform1i(loc_tex, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glfwSwapBuffers(window);
    glfwPollEvents();

}
/*----------------------------------------------------------
  Draw to GPU
*/
void draw(void) {
    double time_now, time_elapsed;
    
    //Step 1 Path Tracing: to current FBO
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[cur]);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glUseProgram(pathtr_id);
    glUniform2f(loc_res, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
    glUniform1i(loc_frame, frame_id);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex[prev]);
    glUniform1i(loc_prev, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    int tmp = cur; cur = prev; prev = tmp;
    frame_id++;

    if(frame_id >= MAX_SAMPLES)
        printf("\n%s\nRender complete - %d samples/pixel in %.1fs\n", txt_sep, frame_id, time_elapsed);
    
    //Step 2 Display : accumulated texture to screen
    display();

    // Metrics
    if (frame_id % 60 == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        time_now = ts.tv_sec + ts.tv_nsec * 1e-9;
        time_elapsed = time_now - start_time;
        
        double fps = frame_id / time_elapsed;
        double samples_per_s = (double)frame_id * WINDOW_WIDTH * WINDOW_HEIGHT / time_elapsed;
        double ms_frame = time_elapsed / frame_id * 1000.0;

        printf("\rSamples/pixel: %d | FPS: %.1f | %.1fms/frame | %.1f | Time:%.1fs",
            frame_id, fps, ms_frame, samples_per_s / 1e6, time_elapsed);
        fflush(stdout);
    }
}