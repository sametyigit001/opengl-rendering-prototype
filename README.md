# Refactored Engine Code

A Windows x64 model viewer and editor built with a C++17 OpenGL renderer and a .NET 8 WPF interface. The WPF viewport hosts a native child window through the C bridge in `src/bridge.cpp`.

## Current features

- Import static models with Assimp and display their node hierarchy in the editor.
- Select models or nodes from the hierarchy or by clicking the viewport. Mesh picking uses bounding volume hierarchies.
- Edit model and node transforms, toggle wireframe per model, and inspect frame rate in the editor.
- Render textured meshes with directional lighting using OpenGL 3.3 shaders.

The editor and renderer are still under development. Large model imports are synchronous and can stall the UI. Assets used for local development are not included in the repository.

## Build

Requirements: Windows x64, Visual Studio 2022 with the C++ desktop workload, .NET 8 SDK with WPF support, CMake 3.24 or newer, and Assimp. The repository includes GLAD, GLM, and stb_image headers. A local `dependencies/include/assimp` plus `dependencies/libs/assimp-vc143-mt.lib` and `zlibstatic.lib` layout is supported for the current machine. For a fresh clone, install vcpkg and pass its toolchain to CMake; `vcpkg.json` declares Assimp.

```powershell
cmake --preset x64-debug -DCMAKE_TOOLCHAIN_FILE="C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake"
cmake --build --preset x64-debug --target editor
```

For Release, replace `x64-debug` with `x64-release`. The `editor` target builds the native DLL and WPF app and copies the native DLL and its runtime dependencies beside the editor executable at `editor/bin/x64/Debug/net8.0-windows/` or the corresponding Release directory. Open `rendererUI.exe` from that directory. To build only the DLL, use `--target refactored_engine_code`.

The existing `refactored_engine_code.sln` is also available for Visual Studio. Select an x64 configuration. Its native project uses the local Assimp library layout under `dependencies/`.

## Layout

```text
editor/         WPF editor and native viewport interop
src/            Renderer, model import, scene, picking, and Windows host
src/shaders/    GLSL shader sources
dependencies/   Vendored headers and optional local libraries
cmake/          CMake build helpers
assets/         Local development assets (ignored)
```

`src/main.cpp` and the old standalone GLFW window were removed when the editor became the application entry point. The public interop functions remain in `src/bridge.h`.

## License

See [LICENSE.txt](LICENSE.txt).
