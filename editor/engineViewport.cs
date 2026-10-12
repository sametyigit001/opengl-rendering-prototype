using System;
using System.Runtime.InteropServices;
using System.Windows;
using System.Windows.Input;
using System.Diagnostics;
using System.Threading.Tasks;
using System.Windows.Interop;
using System.Windows.Media;
using System.Windows.Controls;

namespace rendererUI
{
    public struct POINT
    {
        public int x, y;
    }

    enum movementKeys
    {
        forward = 0,
        backward = 1,
        left = 2,
        right = 3,
        up = 4,
        down = 5
    }

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

    public class engineViewport : HwndHost
    {
        [DllImport("user32.dll")]
        public static extern bool GetCursorPos(out POINT lpPoint);

        [DllImport("user32.dll")]
        public static extern short GetAsyncKeyState(int vKey);

        [DllImport("user32.dll")]
        private static extern IntPtr SetCapture(IntPtr hWnd);

        [DllImport("user32.dll")]
        private static extern bool ReleaseCapture();
        private const int VK_RBUTTON = 0x02;

        [DllImport("user32.dll")]
        public static extern bool ScreenToClient(IntPtr hWnd, ref POINT lpPoint);

        private const int VK_LBUTTON = 0x01;
        private bool _wasLeftClicking = false;
        private bool _pickInProgress = false;

        private POINT _lastRawPos;
        private bool _wasLooking = false;
        private bool _rightLookActive = false;

        private readonly Stopwatch _timer = new();
        private bool _initialized = false;
        private double lastFrame = 0.0;

        private bool _forward;
        private bool _backward;
        private bool _up;
        private bool _down;
        private bool _right;
        private bool _left;

       

        public engineViewport()
        {
            Focusable = true;
            Loaded += (s, e) => Focus();
            LostKeyboardFocus += (_, _) =>
            {
                _forward = _backward = _left = _right = _up = _down = false;

                if (_rightLookActive)
                    ReleaseCapture();

                _rightLookActive = false;
            };
        }

        protected override HandleRef BuildWindowCore(HandleRef hwndParent)
        {
            uint initialWidth = 800;
            uint initialHeight = 600;

            IntPtr hwnd = coreEngine.hostCreate(hwndParent.Handle, initialWidth, initialHeight);
            _initialized = true;

            _timer.Start();
            lastFrame = _timer.Elapsed.TotalSeconds;

            // CompositionTarget handles input polling smoothly without blocking the GPU
            CompositionTarget.Rendering += OnUpdateInput;
            return new HandleRef(this, hwnd);
        }

        private void OnUpdateInput(object? sender, EventArgs e)
        {
            double currentTime = _timer.Elapsed.TotalSeconds;
            float deltaTime = (float)(currentTime - lastFrame);
            lastFrame = currentTime;

            if (deltaTime > 0.1f) deltaTime = 0.1f;

            // Keyboard input dispatch
            if (_forward) coreEngine.hostKeyboardInput((int)movementKeys.forward, deltaTime);
            if (_backward) coreEngine.hostKeyboardInput((int)movementKeys.backward, deltaTime);
            if (_left) coreEngine.hostKeyboardInput((int)movementKeys.left, deltaTime);
            if (_right) coreEngine.hostKeyboardInput((int)movementKeys.right, deltaTime);
            if (_up) coreEngine.hostKeyboardInput((int)movementKeys.up, deltaTime);
            if (_down) coreEngine.hostKeyboardInput((int)movementKeys.down, deltaTime);

           
            // Start right-look only when the press begins inside the viewport.
            bool rightDown = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;

            if (rightDown && !_wasLooking)
            {
                GetCursorPos(out POINT startPos);
                POINT clientPos = startPos;
                ScreenToClient(Handle, ref clientPos);

                bool startedInside =
                    clientPos.x >= 0 && clientPos.y >= 0 &&
                    clientPos.x <= ActualWidth && clientPos.y <= ActualHeight;

                if (startedInside)
                {
                    _rightLookActive = true;
                    SetCapture(Handle);
                    _lastRawPos = startPos;

                    Focus();
                    Keyboard.Focus(this);
                }
            }

            if (_rightLookActive && rightDown)
            {
                GetCursorPos(out POINT currentPos);

                float xoffset = (float)(currentPos.x - _lastRawPos.x);
                float yoffset = (float)(_lastRawPos.y - currentPos.y);

                if (Math.Abs(xoffset) > 0.001f || Math.Abs(yoffset) > 0.001f)
                    coreEngine.hostMouseMove(xoffset, yoffset);

                _lastRawPos = currentPos;
            }

            if (!rightDown && _wasLooking)
            {
                if (_rightLookActive)
                    ReleaseCapture();

                _rightLookActive = false;
            }

            _wasLooking = rightDown;

            // Left-mouse picking dispatch
            bool isLeftClicking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
            
            // Only fire raycast on the initial press (transition from up to down)
            if (isLeftClicking && !_wasLeftClicking && !_pickInProgress)
            {
                GetCursorPos(out POINT clickPos);

                // Convert global monitor coordinates to local window coordinates
                ScreenToClient(this.Handle, ref clickPos);

                // Optional: Ensure the click is actually inside the engine viewport bounds
                if (clickPos.x >= 0 && clickPos.y >= 0 &&
                    clickPos.x <= ActualWidth && clickPos.y <= ActualHeight)
                {
                    if (Application.Current.MainWindow is not MainWindow mainWindow)
                    {
                        _wasLeftClicking = isLeftClicking;
                        return;
                    }

                    Focus(); // Steal focus so keyboard works
                    Keyboard.Focus(this);
                    Debug.WriteLine($"[Viewport] Clicked at {clickPos.x}, {clickPos.y}");
                    long selectionRevision = mainWindow.TreeSelectionRevision;
                    bool selectNodeTarget = mainWindow.NodeTargetRadio.IsChecked == true;
                    _pickInProgress = true;
                    _ = PickAtAsync(
                        clickPos.x,
                        clickPos.y,
                        selectionRevision,
                        selectNodeTarget);
                }
            }
            _wasLeftClicking = isLeftClicking;
        }

        private async Task PickAtAsync(
            float mouseX,
            float mouseY,
            long selectionRevision,
            bool selectNodeTarget)
        {
            try
            {
                var result = await Task.Run(() =>
                {
                    coreEngine.hostPick(
                        mouseX, mouseY,
                        out int modelIndex,
                        out int nodeIndex,
                        out bool isHit);
                    return (modelIndex, nodeIndex, isHit);
                });

                if (Application.Current.MainWindow is not MainWindow mainWindow ||
                    mainWindow.TreeSelectionRevision != selectionRevision)
                    return;

                if (result.isHit && selectNodeTarget)
                {
                    coreEngine.hostSelectNode(result.modelIndex, result.nodeIndex);
                    mainWindow.SelectNodeFromViewport(result.modelIndex, result.nodeIndex);
                }
                else if (result.isHit && mainWindow.ModelTargetRadio.IsChecked == true)
                {
                    coreEngine.hostSelectModel(result.modelIndex);
                    mainWindow.SelectModelFromViewport(result.modelIndex);
                }
                else if (!result.isHit)
                {
                    coreEngine.hostClearSelection();
                    mainWindow.ClearTreeSelectionFromViewport();
                }
            }
            catch (Exception exception)
            {
                Debug.WriteLine($"[Viewport] Picking failed: {exception}");
            }
            finally
            {
                _pickInProgress = false;
            }
        }

        protected override void DestroyWindowCore(HandleRef hwnd)
        {
            CompositionTarget.Rendering -= OnUpdateInput;
            coreEngine.hostDestroy();
            _initialized = false;
        }

        protected override void OnWindowPositionChanged(Rect rcBoundingBox)
        {
            base.OnWindowPositionChanged(rcBoundingBox);

            if (!_initialized) return;

            uint width = (uint)Math.Max(1, rcBoundingBox.Width);
            uint height = (uint)Math.Max(1, rcBoundingBox.Height);

            coreEngine.hostResize(width, height);
        }

        protected override void OnKeyDown(KeyEventArgs e)
        {
            base.OnKeyDown(e);
            SetKeyState(e.Key, true);
            e.Handled = true;
        }

        protected override void OnKeyUp(KeyEventArgs e)
        {
            base.OnKeyUp(e);
            SetKeyState(e.Key, false);
            e.Handled = true;
        }

        protected override void OnMouseDown(MouseButtonEventArgs e)
        {
            base.OnMouseDown(e);
            Keyboard.Focus(this);

            
        }

        private void SetKeyState(Key key, bool isPressed)
        {
            switch (key)
            {
                case Key.W: _forward = isPressed; break;
                case Key.A: _left = isPressed; break;
                case Key.S: _backward = isPressed; break;
                case Key.D: _right = isPressed; break;
                case Key.LeftShift: _up = isPressed; break;
                case Key.LeftCtrl: _down = isPressed; break;
            }
        }
    }
}
