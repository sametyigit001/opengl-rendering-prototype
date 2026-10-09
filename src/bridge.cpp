#include "glad/glad.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#pragma comment(lib, "opengl32.lib")

#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>

#include "bridge.h"
#include "Scene.h"
#include "winHost.h"


static std::mutex g_pathMutex;
static std::string g_pendingPath = "";
static std::atomic<bool> g_needLoad(false);

static std::mutex g_sceneMutex;

static winHost* host = nullptr;
static HDC hdc = nullptr;
static HGLRC gl = nullptr;
static std::unique_ptr<Scene> scene = nullptr;

// Threading controls
static std::atomic<bool> g_Running(false);
static std::atomic<bool> g_modelLoad(false);
static std::thread g_RenderThread;
static std::atomic<double> g_latestFps(0.0);
static std::atomic<double> g_latestFrameTimeMs(0.0);

// Thread-safe resize pending states
static std::atomic<uint32_t> g_PendingWidth(0);
static std::atomic<uint32_t> g_PendingHeight(0);
static std::atomic<bool>     g_NeedsResize(false);
static std::atomic<int>      modelIndex = -1;

typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);

static bool initWGL(HWND hwnd) {
    hdc = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(hdc, &pfd);
    if (!format || !SetPixelFormat(hdc, format, &pfd)) return false;

    gl = wglCreateContext(hdc);
    if (!gl) return false;

    // Temporarily bind on UI thread to load GLAD and query extensions
    if (!wglMakeCurrent(hdc, gl)) return false;
    if (!gladLoadGL()) return false;

    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT =
        (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT) {
        wglSwapIntervalEXT(1); // 0 = Disable V-Sync (Immediate Swap)
    }

    // Release context so the background worker thread can claim it
    wglMakeCurrent(nullptr, nullptr);
    return true;
}

static void renderWorker() {
    // Claim the OpenGL context on this dedicated worker thread
    if (!wglMakeCurrent(hdc, gl)) return;

    auto lastFpsTime = std::chrono::high_resolution_clock::now();
    int frameCount = 0;

    while (g_Running.load(std::memory_order_relaxed)) {
        // Safe resize check performed on the context-owning thread
        if (g_NeedsResize.exchange(false, std::memory_order_acquire)) {
            uint32_t w = g_PendingWidth.load(std::memory_order_relaxed);
            uint32_t h = g_PendingHeight.load(std::memory_order_relaxed);

            if (w > 0 && h > 0) {
                glViewport(0, 0, w, h);
                std::lock_guard<std::mutex> lock(g_sceneMutex);
                if (scene) {
                    scene->resize(w, h);
                }
            }
        }

        if (g_needLoad.exchange(false, std::memory_order_acquire)) {
            std::string copyPath;
            {
                std::lock_guard<std::mutex> lock(g_pathMutex);
                copyPath = g_pendingPath;
            }
            std::lock_guard<std::mutex> lock(g_sceneMutex);
            if (scene && !copyPath.empty()) {
               modelIndex.store(scene->loadModel(copyPath),std::memory_order_release);
            }
            g_modelLoad.store(false, std::memory_order_release);
        } 
        {
            std::lock_guard<std::mutex> lock(g_sceneMutex);
            if (scene) {
                scene->render();
            }
        }
        SwapBuffers(hdc);

        // Native profiling: recalculates and logs every 500ms
        frameCount++;
        auto now = std::chrono::high_resolution_clock::now();
        double elapsedSec = std::chrono::duration<double>(now - lastFpsTime).count();

        if (elapsedSec >= 0.5) {
            double fps = frameCount / elapsedSec;
            double frameTimeMs = (elapsedSec / frameCount) * 1000.0;

            g_latestFps.store(fps, std::memory_order_relaxed);
            g_latestFrameTimeMs.store(frameTimeMs, std::memory_order_relaxed);

            char buf[128];
            sprintf_s(buf, "[Engine Render Thread] FPS: %.1f | FrameTime: %.2f ms\n", fps, frameTimeMs);
            OutputDebugStringA(buf);

            frameCount = 0;
            lastFpsTime = now;
        }
    }

    // Release context before thread exits
    wglMakeCurrent(nullptr, nullptr);
}

extern "C" {
    BRIDGE_API void* __cdecl hostCreate(void* parent, uint32_t width, uint32_t height) {
        g_latestFps.store(0.0, std::memory_order_relaxed);
        g_latestFrameTimeMs.store(0.0, std::memory_order_relaxed);

        AllocConsole();
        FILE* fDummy;
        freopen_s(&fDummy, "CONOUT$", "w", stdout);
        freopen_s(&fDummy, "CONOUT$", "w", stderr);
        std::cout << "[Engine] Native Console Attached!\n";

        if (!host) host = new winHost();
        if (!host->create(parent, width, height)) return nullptr;

        HWND hwnd = (HWND)host->getHandle();
        if (!initWGL(hwnd)) return nullptr;

        // Temporarily bind on main thread to construct Scene (compile shaders, load textures)
        wglMakeCurrent(hdc, gl);
        glViewport(0, 0, width, height);
        scene = std::make_unique<Scene>(width, height);
        wglMakeCurrent(nullptr, nullptr);

        // Spin up the independent render loop
        g_Running.store(true);
        g_RenderThread = std::thread(renderWorker);

        return hwnd;
    }

    BRIDGE_API void __cdecl hostResize(uint32_t width, uint32_t height) {
        if (host) {
            host->resize(width, height);
        }

        // Signal the worker thread without touching OpenGL context directly
        if (width > 0 && height > 0) {
            g_PendingWidth.store(width, std::memory_order_relaxed);
            g_PendingHeight.store(height, std::memory_order_relaxed);
            g_NeedsResize.store(true, std::memory_order_release);
        }
    }

    BRIDGE_API void __cdecl hostDestroy() {
        // Stop and join worker thread first
        if (g_Running.load()) {
            g_Running.store(false);
            if (g_RenderThread.joinable()) {
                g_RenderThread.join();
            }
        }

        // Clean up GL resources safely on calling thread
        if (hdc && gl) {
            wglMakeCurrent(hdc, gl);
            scene.reset();
            wglMakeCurrent(nullptr, nullptr);
        }

        if (gl) {
            wglDeleteContext(gl);
            gl = nullptr;
        }

        if (host) {
            if (hdc) ReleaseDC((HWND)host->getHandle(), hdc);
            delete host;
            host = nullptr;
        }
    }

    BRIDGE_API void __cdecl hostKeyboardInput(int direction, float deltatime) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (scene) {
            scene->onKeyboardInput(direction, deltatime);
        }
    }

    BRIDGE_API void __cdecl hostMouseMove(float xoffset, float yoffset) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (scene) {
            scene->onMouseMove(xoffset, yoffset);
        }
    }
    BRIDGE_API void __cdecl hostLoadModel(const char* path)
    {
        if (!path) return;
        {
            std::lock_guard<std::mutex> lock(g_pathMutex);
            g_pendingPath = path;
        }
        g_modelLoad.store(true, std::memory_order_release);
        g_needLoad.store(true, std::memory_order_release);
    }

    BRIDGE_API int __cdecl hostPick(float mouseX, float mouseY,
        int* outModelIndex, int* outNodeIndex,bool* isHit)
    {
        storeIndexes hit = scene->pick(mouseX, mouseY);

        if (!outModelIndex || !outNodeIndex) return 0;

        *outModelIndex = hit.modelIndex;
        *outNodeIndex = hit.nodeIndex;
        *isHit = hit.selected;
        return hit.modelIndex >= 0 ? 1 : 0;
    }
    BRIDGE_API void __cdecl deleteModel(int modelIndex) {
        if (scene) {
            scene->deleteModel(modelIndex);
        }
    }
    BRIDGE_API int __cdecl hostGetNodeCount(int modelIndex) {
        if (!scene) {
            std::cout << "[Bridge] ERROR: Global 'scene' pointer is NULL!\n";
            return 0;
        }
        if (!scene->getModel(modelIndex)) {
            std::cout << "[Bridge] ERROR: scene->getModel() is NULL (Model failed to load)!\n";
            return 0;
        }

        int count = static_cast<int>(scene->getModel(modelIndex)->getNodes().size());
        std::cout << "[Bridge] hostGetNodeCount successfully returning: " << count << "\n";
        return count;
    }
    BRIDGE_API int __cdecl hostGetNodeParent(int modelIndex,int index) {
        if (scene && scene->getModel(modelIndex)) {
            const auto& nodes = scene->getModel(modelIndex)->getNodes();
            if (index >= 0 && index < nodes.size()) {
                return nodes[index].parentIndex;
            }
        }
        return -1;
    }
    BRIDGE_API const char* __cdecl hostGetNodeName(int modelIndex,int index) {
        if (scene && scene->getModel(modelIndex)) {
            const auto& nodes = scene->getModel(modelIndex)->getNodes();
            if (index >= 0 && index < nodes.size()) {
                return nodes[index].name.c_str();
            }
        }
        return "unknown";
    }
    BRIDGE_API int __cdecl hostGetNodePos(int modelIndex,int index, float* x, float* y, float* z) {
        if (!scene || !scene->getModel(modelIndex) || !x || !y || !z) {
            return 0;
        }
        Model* model = scene->getModel(modelIndex);
        const auto& nodes = model->getNodes();

        if (index == -1) {
            const glm::vec3& position = model->position;
            *x = position.x;
            *y = position.y;
            *z = position.z;
        }
        else {
            if (index < 0 || static_cast<size_t>(index >= nodes.size())) {
                return 0;
            }

            const glm::vec3& position = nodes[index].position;
            *x = position.x;
            *y = position.y;
            *z = position.z;
        }


        return 1;
    }
    BRIDGE_API int __cdecl hostGetNodeScale(int modelIndex,int index, float* x, float* y, float* z) {
        if (!scene || !scene->getModel(modelIndex) || !x || !y || !z) {
            return 0;
        }
        Model* model = scene->getModel(modelIndex);
        const auto& nodes = model->getNodes();

        if (index == -1) {
            const glm::vec3& scale = model->scale;
            *x = scale.x;
            *y = scale.y;
            *z = scale.z;
        }
        else {
            if (index < 0 || static_cast<size_t>(index >= nodes.size())) {
                return 0;
            }

            const glm::vec3& scale = nodes[index].scale;
            *x = scale.x;
            *y = scale.y;
            *z = scale.z;
        }


        return 1;
    }
    BRIDGE_API int __cdecl hostGetNodeRotation(int modelIndex,int index, float* x, float* y, float* z) {
        if (!scene || !scene->getModel(modelIndex) || !x || !y || !z) {
            return 0;
        }
        Model* model = scene->getModel(modelIndex);
        const auto& nodes = model->getNodes();

        if (index == -1) {
            const glm::vec3& rotation = model->rotation;
            *x = rotation.x;
            *y = rotation.y;
            *z = rotation.z;
        }
        else {
            if (index < 0 || static_cast<size_t>(index >= nodes.size())) {
                return 0;
            }

            const glm::vec3& rotation = nodes[index].rotation;
            *x = rotation.x;
            *y = rotation.y;
            *z = rotation.z;
        }


        return 1;
    }
    BRIDGE_API int __cdecl hostSelectNode(int modelIndex, int index) {
        if (!scene || !scene->getModel(modelIndex)) {
            return 0;
        }
        Model* model = scene->getModel(modelIndex);

        model->selectNode(index);
        return 1;
    }
    BRIDGE_API int __cdecl hostSelectModel(int modelIndex) {
        if (!scene) return 0;
        scene->selectModel(modelIndex);
        return 1;
    }
    BRIDGE_API bool __cdecl hostIsModelLoadInProgress() {
        return g_modelLoad.load(std::memory_order_acquire);
    }
    BRIDGE_API void __cdecl hostSetNodePos(int modelIndex,int index, float x, float y, float z) {
        if (!scene || !scene->getModel(modelIndex)) {
            return;
        }

        Model* model = scene->getModel(modelIndex);
        if (index == -1) {
            model->position.x = x;
            model->position.y = y;
            model->position.z = z;
        }
        else {
             model->setNodePos(index, x, y, z);
        }
    }

    BRIDGE_API void __cdecl hostSetNodeRotation(int modelIndex, int index, float x, float y, float z) {
        if (!scene || !scene->getModel(modelIndex)) {
            return;
        }

        Model* model = scene->getModel(modelIndex);
        if (index == -1) {
           
            model->rotation.x = x;
            model->rotation.y = y;
            model->rotation.z = z;
        }
        else {
            model->setNodeRotation(index, x, y, z);
        }
    }

    BRIDGE_API void __cdecl hostSetNodeScale(int modelIndex,int index, float x, float y, float z) {
        if (!scene || !scene->getModel(modelIndex)) {
            return;
        }

        Model* model = scene->getModel(modelIndex);
        if (index == -1) {
            model->scale.x = x;
            model->scale.y = y;
            model->scale.z = z;
        }
        else {
            model->setNodeScale(index, x, y, z);
        }
    }
    BRIDGE_API int __cdecl getModelIndex() {
        const int index = modelIndex.exchange(-1, std::memory_order_relaxed);
        return index;
    }
    BRIDGE_API void __cdecl hostClearSelection() {
        if (!scene)
            return;
        scene->clearSelection();
    }
    BRIDGE_API void __cdecl hostEnableWireframe(int index) {
        if (!scene)
            return;
        scene->getModel(index)->enableWireframe = true;
    }
    BRIDGE_API void __cdecl hostDisableWireframe(int index) {
        if (!scene)
            return;
        scene->getModel(index)->enableWireframe = false;
    }
    BRIDGE_API void __cdecl hostGetRenderStats(double* outFps, double* outFrameTimeMs) {
        if (!outFps || !outFrameTimeMs)
            return;

        *outFps = g_latestFps.load(std::memory_order_relaxed);
        *outFrameTimeMs = g_latestFrameTimeMs.load(std::memory_order_relaxed);
    }
}
