\mainpage Path-Tracer

# Path Tracer

A GPU-accelerated **path tracer** written in OpenGL 4.6 and C++17.
It renders physically-based global illumination interactively using compute shaders, 
BVH structure, Next Event Estimaation and Intel Open Image Denoise.

## Overview

The project is organized around these main files:

- **main.cpp**: window, input, main render loop, the DearImGui interface, denoiser, sheduler and screenshot, around other auxiliary functions;
- **mesh.cpp**: model import with ASSIMP, the GPU-sied vertex, triangles, materials
 and light structures along with their respective SSBO uploads;
- **bvh.cpp**: constructs and uploads the bounding volume hierarchy for the ray/triangle acceleration;
- **texture.cpp**: HDRI environment loading and the materials texture array;
- **shader.cpp**: runtime GLSL compilation and linking, also implemented the `#include` behaviour;
- **denoiser.cpp**: a simple wrapper around OIDN;
- **loader.hpp**: the buffers used to hand an asynchronously-loaded model from a secundary thread back to the render thread;
- **config.hpp**: All that is render, camera and renderer configuration structs with organized parameters.

## How The Render Loop Works

Each frame runs four steps:
1. A **path-tracing** pass (compute/fragment);
2. An optional **denoise** at scheduled near-end checkpoints;
3. A **display** pass that adds filters to the render and sends the accumulated texture to screen;


![example](@ref example.png)

