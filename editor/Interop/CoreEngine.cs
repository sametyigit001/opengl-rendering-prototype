using System;
using System.Runtime.InteropServices;

namespace rendererUI
{
    internal static class coreEngine
    {
        private const string DLLName = "refactored_engine_code.dll";

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr hostCreate(IntPtr parent, uint width, uint height);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostPick(float mouseX,float mouseY,out int modelIndex,out int nodeIndex, [MarshalAs(UnmanagedType.I1)] out bool isHit);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostResize(uint width, uint height);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostDestroy();

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostKeyboardInput(int direction, float deltatime);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostMouseMove(float xoffset, float yoffset);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void deleteModel(int modelIndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern void hostLoadModel([MarshalAs(UnmanagedType.LPStr)] string filepath);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeCount(int modelIndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeParent(int modelIndex,int nodeIndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        private static extern IntPtr hostGetNodeName(int modelndex,int nodeIndex);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodePos(int modelIndex,int nodeIndex,out float x,out float y,out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeRotation(int modelIndex,int nodeIndex,out float x,out float y,out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostGetNodeScale(int modelIndex, int nodeIndex,out float x,out float y,out float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostSelectNode(int modelIndex,int nodeİndex);

        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        [return: MarshalAs(UnmanagedType.I1)]
        public static extern bool hostIsModelLoadInProgress();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetNodePos(int modelIndex,int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetNodeRotation(int modelIndex,int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostSetNodeScale(int modelIndex,int index, float x, float y, float z);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int getModelIndex();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern int hostSelectModel(int index);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostClearSelection();
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostDisableWireframe(int index);
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl)]
        public static extern void hostEnableWireframe(int index);

        // Implement this export in the engine bridge to expose the latest
        // render-thread measurements to the UI.
        [DllImport(DLLName, CallingConvention = CallingConvention.Cdecl, EntryPoint = "hostGetRenderStats")]
        public static extern void hostGetRenderStats(out double fps, out double frameTimeMs);

        // Safe wrapper to convert the C++ pointer into a C# string
        public static string GetNodeName(int modelIndex,int nodeIndex)
        {
            IntPtr ptr = hostGetNodeName(modelIndex,nodeIndex);
            return Marshal.PtrToStringAnsi(ptr) ?? "Unknown";
        }
    }
}
