#include "glad/glad.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#pragma comment(lib, "opengl32.lib")

#include <thread>
#include <atomic>
#include <memory>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>

#include "bridge.h"
#include "scene/Scene.h"
#include "winHost.h"


static std::mutex g_pathMutex;
static std::string g_pendingPath = "";
static int g_pendingEntityId = -1;
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
static std::atomic<int>      g_importedEntityIndex = -1;

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
            int copyEntityId;
            {
                std::lock_guard<std::mutex> lock(g_pathMutex);
                copyPath = g_pendingPath;
                copyEntityId = g_pendingEntityId;
            }
            std::lock_guard<std::mutex> lock(g_sceneMutex);
            if (scene && !copyPath.empty()) {
               g_importedEntityIndex.store(scene->loadModel(copyPath,copyEntityId),std::memory_order_release);
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

        // Wait for any outstanding UI-thread pick before destroying the scene.
        {
            std::lock_guard<std::mutex> lock(g_sceneMutex);
            if (hdc && gl) {
                wglMakeCurrent(hdc, gl);
                scene.reset();
                wglMakeCurrent(nullptr, nullptr);
            }
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
    BRIDGE_API void __cdecl hostLoadModel(const char* path, int entityId)
    {
        if (!path) return;
        {
            std::lock_guard<std::mutex> lock(g_pathMutex);
            g_pendingEntityId = entityId;
            g_pendingPath = path;
        }
        g_modelLoad.store(true, std::memory_order_release);
        g_needLoad.store(true, std::memory_order_release);
    }

    BRIDGE_API int __cdecl hostPick(float mouseX, float mouseY,
        int* outEntityIndex, int* outNodeIndex,bool* isHit)
    {
        if (!outEntityIndex || !outNodeIndex || !isHit)
            return 0;

        *outEntityIndex = -1;
        *outNodeIndex = -1;
        *isHit = false;

        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene)
            return 0;

        storeIndexes hit = scene->pick(mouseX, mouseY);

        *outEntityIndex = hit.entityIndex;
        *outNodeIndex = hit.nodeIndex;
        *isHit = hit.selected;
        return hit.entityIndex >= 0 ? 1 : 0;
    }
    BRIDGE_API void __cdecl deleteModel(int entityIndex) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (scene) {
            scene->deleteModel(entityIndex);
        }
    }
    BRIDGE_API int __cdecl hostGetNodeCount(int entityIndex) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene) {
            std::cout << "[Bridge] ERROR: Global 'scene' pointer is NULL!\n";
            return 0;
        }
        if (!scene->getModel(entityIndex)) {
            std::cout << "[Bridge] ERROR: scene->getModel() is NULL (Model failed to load)!\n";
            return 0;
        }

        int count = static_cast<int>(scene->getModel(entityIndex)->getNodes().size());
        std::cout << "[Bridge] hostGetNodeCount successfully returning: " << count << "\n";
        return count;
    }
    BRIDGE_API int __cdecl hostGetNodeParent(int entityIndex,int index) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (scene && scene->getModel(entityIndex)) {
            const auto& nodes = scene->getModel(entityIndex)->getNodes();
            if (index >= 0 && index < nodes.size()) {
                return nodes[index].parentIndex;
            }
        }
        return -1;
    }
    BRIDGE_API const char* __cdecl hostGetNodeName(int entityIndex,int index) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (scene && scene->getModel(entityIndex)) {
            const auto& nodes = scene->getModel(entityIndex)->getNodes();
            if (index >= 0 && index < nodes.size()) {
                return nodes[index].name.c_str();
            }
        }
        return "unknown";
    }
    BRIDGE_API int __cdecl hostGetNodePos(int entityIndex,int index, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex) || !x || !y || !z) {
            return 0;
        }
        Model* model = scene->getModel(entityIndex);
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
    BRIDGE_API int __cdecl hostGetNodeScale(int entityIndex,int index, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex) || !x || !y || !z) {
            return 0;
        }
        Model* model = scene->getModel(entityIndex);
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
    BRIDGE_API int __cdecl hostGetNodeRotation(int entityIndex,int index, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex) || !x || !y || !z) {
            return 0;
        }
        Model* model = scene->getModel(entityIndex);
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
    BRIDGE_API int __cdecl hostSelectNode(int entityIndex, int index) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex)) {
            return 0;
        }
        scene->selectNode(entityIndex,index);
        return 1;
    }
    BRIDGE_API int __cdecl hostSelectModel(int entityIndex) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex)) return 0;
        scene->selectModel(entityIndex);
        return 1;
    }
    BRIDGE_API bool __cdecl hostIsModelLoadInProgress() {
        return g_modelLoad.load(std::memory_order_acquire);
    }
    BRIDGE_API void __cdecl hostSetNodePos(int entityIndex,int index, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex)) {
            return;
        }

        Model* model = scene->getModel(entityIndex);
        if (index == -1) {
            model->position = { x,y,z };
        }
        else {
             model->setNodePos(index, x, y, z);
        }

    }

    BRIDGE_API void __cdecl hostSetNodeRotation(int entityIndex, int index, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex)) {
            return;
        }

        Model* model = scene->getModel(entityIndex);
        if (index == -1) {
           
            model->rotation = { x,y,z };
        }
        else {
            model->setNodeRotation(index, x, y, z);
        }
    }

    BRIDGE_API void __cdecl hostSetNodeScale(int entityIndex,int index, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getModel(entityIndex)) {
            return;
        }

        Model* model = scene->getModel(entityIndex);
        if (index == -1) {
            model->scale = { x,y,z };
        }
        else {
            model->setNodeScale(index, x, y, z);
        }
    }
    BRIDGE_API int __cdecl hostGetImportedEntityIndex() {
        return g_importedEntityIndex.exchange(-1, std::memory_order_relaxed);
    }
    BRIDGE_API void __cdecl hostClearSelection() {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene)
            return;
        scene->clearSelection();
    }
    BRIDGE_API void __cdecl hostEnableWireframe(int entityIndex) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene)
            return;
        if (Model* model = scene->getModel(entityIndex))
            model->enableWireframe = true;
    }
    BRIDGE_API void __cdecl hostDisableWireframe(int entityIndex) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene)
            return;
        if (Model* model = scene->getModel(entityIndex))
            model->enableWireframe = false;
    }
    BRIDGE_API void __cdecl hostGetRenderStats(double* outFps, double* outFrameTimeMs) {
        if (!outFps || !outFrameTimeMs)
            return;

        *outFps = g_latestFps.load(std::memory_order_relaxed);
        *outFrameTimeMs = g_latestFrameTimeMs.load(std::memory_order_relaxed);
    }
    BRIDGE_API int __cdecl hostNewEntity() {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene) return -3;

       return scene->newEntity();
    }
    BRIDGE_API void __cdecl hostDeleteEntity(int entityIndex) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene) return;
        scene->deleteEntity(entityIndex);
    }
    BRIDGE_API int __cdecl hostGetEntityPos(int entityIndex, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityIndex) || !x || !y || !z) return 0;

        Entity* entity = scene->getEntity(entityIndex);

        *x =  entity->local.position.x;
        *y =  entity->local.position.y;
        *z =  entity->local.position.z;
        return 1;
    };
    BRIDGE_API int __cdecl hostGetEntityScale(int entityIndex, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityIndex) || !x || !y || !z) return 0;

        Entity* entity = scene->getEntity(entityIndex);

        *x = entity->local.scale.x;
        *y = entity->local.scale.y;
        *z = entity->local.scale.z;
        return 1;
    };
    BRIDGE_API int __cdecl hostGetEntityRotation(int entityIndex, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityIndex) || !x || !y || !z) return 0;

        Entity* entity = scene->getEntity(entityIndex);

        *x = entity->local.rotation.x;
        *y = entity->local.rotation.y;
        *z = entity->local.rotation.z;
        return 1;
    };
    BRIDGE_API int __cdecl hostSetEntityPos(int entityIndex, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityIndex)) {
            return 0;
        }

        Entity* entity = scene->getEntity(entityIndex);
        entity->local.position = { x,y,z };
        scene->updateEntityMat(entityIndex);
        return 1;
    }
    BRIDGE_API int __cdecl hostSetEntityScale(int entityIndex, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityIndex)) {
            return 0;
        }

        Entity* entity = scene->getEntity(entityIndex);
        entity->local.scale = { x,y,z };
        scene->updateEntityMat(entityIndex);
        return 1;
    }
    BRIDGE_API int __cdecl hostSetEntityRotation(int entityIndex, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityIndex)) {
            return 0;
        }

        Entity* entity = scene->getEntity(entityIndex);
        entity->local.rotation = { x,y,z };
        scene->updateEntityMat(entityIndex);
        return 1;
    }
    BRIDGE_API bool __cdecl hostSetParent(int child, int parent) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene) return false;
        return scene->setParent(child, parent);
    }
    BRIDGE_API void __cdecl hostSelectEntity(int entityId) {
        if (!scene || !scene->getEntity(entityId)) return;

        scene->selectEntity(entityId);
    }
    BRIDGE_API int __cdecl hostGetViewportCamera(float* fov, float* speed, float* nearPlane, float* farPlane)
    {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getViewportCamera() || !fov || !speed || !nearPlane || !farPlane)
            return 0;

        *fov = scene->getViewportCamera()->getFOV();
        *speed = scene->getViewportCamera()->getSpeed();
        *nearPlane = scene->getViewportCamera()->getNearPlane();
        *farPlane = scene->getViewportCamera()->getFarPlane();
        return 1;
    }
    BRIDGE_API void __cdecl hostSetViewportCamera(float fov, float speed, float nearPlane, float farPlane) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getViewportCamera()) {
            return;
        }
        scene->setEditorCameraSettings(fov, speed, nearPlane, farPlane);
    }
    BRIDGE_API int __cdecl hostAddCamera(int entityId) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityId)) return 0;
        return scene->addCamera(entityId);
    }
    BRIDGE_API void __cdecl hostSwitchCamera(int entityId) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityId)) return;

        scene->switchCamera(entityId);
    }
    BRIDGE_API void __cdecl hostSwitchToEditor() {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene) return;

        scene->switchToEditor();
    }
    BRIDGE_API int __cdecl hostGetSelectedCamera(int index,float* fov, float* speed, float* nearPlane, float* farPlane){

        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene ||!scene->getEntity(index) || !scene->getEntity(index)->camera || !fov || !speed || !nearPlane || !farPlane)
            return 0;
        Entity* entity = scene->getEntity(index);
        *fov = entity->camera->getFOV();
        *speed = entity->camera->getSpeed();
        *nearPlane = entity->camera->getNearPlane();
        *farPlane = entity->camera->getFarPlane();
        return 1;
    }
    BRIDGE_API void __cdecl hostSetSelectedCamera(int index, float fov, float speed, float nearPlane, float farPlane){
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(index)) return;
        scene->setSelectedCameraSettings(index, fov, speed, nearPlane, farPlane);
    }
    BRIDGE_API void __cdecl hostSetEntityCamPos(int entityId, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !scene->getEntity(entityId)) return;
        glm::vec3 pos(x, y, z);
        scene->setCameraPos(entityId, pos);

    }
    BRIDGE_API void __cdecl hostSetEntityCamRotation(int entityId, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene) return;
        scene->setCameraRotation(entityId, glm::vec3(x, y, z));
    }
    BRIDGE_API void __cdecl hostDeleteCamera(int entityId) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene) return;
        scene->deleteCamera(entityId);
    }
    BRIDGE_API int __cdecl hostGetEntityCamPos(int entityId, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !x || !y || !z) return 0;
        Entity* entity = scene->getEntity(entityId);
        if (!entity || !entity->camera) return 0;
        const glm::vec3& value = entity->camera->getLocalPos();
        *x = value.x;
        *y = value.y;
        *z = value.z;
        return 1;
    }
    BRIDGE_API int __cdecl hostGetEntityCamRotation(int entityId, float* x, float* y, float* z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        if (!scene || !x || !y || !z) return 0;
        Entity* entity = scene->getEntity(entityId);
        if (!entity || !entity->camera) return 0;
        const glm::vec3& value = entity->camera->getLocalRotation();
        *x = value.x;
        *y = value.y;
        *z = value.z;
        return 1;
    }
    BRIDGE_API bool __cdecl hostGetLight(int entityId, LightConfig* outLight) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !outLight || !entity->light) return false;
        LightConfig temp;
        temp.type = static_cast<LightTypes>(entity->light->config.type);
        temp.ambientStr =  entity->light->config.ambientStr;
        temp.diffuseStr =  entity->light->config.diffuseStr;
        temp.specularStr = entity->light->config.specularStr;
        temp.direction = { entity->light->config.direction.x ,entity->light->config.direction.y,entity->light->config.direction.z };
        temp.position = { entity->light->config.position.x ,entity->light->config.position.y,entity->light->config.position.z };
        temp.constant = entity->light->config.constant;
        temp.linear = entity->light->config.linear;
        temp.quadratic = entity->light->config.quadratic;
        temp.cutOff = entity->light->config.cutOff;
        temp.outerCutOff = entity->light->config.outerCutOff;
        temp.color = { entity->light->config.color.x,entity->light->config.color.y ,entity->light->config.color.z };
        temp.intensity = entity->light->config.intensity;

        *outLight = temp;
        return true;
    }
    BRIDGE_API bool __cdecl hostCopyLight(int sourceEntityId, int targetEntityId){
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* sourceEntity;
        Entity* targetEntity;
        if (!scene || !(sourceEntity = scene->getEntity(sourceEntityId)) || !(targetEntity = scene->getEntity(targetEntityId)) || !sourceEntity->light || !targetEntity->light) return false;

        targetEntity->light->config = sourceEntity->light->config;
        return true;

    }
    BRIDGE_API bool hostSetLightIntensity(int entityId, float val) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light || !std::isfinite(val) || val < 0.0f) return false;

       entity->light->config.intensity = val;
       return true;
    }

    BRIDGE_API bool hostSetLightType(int entityId, int type) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light) return false;
        if ((type == 0 || type == 2) && entity->light->config.direction == glm::vec3(0.0f)) return false;

        entity->light->config.type = (lightTypes)type;
        return true;
    }
    BRIDGE_API bool hostSetLightColor(int entityId, float red, float green,float blue) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light ||
            !std::isfinite(red) || !std::isfinite(green) || !std::isfinite(blue) ||
            red < 0.0f || red > 1.0f || green < 0.0f || green > 1.0f || blue < 0.0f || blue > 1.0f) return false;
        glm::vec3 color{ red,green,blue };
        entity->light->config.color = color;
        return true;
    }
    BRIDGE_API bool __cdecl hostAddLight(int entityId) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || entity->light) return false;
       return scene->addLight(entityId) != 0;
    }
    BRIDGE_API bool __cdecl hostDeleteLight(int entityId) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId))) return false;
        scene->deleteLight(entity);
        return true;
    }
    BRIDGE_API void __cdecl hostSetLightPos(int entityId, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) return;
        entity->light->config.position = { x,y,z };
    }
    BRIDGE_API void __cdecl hostSetLightDir(int entityId, float x, float y, float z) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light ||
            !std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z) ||
            (entity->light->config.type != lightTypes::point && x == 0.0f && y == 0.0f && z == 0.0f)) return;
        entity->light->config.direction = { x,y,z };
    }
    BRIDGE_API void hostSetLightAmbientStr(int entityId, float val) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light || !std::isfinite(val) || val < 0.0f) return;
        entity->light->config.ambientStr = val;
    }
    BRIDGE_API void hostSetLightDiffuseStr(int entityId, float val) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light || !std::isfinite(val) || val < 0.0f) return;
        entity->light->config.diffuseStr = val;
    }
    BRIDGE_API void hostSetLightSpecularStr(int entityId, float val) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light || !std::isfinite(val) || val < 0.0f) return;
        entity->light->config.specularStr = val;
    }
    BRIDGE_API void hostSetLightCutOff(int entityId, float inner, float outer) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light ||
            !std::isfinite(inner) || !std::isfinite(outer) ||
            inner < 0.0f || inner >= outer || outer > 180.0f ||
            glm::cos(glm::radians(inner)) <= glm::cos(glm::radians(outer))) return;
        entity->light->config.updateInnerCutOff(inner);
        entity->light->config.updateOuterCutOff(outer);
    }
    BRIDGE_API void hostSetLightConstant(int entityId, float val) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light || !std::isfinite(val) || val <= 0.0f) return;
        entity->light->config.constant = val;
    }
    BRIDGE_API void hostSetLightLinear(int entityId, float val) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light || !std::isfinite(val) || val < 0.0f) return;
        entity->light->config.linear = val;
    }
    BRIDGE_API void hostSetLightQuadratic(int entityId, float val) {
        std::lock_guard<std::mutex> lock(g_sceneMutex);
        Entity* entity;
        if (!scene || !(entity = scene->getEntity(entityId)) || !entity->light || !std::isfinite(val) || val < 0.0f) return;
        entity->light->config.quadratic = val;
    }
}
