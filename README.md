
# Path Tracing in Games and Virtual Environments 

A GPU-accelerated path tracer written in OpenGL 4.6. Built as a final year project at the Universidade da Beira Interior.

Renders physically-based global illumination interactively using an OpenGL compute shader.

![preview1](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxLzVlMzIzZmUyLTNhMTMtNDM5ZS1iYTA5LTk1NTE1YjViOWViOC9yZW5kZXJfMjAyNjA3MDFfMjMxNzM4Xzcwcy5wbmc_dGFibGU9YmxvY2smaWQ9MzkwM2IyMDYtODZlYy04MDI3LTg4NzgtZTgyM2QyN2Y4OTk5JnNwYWNlSWQ9MDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxJmV4cGlyYXRpb25UaW1lc3RhbXA9MTc4MzAwODAwMDAwMCZzaWduYXR1cmU9UHRlUTRjNW5HaXhrY01udGZMVTNnRnJVR05XMVVrb0lMQzFMX2c5YTBqdw==.png?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)
![preview2](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxLzEzMTg1NDZhLTIxZWYtNDU5YS1iNmNjLTBlNTc3ZWMzMmRhYi9yZW5kZXJfMjAyNjA2MjlfMDA1NzA0XzQwcy5wbmc_dGFibGU9YmxvY2smaWQ9MzkwM2IyMDYtODZlYy04MDk0LThiNGQtZjBmNzlhZmEzYzhkJnNwYWNlSWQ9MDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxJmV4cGlyYXRpb25UaW1lc3RhbXA9MTc4MzAwODAwMDAwMCZzaWduYXR1cmU9S2ZvZ09qa2Jqc2FTTjZjLXNRM0pUbUpveWxpeDBmMm90dnJXaU40SUh5bw==.png?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)
![preview3](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxLzY5Mzc4NDU3LTUxMGItNDkwZS1iNWU4LWZiY2RhZDZhYjQwMy9XaGF0c0FwcF9JbWFnZV8yMDI2LTA2LTI4X2F0XzE5LjE1LjAwLmpwZWc_dGFibGU9YmxvY2smaWQ9MzhlM2IyMDYtODZlYy04MGEyLWI4YWUtZTNkYjI3MzIwYTlkJnNwYWNlSWQ9MDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxJmV4cGlyYXRpb25UaW1lc3RhbXA9MTc4Mjc3MDQwMDAwMCZzaWduYXR1cmU9UGxpMG5IbjlQcmpsTmE1ak1kWjJHbzc5OHNURGhscVV6MjBkUld1Yzc1UQ==.jpeg?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)

## Features

- Built with compute shader, but with a fallback to fragment shader
- BVH acceleration (median-split on longest axis) with heatmap debug view
- Material types: diffuse, mirror, glass, tinted glass, shadow catcher
- Next Event Estimation (NEE) with point, directional and spot lights, and emissive triangles
- Reinhard and ACES tone mapping
- Depth of Field with focal-plane view
- HDRI environment maps
- Intel OIDN 2.5.0 denoiser
- Model loader using ASSIMP

---

## Requirements

### Hardware

A GPU with OpenGL 4.6

Tested on an RTX 3060 (12GB) and an AMD Ryzen 7 Radeon iGPU (512MB). It is reccomended to enable Tile Dispatch on low-VRAM/integrated GPUs to avoid driver timeouts.

## Software

The program runs on Linux.

System packages installed via `apt`:

| Library   | Package         | Purpose             |
|-----------|-----------------|---------------------|
| GLFW      | `libglfw3-dev`  | Window + Input      |
| GLEW      | `libglew-dev`   | OpenGL extension    |
| GLM       | `libglm-dev`    | Math                |
| ASSIMP    | `libassimp-dev` | Model import        |
|pkg-config | `pkg-config`    | Build configuration |

Also required, but can be installed automatically using `setup.sh`:

**Intel Open Image Denoise 2.5.0** (downloads to `/opt/oidn`)

The project is bundled with (no need to install): **Dear ImGui**, **stb** and **OIDN**.

---

## Quick Start

Before running the program run the script `setup.sh` to install and check for all
dependencies:

```bash
chmod +x setup.sh
./setup.sh
```

Create the build directory and build the project:

```bash
mkdir build
cd build
cmake ..
make
```

Now you can run the program in `/build`:

```bash
./pathtracer
```

## Assets Location

The program scans these folder recursively, so subfolders also work.

**Models → models/**

Supported extensions: .gltf, .glb, .obj, .fbx

In the program, open the **Scene Editor → Model Explorer**, press **Scan** and select the model to load.
Keep textures in the same folder as the model or related to is as it came originally.

**HDRI Environment Maps → hdri/**

Supported extensions: .hdr, .exr

In the program, open **Render Inspector → Environment → Select Texture**, pick a map and click **Enable Env Map**.

---

## Controls

| Input                     | Action                                   |
|---------------------------|------------------------------------------|
| `W` `A` `S` `D`           | Move camera                              |
| `Left Shift` (held)       | Move faster                              |
| Left click + drag         | Look around (FPS-style)                  |
| `Shift` + mouse scroll    | Increase / decrease camera speed         |
| `H` or `0`                | Return camera to the home position       |
| `R`                       | Reset accumulation                       |
| `E`                       | Toggle background (black / white)        |
| `F11`                     | Toggle fullscreen                        |
| `F12`                     | Save a screenshot                        |
| `Esc`                     | Quit                                     |

The full interface (render settings, materials, lighting, denoiser, presets) is documented in **[docs/USAGE.md](docs/USAGE.md)**.

## Screenshots

Press **F12** (or the **Take Screenshot** button). Images are written to the `build/` directory.

---

## Project layout

```
path-tracer
├── assets/      Images used by the docs (example, logo)
├── common/     Shader run-time loader
├── docs_md/     Doxygen main page and extra pages
├── docs_style/ Doxygen Awesome CSS theme
├── hdri/        HDRI environment maps (.hdr, .exr)   ← put yours here
├── include/    Bundled libraries and headers
├── models/      3D models (.gltf, .glb, .obj, .fbx)  ← put yours here
├── shaders/    GLSL shaders (compute, fragment, vertex)
└── src/         C++ sources and headers
```

## Documentation

Generate HTML pages with Doxygen with the command:

```bash
    doxygen Doxyfile
```

Output is written to `docs/html`; open with

```bash
    open docs/html/index.html
```

## Gallery

![img1](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxL2NjMzVlZmRiLTM3NjItNGU2Ni05NmY5LTIxMGY4MWZlNDFlNy9XaGF0c0FwcF9JbWFnZV8yMDI2LTA2LTI4X2F0XzE5LjE1LjAwKDEpLmpwZWc_dGFibGU9YmxvY2smaWQ9MzhlM2IyMDYtODZlYy04MDE1LTkyYmYtZGI4YjNiOTMwZjAyJnNwYWNlSWQ9MDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxJmV4cGlyYXRpb25UaW1lc3RhbXA9MTc4Mjc3MDQwMDAwMCZzaWduYXR1cmU9S1JacFUtUlZzdzh4NWdPbmVGZW1BbVpGQW5DbEdFMHhxenlyNlFwOFBtUQ==.jpeg?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)

![img3](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxL2MzODAzNGNiLTBmM2QtNDg1MS1iZmQ4LWMxMDM1M2Y2NzJlZS9XaGF0c0FwcF9JbWFnZV8yMDI2LTA2LTI5X2F0XzAwLjA5LjE3KDEpLmpwZWc_dGFibGU9YmxvY2smaWQ9MzhlM2IyMDYtODZlYy04MDQ5LTgyOTQtZmFjOGM2ZDY0YWFmJnNwYWNlSWQ9MDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxJmV4cGlyYXRpb25UaW1lc3RhbXA9MTc4Mjc3MDQwMDAwMCZzaWduYXR1cmU9SmtLVWt4U21sUE92OUQwS1dfa3p6ZktQajJBM2ZhYl9EMGJaV25KcFFmcw==.jpeg?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)

![img4](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxL2RhY2JmOWFlLWE1NjItNGZlNS1iZmE0LWU3ZDI2OTg4NWM0Zi9yZW5kZXJfMjAyNjA1MjRfMjEyNzMwXzUwMHMucG5nP3RhYmxlPWJsb2NrJmlkPTM4ZTNiMjA2LTg2ZWMtODBhMy1hNGI2LWU3NTJlYzFiZDIwYSZzcGFjZUlkPTA2NTNkZDU3LTMxN2MtNGY1Yy1iMGM1LWQ0Y2ZkZTJlMTZmMSZleHBpcmF0aW9uVGltZXN0YW1wPTE3ODI3NzA0MDAwMDAmc2lnbmF0dXJlPWNUT0ppQUlVSThtVS1OeEgtdXQ5cjJONWl3cEM1RkVVQnJFTXh0SXBvbmc=.png?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)

![img5](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxLzhlOGZlYWE5LTRlODctNGQ1Mi04YzQ0LTFhYTllYmIxMWE0NC9yZW5kZXJfMjAyNjA1MjZfMjMxMDA2XzExMXMucG5nP3RhYmxlPWJsb2NrJmlkPTM4ZTNiMjA2LTg2ZWMtODA5ZC1iMGVhLWU1NWJkNWM3MDg5MSZzcGFjZUlkPTA2NTNkZDU3LTMxN2MtNGY1Yy1iMGM1LWQ0Y2ZkZTJlMTZmMSZleHBpcmF0aW9uVGltZXN0YW1wPTE3ODI3NzA0MDAwMDAmc2lnbmF0dXJlPUoxVGtrMjlqczRkaktUTjB6OEF5Z1FaNGVrLXFudUJ0OHBJUGYyQzBxWUU=.png?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)

![img6](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxLzcwZWJhODhlLTkxMzctNDA4Yi04MGY5LTJhYmI4YWQ2NzFlNy9XaGF0c0FwcF9JbWFnZV8yMDI2LTA2LTI5X2F0XzAwLjA5LjE3KDIpLmpwZWc_dGFibGU9YmxvY2smaWQ9MzhlM2IyMDYtODZlYy04MDBmLWEwNjYtZjY4NGU1OTJhMGE5JnNwYWNlSWQ9MDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxJmV4cGlyYXRpb25UaW1lc3RhbXA9MTc4Mjc3MDQwMDAwMCZzaWduYXR1cmU9Y0RfNkZKN29CWC1lbW9BYkFXdHRqRnpIYmpPNEdWaHpLdlE2RUJfWVBMSQ==.jpeg?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)

![img7](https://image-forwarder.notaku.so/aHR0cHM6Ly9maWxlLm5vdGlvbi5zby9mL2YvMDY1M2RkNTctMzE3Yy00ZjVjLWIwYzUtZDRjZmRlMmUxNmYxLzcwNGQ5ZjE4LTc3MmEtNGI1MS1iNDIwLTQ2OTBjMTczMWExZS9yZW5kZXJfMjAyNjA3MDFfMTEwNTU2XzEwMHMucG5nP3RhYmxlPWJsb2NrJmlkPTM5MDNiMjA2LTg2ZWMtODA3Ni1hMDcwLWZjMzI2Mzc4YmNjOCZzcGFjZUlkPTA2NTNkZDU3LTMxN2MtNGY1Yy1iMGM1LWQ0Y2ZkZTJlMTZmMSZleHBpcmF0aW9uVGltZXN0YW1wPTE3ODI5Mjg4MDAwMDAmc2lnbmF0dXJlPWNTbElsS1QyNzQ4amhGaVhHNEtzaEZ1bWlNbjN4bi1HR1UtR2R6YXFDZjQ=.png?workspaceId=0653dd57-317c-4f5c-b0c5-d4cfde2e16f1)

## Authors

- [Leonel Oliveira Matos](https://github.com/LeonelMatos)
- Prof. Doutor Abel J.P. Gomes

