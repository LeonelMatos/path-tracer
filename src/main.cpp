/**
 * \file main.cpp
 * \author Leonel Matos
 * \date 2026-03-24
 * \brief Main code
 * \copyright Copyright (c) 2026
 */
/*
    Path Tracer
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
#include <vector>
#define GLEW_NO_GLU
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "mesh.hpp"
#include "common/shader.hpp"

#define VERSION "1.0.3"

using namespace std;
using namespace glm;

/*----------------------------------------------------------
  Global Variables
*/
GLuint program_id;
GLuint pathtr_id;
GLFWwindow* window;

static const int WINDOW_WIDTH = 1000, WINDOW_HEIGHT = 1000;

#define WINDOW_TITLE "Path Tracer"

const int V_SYNC = 0;
const uint MAX_SAMPLES = 10;

GLuint tex[2], fbo[2];
GLuint vao;

GLint loc_res, loc_frame, loc_prev, loc_tex;

int frame_id = 0;
int cur_f = 0, prev_f = 1;

//Time metrics
double start_time = 0.0;
uint total_frames = 0;

const char* txt_sep = "----------------------------";

/*----------------------------------------------------------
  GPU
*/

GLuint triangle_ssbo;
GLuint material_ssbo;


/*----------------------------------------------------------
  Function Declarations
*/
void formatTime(double seconds, char*buf, int buf_size);
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

    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    if (!window) { glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(V_SYNC);
    
    glewExperimental = GL_TRUE;
    glewInit();
    
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    
    if(!transferDataToGPU())
    return -1;
    
    printf("%s\n%s v%s\nResolution: %dx%d\n\nPress ESC to quit\n%s\n", txt_sep, WINDOW_TITLE, VERSION, WINDOW_WIDTH, WINDOW_HEIGHT, txt_sep);

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

void formatTime(double seconds, char*buf, int buf_size) {
    if (seconds < 60.0)
        snprintf(buf, buf_size, "%.1fs", seconds);
    else {
        int min = (int)(seconds/60.0);
        float sec = seconds - (min * 60.0);
        snprintf(buf, buf_size, "%dm%.1fs", min, sec);
    }
}

//----------------------------------------------------------

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

    ///\TODO Add uniform verifications

    //Textures DSA
    glCreateTextures(GL_TEXTURE_2D, 2, tex);
    for (int i = 0; i < 2; i++) {
        glTextureStorage2D(tex[i], 1, GL_RGBA32F, WINDOW_WIDTH, WINDOW_HEIGHT);
        glTextureParameteri(tex[i], GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTextureParameteri(tex[i], GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTextureParameteri(tex[i], GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTextureParameteri(tex[i], GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }

    //FBO DSA ping-pong
    glCreateFramebuffers(2, fbo);
    for (int i = 0; i < 2; i++) {
        glNamedFramebufferTexture(fbo[i], GL_COLOR_ATTACHMENT0, tex[i], 0);
        if (glCheckNamedFramebufferStatus(fbo[i], GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "FBO %d incomplete. Check FBO DSA implementation.\n", i);
            return false;
        }
    }

    glCreateVertexArrays(1, &vao);
    glBindVertexArray(vao);

    auto test_triangles = makeTestMesh();
    vector<GPUMaterial> test_materials = {{{1.0f, 1.0f, 1.0f, 1}, {0,0,0,0}, 0, 0, {0,0}}};

    vector<GPUTriangle> tris; vector<GPUMaterial> mats;

    mat4 transform = translate(mat4(1.0f), vec3(0, 0, -1));
    transform = scale(transform, vec3(0.01f));
    transform = rotate(transform, radians(90.0f), vec3(1, 0, 0));
    //transform = rotate(transform, radians(180.0f), vec3(0, 1, 0));

    loadMesh("../models/stanford_bunny_pbr/scene.gltf", tris, mats, transform);
    for (auto& mat : mats) {
        mat.albedo = vec4(0.8f, 0.3f, 0.1f, 1.0f);  // laranja
        mat.type = 0;
    }
    uploadMesh(tris, mats, triangle_ssbo, material_ssbo);

    printf("sizeof GPUMaterial: %zu\n", sizeof(GPUMaterial));
    printf("sizeof GPUTriangle: %zu\n", sizeof(GPUTriangle));
    printf("offsetof material_id: %zu\n", offsetof(GPUTriangle, material_id));

    return true;
}

void cleanDataFromGPU() {
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(2, tex);
    glDeleteFramebuffers(2, fbo);
    glDeleteProgram(pathtr_id);
    glDeleteProgram(program_id);

    glDeleteBuffers(1, &triangle_ssbo);
    glDeleteBuffers(1, &material_ssbo);
    
}

void display(void) {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glUseProgram(program_id);
    
    glBindTextureUnit(0, tex[cur_f]);
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
    glBindFramebuffer(GL_FRAMEBUFFER, fbo[cur_f]);
    glViewport(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
    glUseProgram(pathtr_id);
    glUniform2f(loc_res, (float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
    glUniform1i(loc_frame, frame_id);
    glBindTextureUnit(0, tex[prev_f]);
    glUniform1i(loc_prev, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    int tmp = cur_f; cur_f = prev_f; prev_f = tmp;
    frame_id++;

    if(frame_id >= MAX_SAMPLES)
        printf("\n%s\nRender complete - %d samples in %.1fs\n", txt_sep, frame_id, time_elapsed);
    
    //Step 2 Display : accumulated texture to screen
    display();

    // Metrics
    if (frame_id % 10 == 0) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);

        time_now = ts.tv_sec + ts.tv_nsec * 1e-9;
        time_elapsed = time_now - start_time;
        
        double fps = frame_id / time_elapsed;
        double samples_per_s = (double)frame_id * WINDOW_WIDTH * WINDOW_HEIGHT / time_elapsed;
        double ms_frame = time_elapsed / frame_id * 1000.0;
        char time_buf[32];
        formatTime(time_elapsed, time_buf, sizeof(time_buf));

        printf("\rSamples/pixel: %d | FPS: %.1f | %.1fms/frame | %.1f | Time:%s",
            frame_id, fps, ms_frame, samples_per_s / 1e6, time_buf);
        fflush(stdout);
    }
}