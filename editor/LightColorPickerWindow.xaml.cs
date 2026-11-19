using System;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Input;
using System.Windows.Media;

namespace rendererUI
{
    public partial class LightColorPickerWindow : Window
    {
        private double _hue;
        private double _saturation;
        private double _value;

        public Color SelectedColor { get; private set; }

        public LightColorPickerWindow(Color initialColor)
        {
            InitializeComponent();
            SelectedColor = initialColor;
            RgbToHsv(initialColor, out _hue, out _saturation, out _value);
            Loaded += (_, _) => RefreshPicker();
        }

        private void ColorArea_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            ColorArea.CaptureMouse();
            UpdateColorArea(e.GetPosition(ColorArea));
        }

        private void ColorArea_MouseMove(object sender, MouseEventArgs e)
        {
            if (ColorArea.IsMouseCaptured) UpdateColorArea(e.GetPosition(ColorArea));
        }

        private void ColorArea_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            if (!ColorArea.IsMouseCaptured) return;
            UpdateColorArea(e.GetPosition(ColorArea));
            ColorArea.ReleaseMouseCapture();
        }

        private void HueArea_MouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            HueArea.CaptureMouse();
            UpdateHue(e.GetPosition(HueArea).Y);
        }

        private void HueArea_MouseMove(object sender, MouseEventArgs e)
        {
            if (HueArea.IsMouseCaptured) UpdateHue(e.GetPosition(HueArea).Y);
        }

        private void HueArea_MouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            if (!HueArea.IsMouseCaptured) return;
            UpdateHue(e.GetPosition(HueArea).Y);
            HueArea.ReleaseMouseCapture();
        }

        private void UpdateColorArea(Point point)
        {
            _saturation = Math.Clamp(point.X / ColorArea.ActualWidth, 0, 1);
            _value = 1 - Math.Clamp(point.Y / ColorArea.ActualHeight, 0, 1);
            RefreshPicker();
        }

        private void UpdateHue(double y)
        {
            _hue = Math.Clamp(y / HueArea.ActualHeight, 0, 1) * 360;
            RefreshPicker();
        }

        private void RefreshPicker()
        {
            ColorArea.Background = new SolidColorBrush(HsvToRgb(_hue, 1, 1));
            SelectedColor = HsvToRgb(_hue, _saturation, _value);
            SelectedPreview.Background = new SolidColorBrush(SelectedColor);
            HexText.Text = $"#{SelectedColor.R:X2}{SelectedColor.G:X2}{SelectedColor.B:X2}";
            Canvas.SetLeft(ColorMarker, _saturation * ColorArea.ActualWidth - ColorMarker.Width / 2);
            Canvas.SetTop(ColorMarker, (1 - _value) * ColorArea.ActualHeight - ColorMarker.Height / 2);
            Canvas.SetTop(HueMarker, _hue / 360 * HueArea.ActualHeight - HueMarker.Height / 2);
        }

        private static Color HsvToRgb(double hue, double saturation, double value)
        {
            double chroma = value * saturation;
            double x = chroma * (1 - Math.Abs((hue / 60) % 2 - 1));
            double m = value - chroma;
            double r, g, b;
            if (hue < 60 || hue >= 360) (r, g, b) = (chroma, x, 0);
            else if (hue < 120) (r, g, b) = (x, chroma, 0);
            else if (hue < 180) (r, g, b) = (0, chroma, x);
            else if (hue < 240) (r, g, b) = (0, x, chroma);
            else if (hue < 300) (r, g, b) = (x, 0, chroma);
            else (r, g, b) = (chroma, 0, x);
            return Color.FromRgb((byte)Math.Round((r + m) * 255),
                                 (byte)Math.Round((g + m) * 255),
                                 (byte)Math.Round((b + m) * 255));
        }

        private static void RgbToHsv(Color color, out double hue, out double saturation, out double value)
        {
            double r = color.R / 255.0, g = color.G / 255.0, b = color.B / 255.0;
            double max = Math.Max(r, Math.Max(g, b));
            double min = Math.Min(r, Math.Min(g, b));
            double delta = max - min;
            value = max;
            saturation = max == 0 ? 0 : delta / max;
            if (delta == 0) hue = 0;
            else if (max == r) hue = 60 * (((g - b) / delta + 6) % 6);
            else if (max == g) hue = 60 * ((b - r) / delta + 2);
            else hue = 60 * ((r - g) / delta + 4);
        }

        private void Apply_Click(object sender, RoutedEventArgs e) => DialogResult = true;
        private void Cancel_Click(object sender, RoutedEventArgs e) => DialogResult = false;
    }
}
