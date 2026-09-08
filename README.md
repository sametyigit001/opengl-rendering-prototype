# Refactored OpenGL Engine

> A C++17 student rendering-engine project built to demonstrate core understanding of **C++ memory management**, **software architecture**, and **modern OpenGL** principles.

This project is a compact, hands-on exploration of real-time rendering fundamentals. It loads external 3D assets, uploads geometry and textures to the GPU, manages shader-driven materials and lights, and presents the scene through an interactive first-person camera. The code intentionally separates platform/windowing, GPU resources, scene assets, materials, lighting, and rendering responsibilities—the same boundaries that scale into larger engine work.

## Visual Showcase

### Engine Demo

![Interactive engine demo](media/demo.gif)

*Real-time Stanford Lucy rendering with free-look camera movement and the live FPS counter.*

### Screenshots

| Hero render | Geometry detail | Alternate view |
| --- | --- | --- |
| ![Stanford Lucy overview](media/lucy-overview.png) | ![Stanford Lucy close-up](media/lucy-closeup.png) | ![Stanford Lucy alternate angle](media/lucy-alternate-angle.png) |

## Key Technical Highlights

- **Explicit GPU-resource ownership.** `VAO`, `VBO`, `EBO`, `Texture`, and `Shader` encapsulate OpenGL object lifetimes, delete copy operations, and support move semantics where applicable. This provides a deliberate RAII-oriented boundary around low-level graphics handles.
- **Data-driven mesh upload path.** Imported vertex/index data is uploaded into indexed GPU buffers, vertex attributes are built from layouts, and CPU-side mesh data is released after upload to reduce persistent memory use.
- **Model import with reusable texture assets.** The `Model` layer uses **Assimp** to recursively process scene nodes and triangulated meshes. It resolves texture paths robustly and caches textures by canonical path through `std::shared_ptr`, avoiding duplicate GPU texture creation across materials.
- **Material abstraction with graceful fallbacks.** `Material` binds diffuse/specular maps and supplies a lazily-created fallback texture when an asset lacks a map, keeping the shading path valid for incomplete source content.
- **Shader-driven Blinn–Phong lighting.** The GLSL material shader supports directional, point, and spot lights, distance attenuation, specular highlights, texture-backed diffuse/specular terms, and a normal-matrix transform for correct lighting under non-uniform transforms.
- **Interactive camera and responsive viewport.** Delta-time movement, mouse-look yaw/pitch clamping, perspective/orthographic camera configuration, framebuffer resize handling, depth testing, back-face culling, and 4× MSAA establish a practical real-time rendering loop.
- **Modern C++17 building blocks.** The project uses `std::vector`, `std::unordered_map`, `std::filesystem`, smart pointers, `constexpr` texture slots, scoped enums, and move operations. Mesh preprocessing also contains OpenMP parallel loops for independent vertex and index conversion.

## Architecture at a Glance

```text
GLFW Window + input
        |
      Camera -------------------+
        |                       |
        +--> Shader uniforms <--+---- Light Manager
                                    |
Assimp Model --> Mesh --> VAO / VBO / EBO --> OpenGL draw calls
                 |
              Material --> cached Texture assets --> GLSL sampler uniforms
```

## Tech Stack

- **Language:** C++17
- **Graphics API:** OpenGL 3.3 Core Profile / GLSL 330
- **Windowing & input:** GLFW
- **OpenGL loader:** GLAD
- **Math:** GLM
- **Asset import:** Assimp
- **Image loading:** stb_image
- **Parallel preprocessing:** OpenMP (enabled in the x64 Release configuration)
- **Build environment:** Visual Studio 2022 / MSVC v143 on Windows

## Current Scope & Asset Support

This is an actively evolving learning engine rather than a full production renderer. The present implementation focuses on the following supported path:

- **Static, triangulated meshes** imported through Assimp, with position, normal, and UV vertex attributes. Skeletal animation, morph targets, vertex colours, and tangent-space data are not yet processed.
- **2D external texture files** loaded through `stb_image`, with diffuse/base-color and specular texture usage. Embedded textures and texture arrays/cubemaps are not currently part of the asset path.
- **Blinn–Phong materials.** PBR maps—metallic, roughness, normal, ambient occlusion, emissive, and image-based lighting—are not yet fully interpreted by the renderer.
- **Up to 16 directional, point, or spot lights** in the forward shader. Shadows, framebuffer passes, post-processing, and deferred rendering are planned next steps.
- **Synchronous startup asset loading** for a locally configured showcase scene. Runtime scene selection, asset streaming, and an editor-driven scene format are future work.
- **Windows / Visual Studio x64 build workflow.** The checked-in project uses bundled Windows libraries and is designed for MSVC v143; a CMake build is a future portability improvement.

## Build & Setup

### Prerequisites

- Windows 10/11 with Visual Studio 2022 and the **Desktop development with C++** workload
- A GPU/driver supporting OpenGL 3.3 Core
- Assimp headers and the matching `assimp-vc143-mt.lib` / `zlibstatic.lib` binaries available under `dependencies/` (or equivalent paths configured in the Visual Studio project)

GLFW, GLAD, GLM, and stb_image source dependencies are included in the repository. Assimp binaries and headers are currently local-only; dependency setup will be automated in a future build-system update.

### Build and run

1. Clone the repository and open `refactored_engine_code.sln` in Visual Studio.
2. Select **x64** and either **Debug** or **Release**.
3. Build with `Ctrl+Shift+B`.
4. Before the first run, configure the sample model path in `src/main.cpp` to point to an asset available on your machine.
5. Run with `F5` (or `Ctrl+F5`) from the repository root so the relative shader paths resolve correctly.

> **Asset note:** Showcase assets, including the Stanford Lucy mesh, are intentionally excluded from version control because of their size and/or licensing. The renderer uses Assimp, so you can supply a compatible local static model for evaluation. A future scene/configuration layer will remove the temporary source-level model-path setting.

### Asset paths and texture discovery

Pass the model file path to `Model` in `src/main.cpp`. The importer derives the model directory from that path, then asks Assimp for each material's diffuse/base-color and specular texture reference.

For each referenced external texture, the engine attempts these locations in order:

1. The texture path exactly as stored in the model file.
2. The path relative to the model directory.
3. The model directory using only the texture filename.
4. A `textures/` folder inside the model directory.
5. A sibling `textures/` folder beside the model directory.
6. The parent directory using only the texture filename, followed by a recursive filename search within that parent directory.

Resolved paths are canonicalized and cached, so repeated material references reuse one GPU texture. If diffuse or specular maps are absent, the material system binds a 1×1 white fallback texture instead. Embedded textures and PBR texture maps are not currently supported.

### Controls

| Input | Action |
| --- | --- |
| `W` / `A` / `S` / `D` | Move forward / left / back / right |
| `Left Shift` / `Left Ctrl` | Move up / down |
| Mouse | Look around |
| `Esc` | Exit |

## Repository Layout

```text
src/
  main.cpp              Application loop and render orchestration
  Window.*              GLFW window, context, resize, and event handling
  Camera.h              First-person camera and projection configuration
  Shader.*              GLSL compilation, linking, uniform caching, uploads
  Model.*               Assimp import, node traversal, texture caching
  Mesh.*                GPU mesh setup and indexed/non-indexed drawing
  Material.*            Texture-backed material binding and fallbacks
  Texture.*             stb_image loading and OpenGL texture management
  Lights.*              Configurable directional, point, and spot lights
  vertexArray.*, vertexBuffer.*, elementBuffer.*
                         Low-level OpenGL buffer abstractions
  shaders/              GLSL vertex and fragment shader programs
assets/                 Local-only showcase meshes and source textures (not tracked)
dependencies/           Vendored headers and prebuilt libraries
```

## Roadmap

### Implemented

- [x] GLFW window creation and OpenGL 3.3 Core context setup
- [x] GLAD function loading
- [x] RAII-style wrappers for vertex arrays, vertex buffers, element buffers, textures, and shader programs
- [x] Indexed mesh rendering with position, normal, and UV attributes
- [x] Assimp-powered model import and recursive scene traversal
- [x] Texture caching, mipmaps, configurable wrapping, and anisotropic filtering when supported
- [x] Diffuse/specular material workflow with fallback textures
- [x] Directional, point, and spot-light shader model with attenuation
- [x] First-person camera, mouse look, delta-time movement, depth testing, MSAA, and face culling

### Next milestones

- [ ] **C# based UI integration** — expose scene, camera, material, and light settings through an editor-facing UI bridge.
- [ ] Replace the separate VBO and EBO wrappers with a generic GPU buffer abstraction for more reusable buffer management.
- [ ] **Instanced Rendering** — add per-instance transforms and `glDraw*Instanced` for high-volume repeated geometry.
- [ ] Add uniform buffer objects (UBOs) for frame/camera/light data and reduce per-uniform driver calls.
- [ ] **Shadow Mapping / Shadow Effects** — implement a depth pass, light-space transforms, PCF filtering, and bias management.
- [ ] Add framebuffer rendering to support a render-to-texture viewport and a foundation for post-processing.
- [ ] Add a simple post-processing pass (grayscale, inversion, or gamma correction) to validate the framebuffer pipeline.
- [ ] Refactor render setup into a small renderer layer as the number of passes grows.

## License

See [LICENSE.txt](LICENSE.txt).
