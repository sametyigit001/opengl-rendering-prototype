#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstdint>

#define BRIDGE_API __declspec(dllexport)
struct Float3
{
	float x, y, z;
};

enum class LightTypes : int
{
	Directional = 0,
	Point = 1,
	Spot = 2
};


struct LightConfig
{
	LightTypes type;
	Float3 direction, position,color;
	float ambientStr, diffuseStr, specularStr;
	float cutOff, outerCutOff;
	float constant, linear, quadratic,intensity;
};


extern "C" {
	BRIDGE_API void* hostCreate(void* parent, uint32_t width,uint32_t height);
	BRIDGE_API void hostResize(uint32_t width, uint32_t height);
	BRIDGE_API void hostRender();
	BRIDGE_API void hostDestroy();
	BRIDGE_API void hostKeyboardInput(int direction, float deltatime);
	BRIDGE_API void HostMouseMove(float xoffset, float yoffset);
	BRIDGE_API void hostLoadModel(const char* path,int entityIndex);
	BRIDGE_API int hostPick(float mouseX, float mouseY,int* outEntityIndex, int* outNodeIndex,bool* isHit);
	BRIDGE_API void deleteModel(int entityIndex);
	BRIDGE_API int hostGetNodeCount(int entityIndex);
	BRIDGE_API int hostGetNodeParent(int entityIndex,int index);
	BRIDGE_API const char* hostGetNodeName(int entityIndex,int index);
	BRIDGE_API int hostGetNodePos(int entityIndex,int index, float* x, float* y, float* z);
	BRIDGE_API int hostGetNodeRotation(int entityIndex,int index, float* x, float* y, float* z);
	BRIDGE_API int hostGetNodeScale(int entityIndex, int index, float* x, float* y, float* z);
	BRIDGE_API int hostSelectNode(int entityIndex, int index);
	BRIDGE_API bool hostIsModelLoadInProgress();
	BRIDGE_API void hostUpdateNode(int entityIndex, int index, float x, float y, float z);
	BRIDGE_API void hostSetNodePos(int entityIndex, int index, float x, float y, float z);
	BRIDGE_API void hostSetNodeRotation(int entityIndex, int index, float x, float y, float z);
	BRIDGE_API void hostSetNodeScale(int entityIndex, int index, float x, float y, float z);
	BRIDGE_API int hostGetImportedEntityIndex();
	BRIDGE_API int hostSelectModel(int entityIndex);
	BRIDGE_API void hostClearSelection();
	BRIDGE_API void hostEnableWireframe(int entityIndex);
	BRIDGE_API void hostDisableWireframe(int entityIndex);
	BRIDGE_API void hostGetRenderStats(double* outFps, double* outFrameTimeMs);
	BRIDGE_API int hostNewEntity();
	BRIDGE_API void hostDeleteEntity(int entityIndex);
	BRIDGE_API int hostGetEntityPos(int entityIndex, float* x,float* y,float* z);
	BRIDGE_API int hostGetEntityScale(int entityIndex, float* x, float* y, float* z);
	BRIDGE_API int hostGetEntityRotation(int entityIndex, float* x, float* y, float* z);
	BRIDGE_API int hostSetEntityPos(int entityIndex, float x,float y,float z);
	BRIDGE_API int hostSetEntityScale(int entityIndex, float x, float y, float z);
	BRIDGE_API int hostSetEntityRotation(int entityIndex, float x, float y, float z);
	BRIDGE_API bool hostSetParent(int child, int parent);
	BRIDGE_API void hostSelectEntity(int entityId);
	BRIDGE_API int hostGetViewportCamera(float* fov, float* speed, float* nearPlane, float* farPlane);
	BRIDGE_API void hostSetViewportCamera(float fov, float speed, float nearPlane, float farPlane);
	BRIDGE_API int hostGetSelectedCamera(int index,float* fov, float* speed, float* nearPlane, float* farPlane);
	BRIDGE_API void hostSetSelectedCamera(int index,float fov, float speed, float nearPlane, float farPlane);
	BRIDGE_API void hostSetEntityCamPos(int entityId,float x, float y, float z);
	BRIDGE_API int hostAddCamera(int entityId);
	BRIDGE_API void hostSwitchCamera(int entityId);
	BRIDGE_API void hostSwitchToEditor();
    BRIDGE_API void hostSetEntityCamRotation(int entityId, float x, float y, float z);
    BRIDGE_API int hostGetEntityCamPos(int entityId, float* x, float* y, float* z);
    BRIDGE_API int hostGetEntityCamRotation(int entityId, float* x, float* y, float* z);
    BRIDGE_API void hostDeleteCamera(int entityId);
	BRIDGE_API bool hostGetLight(int entityId, LightConfig* outLight);
	BRIDGE_API bool hostCopyLight(int sourceEntityId,int targetEntityId);
	BRIDGE_API bool hostSetLightIntensity(int entityId, float val);
	BRIDGE_API bool hostSetLightType(int entityId, int type);
	BRIDGE_API bool hostSetLightColor(int entityId, float red, float green, float blue);
	BRIDGE_API bool hostAddLight(int entityId);
	BRIDGE_API bool hostDeleteLight(int entityId);
	BRIDGE_API void hostSetLightAmbientStr(int entityId, float val);
	BRIDGE_API void hostSetLightDiffuseStr(int entityId, float val);
	BRIDGE_API void hostSetLightSpecularStr(int entityId, float val);
	BRIDGE_API void hostSetLightCutOff(int entityId, float inner,float outer);
	BRIDGE_API void hostSetLightConstant(int entityId, float val);
	BRIDGE_API void hostSetLightLinear(int entityId, float val);
	BRIDGE_API void hostSetLightQuadratic(int entityId, float val);
	BRIDGE_API void hostSetLightPos(int entityId,float x, float y, float z);
	BRIDGE_API void hostSetLightDir(int entityId,float x, float y, float z);
	BRIDGE_API void hostSetVsync(int state);
}
