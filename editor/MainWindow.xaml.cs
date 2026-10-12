using Microsoft.Win32;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;
using System.Threading.Tasks;


namespace rendererUI
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        bool _shouldUpdatePos = true;
        bool _shouldUpdateRotation = true;
        bool _shouldUpdateScale = true;
        int currentModelIndex = -1;
        string lastFileName = "";
        private readonly DispatcherTimer _renderStatsTimer = new();
        private bool _renderStatsExportAvailable = true;
        private bool _isImporting;
        public long TreeSelectionRevision { get; private set; }

        private Dictionary<(int modelIndex, int nodeIndex), TreeViewItem> globalTree = new();
        int pickedNode;
        private static bool TryParseCoordinate(string text, out float value)
        {
            return float.TryParse(
                       text, NumberStyles.Float, CultureInfo.CurrentCulture, out value)
                || float.TryParse(
                       text.Replace(',', '.'),
                       NumberStyles.Float, CultureInfo.InvariantCulture, out value);
        }
        public MainWindow()
        {
            InitializeComponent();

            _renderStatsTimer.Interval = TimeSpan.FromMilliseconds(500);
            _renderStatsTimer.Tick += UpdateRenderStats;
            _renderStatsTimer.Start();
        }

        private void UpdateRenderStats(object? sender, EventArgs e)
        {
            if (!_renderStatsExportAvailable)
                return;

            try
            {
                coreEngine.hostGetRenderStats(out double fps, out double frameTimeMs);
                EngineFpsText.Text = $"Engine FPS: {fps:F1}";
                EngineFrameTimeText.Text = $"Frame time: {frameTimeMs:F2} ms";
            }
            catch (EntryPointNotFoundException)
            {
                _renderStatsExportAvailable = false;
                EngineFpsText.Text = "Engine FPS: bridge export not available";
                EngineFrameTimeText.Text = "Add hostGetRenderStats to the engine bridge.";
            }
        }

        private async void onImportAsset_Click(object sender, RoutedEventArgs e)
        {
            if (_isImporting)
                return;

            OpenFileDialog openFileDialog = new()
            {
                Title = "Import 3D Asset",
                Filter = "3D Models (*.obj;*.gltf;*.glb;*.fbx)|*.obj;*.gltf;*.glb;*.fbx|All Files (*.*)|*.*",
                CheckFileExists = true,
                Multiselect = false
            };

            if (openFileDialog.ShowDialog() != true)
                return;

            _isImporting = true;
            ImportAssetButton.IsEnabled = false;
            lastFileName = openFileDialog.FileName;
            ImportStatusText.Text = $"Importing {System.IO.Path.GetFileName(lastFileName)}...";

            try
            {
                coreEngine.hostLoadModel(lastFileName);

                int elapsedTenths = 0;
                while (coreEngine.hostIsModelLoadInProgress())
                {
                    await Task.Delay(100);
                    elapsedTenths++;

                    if (elapsedTenths >= 300 && elapsedTenths % 100 == 0)
                    {
                        ImportStatusText.Text =
                            $"Still importing {System.IO.Path.GetFileName(lastFileName)}... {elapsedTenths / 10}s";
                    }
                }

                currentModelIndex = coreEngine.getModelIndex();
                if (currentModelIndex < 0)
                {
                    ImportStatusText.Text = "Import failed; the engine returned no model.";
                    MessageBox.Show("The engine finished loading but returned no model index.", "Import Error");
                    return;
                }

                int nodeCount = coreEngine.hostGetNodeCount(currentModelIndex);
                if (nodeCount > 0)
                {
                    await PopulateHierarchyAsync(currentModelIndex, nodeCount);
                    ImportStatusText.Text = $"Imported {System.IO.Path.GetFileName(lastFileName)}";
                    currentModelIndex = -1;
                }
                else
                {
                    ImportStatusText.Text = "Import completed, but the model has no nodes.";
                    MessageBox.Show("The engine loaded a model with no nodes.", "Import Error");
                }
            }
            finally
            {
                _isImporting = false;
                ImportAssetButton.IsEnabled = true;
                ViewportControl.Focus();
                Keyboard.Focus(ViewportControl);
            }
        }
        private void Button_Click(object sender, RoutedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem &&
                selectedItem.Tag is ValueTuple<int, int> indexes)
            {
                int modelIndex = indexes.Item1;

                coreEngine.deleteModel(modelIndex);

                foreach (var key in globalTree.Keys
                             .Where(key => key.modelIndex == modelIndex)
                             .ToList())
                {
                    globalTree.Remove(key);
                }

                var modelRoot = HierarchyTree.Items
                    .OfType<TreeViewItem>()
                    .FirstOrDefault(item =>
                        item.Tag is ValueTuple<int, int> tag &&
                        tag.Item1 == modelIndex &&
                        tag.Item2 == -1);

                if (modelRoot != null)
                    HierarchyTree.Items.Remove(modelRoot);
            }
        }

        private static bool ParseCoordinateOrBlank(
     string text, float blankDefault, out float value)
        {
            if (string.IsNullOrWhiteSpace(text))
            {
                value = blankDefault;
                return true;
            }

            return TryParseCoordinate(text, out value);
        }
        private async Task PopulateHierarchyAsync(int modelIndex, int nodeCount)
        {
            Dictionary<(int, int), TreeViewItem> nodeMap = new Dictionary<(int, int), TreeViewItem>();

            string filenameNoExt = System.IO.Path.GetFileNameWithoutExtension(lastFileName);


            var modelItem = new TreeViewItem
            {
                Header = "[Model] " + filenameNoExt,
                Tag = (modelIndex, -1),
                IsExpanded = true
            };

            nodeMap[(modelIndex, -1)] = modelItem;
            globalTree[(modelIndex, -1)] = modelItem;
            HierarchyTree.Items.Add(modelItem);

            for (int i = 0; i < nodeCount; i++)
            {
                if (i > 0 && i % 128 == 0)
                    await Dispatcher.Yield(DispatcherPriority.Background);

                string name = coreEngine.GetNodeName(modelIndex, i);
                int parentId = coreEngine.hostGetNodeParent(modelIndex, i);

                if (string.IsNullOrWhiteSpace(name))
                {
                    name = $"Unnamed_Node_{i}";
                }



                TreeViewItem item = new TreeViewItem
                {
                    Header = name,
                    Tag = (modelIndex, i),
                    IsExpanded = false
                };

                nodeMap[(modelIndex, i)] = item;
                globalTree[(modelIndex, i)] = item;

                if (parentId == -1)
                {
                    modelItem.Items.Add(item);
                }
                else if (nodeMap.TryGetValue((modelIndex, parentId), out var parentItem))
                {
                    parentItem.Items.Add(item);
                }
            }
        }
        private void HierarchyTree_PreviewMouseLeftButtonUp(object sender, MouseButtonEventArgs e)
        {
            Dispatcher.BeginInvoke(new Action(() =>
            {
                ViewportControl.Focus();
                Keyboard.Focus(ViewportControl);
            }), DispatcherPriority.Input);
        }

        private void HierarchyTree_SelectedItemChanged(object sender, RoutedPropertyChangedEventArgs<object> e)
        {
            TreeSelectionRevision++;

            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && ((int modelIndex, int nodeIndex))selectedItem.Tag is var indexes)
            {
                _shouldUpdatePos = false;
                _shouldUpdateRotation = false;
                _shouldUpdateScale = false;

                try
                {
                    if (coreEngine.hostSelectNode(indexes.modelIndex, indexes.nodeIndex) == 0)
                        return;
                    if (indexes.nodeIndex == -1)
                    {
                        coreEngine.hostSelectModel(indexes.modelIndex);
                    }

                    if (coreEngine.hostGetNodePos(indexes.modelIndex, indexes.nodeIndex, out float px, out float py, out float pz) != 0)
                    {
                        PositionXTextBox.Text = px.ToString("F3");
                        PositionYTextBox.Text = py.ToString("F3");
                        PositionZTextBox.Text = pz.ToString("F3");
                    }

                    if (coreEngine.hostGetNodeRotation(indexes.modelIndex, indexes.nodeIndex, out float rx, out float ry, out float rz) != 0)
                    {
                        RotationXTextBox.Text = rx.ToString("F3");
                        RotationYTextBox.Text = ry.ToString("F3");
                        RotationZTextBox.Text = rz.ToString("F3");
                    }
                    if (coreEngine.hostGetNodeScale(indexes.modelIndex, indexes.nodeIndex, out float sx, out float sy, out float sz) != 0)
                    {
                        ScaleXTextBox.Text = sx.ToString("F3");
                        ScaleYTextBox.Text = sy.ToString("F3");
                        ScaleZTextBox.Text = sz.ToString("F3");
                    }
                }
                finally
                {
                    _shouldUpdatePos = true;
                    _shouldUpdateRotation = true;
                    _shouldUpdateScale = true;
                }
            }
        }

        private void PositionXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(indexes.ModelIndex, indexes.NodeIndex, x, y, z);

                }

            }
        }

        private void PositionYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdatePos)
                {

                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }

        private void PositionZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }
        public void SelectNodeFromViewport(int modelIndex, int nodeIndex)
        {
            if (globalTree.TryGetValue((modelIndex, nodeIndex), out var item))
                item.IsSelected = true;
        }
        public void SelectModelFromViewport(int modelIndex)
        {
            if (globalTree.TryGetValue((modelIndex, -1), out var item))
                item.IsSelected = true;
        }

        private void RotationXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdateRotation)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }

        private void RotationYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {

            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdateRotation)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }

        private void RotationZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdateRotation)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }

        private void ScaleXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdateScale)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeScale(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }

        private void ScaleYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdateScale)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeScale(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }

        private void ScaleZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (((int ModelIndex, int NodeIndex))selectedItem.Tag is var indexes && _shouldUpdateScale)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeScale(indexes.ModelIndex, indexes.NodeIndex, x, y, z);
                }

            }
        }

        public void ClearTreeSelectionFromViewport()
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
                selectedItem.IsSelected = false;
        }

        private static bool IsAllowedNumberText(string text) =>
        Regex.IsMatch(text, @"^-?[0-9]*([.,][0-9]*)?$");

        private static string CandidateText(TextBox box, string insertedText) =>
            box.Text.Remove(box.SelectionStart, box.SelectionLength)
                     .Insert(box.SelectionStart, insertedText);

        private void NumericTextBox_PreviewTextInput(
            object sender, TextCompositionEventArgs e)
        {
            var box = (TextBox)sender;
            e.Handled = !IsAllowedNumberText(CandidateText(box, e.Text));
        }

        private void NumericTextBox_Pasting(object sender, DataObjectPastingEventArgs e)
        {
            if (sender is not TextBox box ||
                !e.DataObject.GetDataPresent(DataFormats.UnicodeText) ||
                e.DataObject.GetData(DataFormats.UnicodeText) is not string pasted ||
                !IsAllowedNumberText(CandidateText(box, pasted)))
            {
                e.CancelCommand();
            }
        }

        private void SelectedNodeNameTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            {
                if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
                    selectedItem.Header = SelectedNodeNameTextBox.Text;
            }
        }


        private void WireframeMode_Click(object sender, RoutedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is not TreeViewItem item ||
                item.Tag is not ValueTuple<int, int> indexes)
                return;

            if (WireframeCheckBox.IsChecked == true)
                coreEngine.hostEnableWireframe(indexes.Item1);
            else
                coreEngine.hostDisableWireframe(indexes.Item1);
        }

    }
}
