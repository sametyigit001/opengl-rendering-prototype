cmake_minimum_required(VERSION 3.24)

project(refactored_engine_code LANGUAGES C CXX)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "The WPF editor and WGL host require 64-bit Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(OpenGL REQUIRED)
find_package(assimp CONFIG QUIET)

if(TARGET assimp::assimp)
    set(RENDERER_ASSIMP_TARGET assimp::assimp)
else()
    # Existing local Visual Studio dependency layout. For a fresh clone, use vcpkg.
    set(RENDERER_LOCAL_DEPS "${CMAKE_CURRENT_SOURCE_DIR}/dependencies")
    if(NOT EXISTS "${RENDERER_LOCAL_DEPS}/include/assimp/Importer.hpp"
       OR NOT EXISTS "${RENDERER_LOCAL_DEPS}/libs/assimp-vc143-mt.lib"
       OR NOT EXISTS "${RENDERER_LOCAL_DEPS}/libs/zlibstatic.lib")
        message(FATAL_ERROR
            "Assimp is missing. Configure with vcpkg's CMake toolchain (vcpkg.json installs assimp), "
            "or provide the existing Assimp headers and libraries in dependencies/.")
    endif()
    add_library(renderer_local_assimp INTERFACE)
    target_include_directories(renderer_local_assimp INTERFACE "${RENDERER_LOCAL_DEPS}/include")
    target_link_libraries(renderer_local_assimp INTERFACE
        "${RENDERER_LOCAL_DEPS}/libs/assimp-vc143-mt.lib"
        "${RENDERER_LOCAL_DEPS}/libs/zlibstatic.lib")
    set(RENDERER_ASSIMP_TARGET renderer_local_assimp)
    set(RENDERER_ASSIMP_LOCAL TRUE)
endif()

add_library(refactored_engine_code SHARED
    src/bridge.cpp
    src/elementBuffer.cpp
    src/glad.c
    src/Lights.cpp
    src/Material.cpp
    src/Mesh.cpp
    src/Model.cpp
    src/Scene.cpp
    src/Shader.cpp
    src/Texture.cpp
    src/vertexArray.cpp
    src/vertexBuffer.cpp
    src/winHost.cpp
)

target_include_directories(refactored_engine_code PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/dependencies/include")
target_link_libraries(refactored_engine_code PRIVATE
    OpenGL::GL ${RENDERER_ASSIMP_TARGET} gdi32 user32)
target_compile_features(refactored_engine_code PRIVATE cxx_std_17)
target_compile_definitions(refactored_engine_code PRIVATE UNICODE _UNICODE)

if(MSVC)
    target_compile_options(refactored_engine_code PRIVATE /W3 /EHsc /openmp)
    if(RENDERER_ASSIMP_LOCAL)
        # The existing local archive was built with /MD and iterator level 0.
        set_property(TARGET refactored_engine_code PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreadedDLL")
        target_compile_definitions(refactored_engine_code PRIVATE _ITERATOR_DEBUG_LEVEL=0)
    endif()
endif()

find_program(DOTNET_EXECUTABLE dotnet)
if(DOTNET_EXECUTABLE)
    # Build the managed host after the native DLL, then copy both DLLs into its output.
    add_custom_target(editor
        COMMAND "${DOTNET_EXECUTABLE}" build "${CMAKE_CURRENT_SOURCE_DIR}/editor/rendererUI.csproj"
            --configuration "$<CONFIG>"
            -p:Platform=x64
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "$<TARGET_FILE:refactored_engine_code>"
            "${CMAKE_CURRENT_SOURCE_DIR}/editor/bin/x64/$<CONFIG>/net8.0-windows/"
        COMMAND "${CMAKE_COMMAND}"
            "-DDESTINATION=${CMAKE_CURRENT_SOURCE_DIR}/editor/bin/x64/$<CONFIG>/net8.0-windows"
            "-DRUNTIME_DLLS=$<TARGET_RUNTIME_DLLS:refactored_engine_code>"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyRuntimeDlls.cmake"
        DEPENDS refactored_engine_code
        VERBATIM)
else()
    message(STATUS "dotnet was not found; only the native DLL target is available.")
endif()
