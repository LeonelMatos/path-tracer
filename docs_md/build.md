# Building The Path Tracer

This page covers the dependencies, what `setup.sh` does, how to build manually and the most common problems and their fixes.

---

## Supported Platform

The project targets **Linux, Ubuntu 26.04**. It uses `apt`/`dpkg` for system packages,
`/etc/ld.so.conf.d` for the OIDN runtime path, and a CMake symlink for live shader reloading.
Other distributions work too, but you'll need to install the equivalent packages and adjust `setup.sh` accordingly.

A GPU and driver supporting **OpenGL 4.6** with **compute shaders** is required.

---

## Dependencies

### System Packages

| Library | Package         | Checked by CMake |
|---------|-----------------|------------------|
| GLFW 3  | `libglfw3-dev`  | `glfw3`          |
| GLEW    | `libglew-dev`   | `GLEW`           |
| GLM     | `libglm-dev`    | `glm`            |
| ASSIMP  | `libassimp-dev` | `assimp`         |
| OpenGL  | (gpu-driver)    | `OpenGL`         |

Install them all at once:

```bash
sudo apt install libglfw3-dev libglew-dev libglm-dev libassimp-dev pkg-config
```

### Intel Open Image Denoise (OIDN) 2.5.0

OIDN is not an `apt` package, it is downloaded as a prebuilt release and installed to
`/opt/oidn`. `setup.sh` handles this, but you can also download it by hand:

```bash
wget https://github.com/OpenImageDenoise/oidn/releases/download/v2.5.0/oidn-2.5.0.x86_64.linux.tar.gz
tar -xzf oidn-2.5.0.x86_64.linux.tar.gz
sudo mv oidn-2.5.0.x86_64.linux /opt/oidn
echo "/opt/oidn/lib" | sudo tee /etc/ld.so.conf.d/oidn.conf
sudo ldconfig
```
> The exact version matters. CMake looks for
> `${OIDN_ROOT}/lib/cmake/OpenImageDenoise-2.5.0`, and the build copies the
> `2.5.0` shared objects into `build/lib/`. If you use a different OIDN release,
> update the version string in both `CMakeLists.txt` and `setup.sh`.

### Already Installed With Project

These inside the repository under `include/` and `common/`:

- **Dear ImGui** — the control panels (compiled directly into the executable).
- **stb** — `stb_image.h`, `stb_image_write.h`, `stb_image_resize2.h`.
- **OpenImageDenoise headers** — `include/OpenImageDenoise/`.

---

## Running

Always launch from inside `build/`:

```bash
cd build
./pathtracer
```

The working directory matters because the program uses **relative paths**:

- Shaders load from `shaders/...` (the symlink inside `build/`).
- The Model Explorer scans `../models`.
- The HDRI selector scans `../hdri`.

Running from elsewhere means it won't find your models, HDRIs or shaders.


## Choosing the shader path (compute vs fragment)

The renderer can run its path-tracing pass either as a **compute shader** (default) or a
**fragment shader**. This is a compile-time switch in `src/config.hpp`:

```cpp
const bool USE_COMPUTE_SH = true;   // false = fragment path
```

Compute is the primary, faster path. The fragment path exists mainly for comparison and for
environments where compute shaders are unavailable. Change it and rebuild.


---

## Troubleshooting

**CMake: `OpenImageDenoise` not found.**
Make sure OIDN 2.5.0 is at `/opt/oidn` (or pass `-DOIDN_ROOT=/your/path`). The directory
`${OIDN_ROOT}/lib/cmake/OpenImageDenoise-2.5.0` must exist.

**Runtime: `libOpenImageDenoise.so.2` cannot be found.**
The build copies OIDN's `.so` files into `build/lib/`. If they're missing, rebuild so the
post-build step runs, or run `sudo ldconfig` after the manual OIDN install.

**GLFW / GLEW / GLM / Assimp not found at configure time.**
Install the matching `-dev` package from the table above; CMake's error message names it.

**The app opens but the model / HDRI lists are empty.**
You're probably not running from `build/`. `cd build` first, then `./pathtracer`. Also press
**Scan** in the UI after adding new files.

**Crash / freeze on a weak or integrated GPU (driver watchdog timeout).**
A single large compute dispatch can exceed the GPU's watchdog limit. In
**Render Inspector → Preview**, enable **Tile Dispatch** and lower **Rows** — this splits the
frame into smaller synchronized dispatches. Lower **Preview Res** and **Max Samples** too if
VRAM is tight.

**A `.gltf` / `.glb` loads but textures are missing.**
Keep the model's texture files in the same folder structure the exporter produced (usually a
`textures/` subfolder next to the model). Assimp resolves texture paths relative to the model.
