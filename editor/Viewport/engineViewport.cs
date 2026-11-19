using System;
using System.Diagnostics;
using System.Runtime.InteropServices;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Interop;
using System.Windows.Media;
using static rendererUI.MainWindow;

namespace rendererUI
{
    public struct POINT
    {
        public int x, y;
    }

    public struct RECT
    {
        public int left, top, right, bottom;
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

        [DllImport("user32.dll")]
        public static extern bool GetClientRect(IntPtr hWnd, out RECT lpRect);

        private const int VK_LBUTTON = 0x01;
        private bool _wasLeftClicking = false;
        private bool _pickInProgress = false;

        private bool _inputSuspended;
        public bool InputSuspended
        {
            get => _inputSuspended;
            set
            {
                _inputSuspended = value;
                _wasLooking = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
                _wasLeftClicking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

                if (!value) return;

                _forward = _backward = _left = _right = _up = _down = false;
                if (_rightLookActive)
                    ReleaseCapture();
                _rightLookActive = false;
            }
        }

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

            if (InputSuspended)
            {
                _forward = _backward = _left = _right = _up = _down = false;

                if (_rightLookActive)
                    ReleaseCapture();

                _rightLookActive = false;
                _wasLooking = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
                _wasLeftClicking = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
                return;
            }

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
                bool hasRightClickBounds = GetClientRect(Handle, out RECT rightClickBounds);

                bool startedInside =
                    hasRightClickBounds &&
                    clientPos.x >= 0 && clientPos.y >= 0 &&
                    clientPos.x < rightClickBounds.right - rightClickBounds.left &&
                    clientPos.y < rightClickBounds.bottom - rightClickBounds.top;

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
                bool hasLeftClickBounds = GetClientRect(Handle, out RECT leftClickBounds);

                // Optional: Ensure the click is actually inside the engine viewport bounds
                if (hasLeftClickBounds && clickPos.x >= 0 && clickPos.y >= 0 &&
                    clickPos.x < leftClickBounds.right - leftClickBounds.left &&
                    clickPos.y < leftClickBounds.bottom - leftClickBounds.top)
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
                    PickTarget target = mainWindow.CurrentPickTarget;
                    _pickInProgress = true;
                    _ = PickAtAsync(
                        clickPos.x,
                        clickPos.y,
                        selectionRevision,
                        target);
                }
            }
            _wasLeftClicking = isLeftClicking;
        }

        private async Task PickAtAsync(
            float mouseX,
            float mouseY,
            long selectionRevision,
            PickTarget target)
        {
            try
            {
                var result = await Task.Run(() =>
                {
                    coreEngine.hostPick(
                        mouseX, mouseY,
                        out int entityIndex,
                        out int nodeIndex,
                        out bool isHit);
                    return (entityIndex, nodeIndex, isHit);
                });

                if (InputSuspended ||
                    Application.Current.MainWindow is not MainWindow mainWindow ||
                    mainWindow.TreeSelectionRevision != selectionRevision)
                    return;

                if (result.isHit)
                {
                    switch (target)
                    {
                        case PickTarget.Entity:
                            coreEngine.hostSelectEntity(result.entityIndex);
                            mainWindow.SelectEntityFromViewport(result.entityIndex);
                            break;

                        case PickTarget.Model:
                            coreEngine.hostSelectModel(result.entityIndex);
                            mainWindow.SelectModelFromViewport(result.entityIndex);
                            break;

                        case PickTarget.Node:
                            coreEngine.hostSelectNode(result.entityIndex, result.nodeIndex);
                            mainWindow.SelectNodeFromViewport(result.entityIndex, result.nodeIndex);
                            break;
                    }
                }
                else
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
