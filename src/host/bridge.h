#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstdint>

#define BRIDGE_API __declspec(dllexport)

extern "C" {
	BRIDGE_API void* hostCreate(void* parent, uint32_t width,uint32_t height);
	BRIDGE_API void hostResize(uint32_t width, uint32_t height);
	BRIDGE_API void hostRender();
	BRIDGE_API void hostDestroy();
	BRIDGE_API void hostKeyboardInput(int direction, float deltatime);
	BRIDGE_API void HostMouveMove(float xoffset, float yoffset);
	BRIDGE_API void hostLoadModel(const char* path);
	BRIDGE_API int hostPick(float mouseX, float mouseY,int* outModelIndex, int* outNodeIndex,bool* isHit);
	BRIDGE_API void deleteModel(int modelIndex);
	BRIDGE_API int hostGetNodeCount(int modelIndex);
	BRIDGE_API int hostGetNodeParent(int modelIndex,int index);
	BRIDGE_API const char* hostGetNodeName(int modelIndex,int index);
	BRIDGE_API int hostGetNodePos(int modelIndex,int index, float* x, float* y, float* z);
	BRIDGE_API int hostGetNodeRotation(int modelIndex,int index, float* x, float* y, float* z);
	BRIDGE_API int hostGetNodeScale(int modelIndex, int index, float* x, float* y, float* z);
	BRIDGE_API int hostSelectNode(int modelIndex, int index);
	BRIDGE_API bool hostIsModelLoadInProgress();
	BRIDGE_API void hostUpdateNode(int modelIndex, int index, float x, float y, float z);
	BRIDGE_API void hostSetNodePos(int modelIndex, int index, float x, float y, float z);
	BRIDGE_API void hostSetNodeRotation(int modelIndex, int index, float x, float y, float z);
	BRIDGE_API void hostSetNodeScale(int modelIndex, int index, float x, float y, float z);
	BRIDGE_API int getModelIndex();
	BRIDGE_API int hostSelectModel(int modelIndex);
	BRIDGE_API void hostClearSelection();
	BRIDGE_API void hostEnableWireframe(int index);
	BRIDGE_API void hostDisableWireframe(int index);
	BRIDGE_API void hostGetRenderStats(double* outFps, double* outFrameTimeMs);
}
