using System;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace rendererUI
{
    internal static class coreEngine
    {
        private const string DLLName = "refactored_engine_code.dll";

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr hostCreate(IntPtr parent, uint width, uint height);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostPick(float mouseX, float mouseY, out int entityIndex, out int nodeIndex, [MarshalAs(UnmanagedType.I1)] out bool isHit);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostResize(uint width, uint height);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostDestroy();

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostKeyboardInput(int direction, float deltatime);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostMouseMove(float xoffset, float yoffset);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void deleteModel(int entityIndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void hostLoadModel([MarshalAs(UnmanagedType.LPStr)] string filepath, int entityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeCount(int entityIndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeParent(int entityIndex, int nodeIndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr hostGetNodeName(int entityIndex, int nodeIndex);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodePos(int entityIndex, int nodeIndex, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeRotation(int entityIndex, int nodeIndex, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeScale(int entityIndex, int nodeIndex, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostSelectNode(int entityIndex, int nodeIndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostIsModelLoadInProgress();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetNodePos(int entityIndex, int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetNodeRotation(int entityIndex, int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetNodeScale(int entityIndex, int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetImportedEntityIndex();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostSelectModel(int entityIndex);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostClearSelection();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostDisableWireframe(int entityIndex);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostEnableWireframe(int entityIndex);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostNewEntity();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostDeleteEntity(int entityIndex);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostSetEntityPos(int entityIndex, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostSetEntityRotation(int entityIndex, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostSetEntityScale(int entityIndex, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetEntityPos(int entityIndex, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetEntityRotation(int entityIndex, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetEntityScale(int entityIndex, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetViewportCamera(out float fov, out float speed, out float nearPlane, out float farPlane);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetSelectedCamera(int index, out float fov, out float speed, out float nearPlane, out float farPlane);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSelectEntity(int entityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetViewportCamera(float fov, float speed, float nearPlane, float farPlane);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetSelectedCamera(int index, float fov, float speed, float nearPlane, float farPlane);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetEntityCamPos(int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetEntityCamRotation(int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetEntityCamPos(int index, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetEntityCamRotation(int index, out float x, out float y, out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostDeleteCamera(int entityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostSetParent(int child, int parent);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostAddCamera(int entityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSwitchCamera(int entityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSwitchToEditor();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostGetLight(int entityId, out LightConfig config);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostCopyLight(int sourceEntityId, int targetEntityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostSetLightIntensity(int entityId, float val);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostSetLightColor(int entityId, float red, float green, float blue);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostSetLightType(int entityId, int type);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostAddLight(int entityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostDeleteLight(int entityId);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightPos(int entityId, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightDir(int entityId, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightAmbientStr(int entityId,float val);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightDiffuseStr(int entityId,float val);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightSpecularStr(int entityId,float val);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightCutOff(int entityId,float inner,float outer);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightConstant(int entityId,float val);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightLinear(int entityId,float val);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetLightQuadratic(int entityId,float val);

        // Implement this export in the engine bridge to expose the latest
        // render-thread measurements to the UI.
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "hostGetRenderStats")]
        public static extern void hostGetRenderStats(out double fps, out double frameTimeMs);

        // Safe wrapper to convert the C++ pointer into a C# string
        public static string GetNodeName(int entityIndex, int nodeIndex)
        {
            IntPtr ptr = hostGetNodeName(entityIndex, nodeIndex);
            return Marshal.PtrToStringAnsi(ptr) ?? "Unknown";
        }
    }
    [StructLayout(LayoutKind.Sequential)]
    struct Float3
    {
        public float x, y, z;
    }

    enum LightTypes : int
    {
        Directional = 0,
        Point = 1,
        Spot = 2
    }

    [StructLayout(LayoutKind.Sequential)]
    struct LightConfig
    {
        public LightTypes type;
        public Float3 direction, position,color;
        public float ambientStr, diffuseStr, specularStr;
        public float cutOff, outerCutOff;
        public float constant, linear, quadratic,intensity;
    }
}
