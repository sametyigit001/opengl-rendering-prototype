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
	BRIDGE_API void hostModelLoad(const char* path);
	BRIDGE_API bool pick(float mouseX, float mouseY);
	BRIDGE_API void deleteModel();
	BRIDGE_API int hostGetNodeCount();
	BRIDGE_API int hostGetNodeParent(int index);
	BRIDGE_API const char* hostGetNodeName(int index);
	BRIDGE_API void hostSelectNode(int index);
}
