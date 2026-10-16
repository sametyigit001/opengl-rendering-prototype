using Microsoft.Win32;
using System;
using System.Globalization;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
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

        enum HierarchyItemKind { Entity, Model, ModelNode, Camera, Light }

        record HierarchyTag(
            int EntityId,
            HierarchyItemKind Kind,
            int? NodeIndex = -1);

        private Dictionary<HierarchyTag, TreeViewItem> globalTree = new();
        private TreeViewItem? _contextTreeItem;
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
                selectedItem.Tag is HierarchyTag indexes)
            {
                int entityIndex = indexes.EntityId;
                if (indexes.Kind == HierarchyItemKind.Entity)
                {
                    DeleteEntityFromTree(entityIndex);
                }
                else if (indexes.Kind == HierarchyItemKind.Model)
                {
                    coreEngine.deleteModel(entityIndex);

                    foreach (var key in globalTree.Keys
                                .Where(key => key.EntityId == entityIndex &&
                                (key.Kind == HierarchyItemKind.Model ||
                                key.Kind == HierarchyItemKind.ModelNode))
                                 .ToList())
                    {
                        globalTree.Remove(key);
                    }

                    var entityRoot = HierarchyTree.Items
                        .OfType<TreeViewItem>()
                        .FirstOrDefault(item =>
                            item.Tag is HierarchyTag tag &&
                            tag.EntityId == entityIndex &&
                            tag.Kind == HierarchyItemKind.Entity);

                    entityRoot?.Items.Remove(selectedItem);
                }
            }
        }

        private void DeleteEntityFromTree(int entityIndex)
        {
            coreEngine.hostDeleteEntity(entityIndex);

            foreach (var key in globalTree.Keys
                         .Where(key => key.EntityId == entityIndex)
                         .ToList())
            {
                globalTree.Remove(key);
            }

            var entityRoot = HierarchyTree.Items
                .OfType<TreeViewItem>()
                .FirstOrDefault(item => item.Tag is HierarchyTag tag &&
                                        tag.EntityId == entityIndex &&
                                        tag.Kind == HierarchyItemKind.Entity);

            if (entityRoot != null)
                HierarchyTree.Items.Remove(entityRoot);
        }

        private void HierarchyTree_PreviewMouseRightButtonDown(object sender, MouseButtonEventArgs e)
        {
            _contextTreeItem = null;
            DependencyObject? source = e.OriginalSource as DependencyObject;
            while (source != null && source is not TreeViewItem)
                source = VisualTreeHelper.GetParent(source);

            if (source is TreeViewItem item)
            {
                _contextTreeItem = item;
                item.IsSelected = true;
            }
        }

        private void DeleteEntityContextMenu_Click(object sender, RoutedEventArgs e)
        {
            if (_contextTreeItem?.Tag is HierarchyTag tag)
                DeleteEntityFromTree(tag.EntityId);

            _contextTreeItem = null;
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
        private async Task PopulateHierarchyAsync(int entityIndex, int nodeCount)
        {
            Dictionary<HierarchyTag, TreeViewItem> nodeMap = new Dictionary<HierarchyTag, TreeViewItem>();

            string filenameNoExt = System.IO.Path.GetFileNameWithoutExtension(lastFileName);

            var entityItem = new TreeViewItem
            {
                Header = "[Entity]",
                Tag = new HierarchyTag(entityIndex, HierarchyItemKind.Entity),
                IsExpanded = false
            };

            var modelItem = new TreeViewItem
            {
                Header = $"[Model] {filenameNoExt}",
                Tag = new HierarchyTag(entityIndex, HierarchyItemKind.Model),
                IsExpanded = false
            };

            globalTree[(HierarchyTag)modelItem.Tag] = modelItem;
            entityItem.Items.Add(modelItem);
            nodeMap[(HierarchyTag)entityItem.Tag] = entityItem;
            globalTree[(HierarchyTag)entityItem.Tag] = entityItem;
            HierarchyTree.Items.Add(entityItem);

            for (int i = 0; i < nodeCount; i++)
            {
                if (i > 0 && i % 128 == 0)
                    await Dispatcher.Yield(DispatcherPriority.Background);

                string name = coreEngine.GetNodeName(entityIndex, i);
                int parentId = coreEngine.hostGetNodeParent(entityIndex, i);

                if (string.IsNullOrWhiteSpace(name))
                {
                    name = $"Unnamed_Node_{i}";
                }

                TreeViewItem item = new TreeViewItem
                {
                    Header = name,
                    Tag = new HierarchyTag(entityIndex, HierarchyItemKind.ModelNode, i),
                    IsExpanded = false
                };

                nodeMap[(HierarchyTag)item.Tag] = item;
                globalTree[(HierarchyTag)item.Tag] = item;
                if (parentId == -1)
                {
                    modelItem.Items.Add(item);
                }
                else if (nodeMap.TryGetValue(
                    new HierarchyTag(entityIndex, HierarchyItemKind.ModelNode, parentId),
                    out var parentItem))
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

            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem &&
                selectedItem.Tag is HierarchyTag tag &&
                (tag.Kind == HierarchyItemKind.Entity ||
                 tag.Kind == HierarchyItemKind.Model ||
                 (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)))
            {

                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.ModelNode
                    ? tag.NodeIndex!.Value
                    : -1;

                _shouldUpdatePos = false;
                _shouldUpdateRotation = false;
                _shouldUpdateScale = false;

                btnDeleteSelection.Content = tag.Kind == HierarchyItemKind.Model ? "Remove Model" : "Delete Entity";
                btnDeleteSelection.IsEnabled = tag.Kind == HierarchyItemKind.Entity ||
                                               tag.Kind == HierarchyItemKind.Model;

                try
                {
                    switch ((HierarchyItemKind)tag.Kind)
                    {
                        case HierarchyItemKind.Entity:
                            ModelInspectorSection.Visibility = Visibility.Collapsed;
                            EntityInspectorSection.Visibility = Visibility.Visible;
                            btnDeleteSelection.Visibility = Visibility.Visible;

                            break;

                        case HierarchyItemKind.ModelNode:
                            coreEngine.hostSelectNode(entityIndex, nodeIndex);
                            ModelInspectorSection.Visibility = Visibility.Visible;
                            EntityInspectorSection.Visibility = Visibility.Collapsed;
                            btnDeleteSelection.Visibility = Visibility.Collapsed;
                            if (coreEngine.hostGetNodePos(entityIndex, nodeIndex, out float px, out float py, out float pz) != 0)
                            {
                                PositionXTextBox.Text = px.ToString("F3");
                                PositionYTextBox.Text = py.ToString("F3");
                                PositionZTextBox.Text = pz.ToString("F3");
                            }

                            if (coreEngine.hostGetNodeRotation(entityIndex, nodeIndex, out float rx, out float ry, out float rz) != 0)
                            {
                                RotationXTextBox.Text = rx.ToString("F3");
                                RotationYTextBox.Text = ry.ToString("F3");
                                RotationZTextBox.Text = rz.ToString("F3");
                            }
                            if (coreEngine.hostGetNodeScale(entityIndex, nodeIndex, out float sx, out float sy, out float sz) != 0)
                            {
                                ScaleXTextBox.Text = sx.ToString("F3");
                                ScaleYTextBox.Text = sy.ToString("F3");
                                ScaleZTextBox.Text = sz.ToString("F3");
                            }
                            break;
                        case HierarchyItemKind.Model:
                            coreEngine.hostSelectModel(entityIndex);
                            ModelInspectorSection.Visibility = Visibility.Visible;
                            EntityInspectorSection.Visibility = Visibility.Collapsed;
                            btnDeleteSelection.Visibility = Visibility.Visible;
                            if (coreEngine.hostGetNodePos(entityIndex, nodeIndex, out float m_px, out float m_py, out float m_pz) != 0)
                            {
                                PositionXTextBox.Text = m_px.ToString("F3");
                                PositionYTextBox.Text = m_py.ToString("F3");
                                PositionZTextBox.Text = m_pz.ToString("F3");
                            }

                            if (coreEngine.hostGetNodeRotation(entityIndex, nodeIndex, out float m_rx, out float m_ry, out float m_rz) != 0)
                            {
                                RotationXTextBox.Text = m_rx.ToString("F3");
                                RotationYTextBox.Text = m_ry.ToString("F3");
                                RotationZTextBox.Text = m_rz.ToString("F3");
                            }
                            if (coreEngine.hostGetNodeScale(entityIndex, nodeIndex, out float m_sx, out float m_sy, out float m_sz) != 0)
                            {
                                ScaleXTextBox.Text = m_sx.ToString("F3");
                                ScaleYTextBox.Text = m_sy.ToString("F3");
                                ScaleZTextBox.Text = m_sz.ToString("F3");
                            }
                            break;
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

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;

                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(entityIndex, nodeIndex, x, y, z);

                }

            }
        }

        private void PositionYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(entityIndex, nodeIndex, x, y, z);
                }

            }
        }

        private void PositionZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(entityIndex, nodeIndex, x, y, z);
                }

            }
        }
        public void SelectNodeFromViewport(int entityIndex, int nodeIndex)
        {
            var key = new HierarchyTag(entityIndex, HierarchyItemKind.ModelNode, nodeIndex);
            if (globalTree.TryGetValue(key, out var item))
                item.IsSelected = true;
        }

        public void SelectModelFromViewport(int entityIndex)
        {
            var key = new HierarchyTag(entityIndex, HierarchyItemKind.Model);
            if (globalTree.TryGetValue(key, out var item))
                item.IsSelected = true;
        }
        private void RotationXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdateRotation)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(entityIndex, nodeIndex, x, y, z);
                }

            }
        }

        private void RotationYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {

            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdateRotation)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(entityIndex, nodeIndex, x, y, z);
                }

            }
        }

        private void RotationZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdateRotation)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(entityIndex, nodeIndex, x, y, z);
                }

            }
        }

        private void ScaleXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdateScale)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeScale(entityIndex, nodeIndex, x, y, z);
                }

            }
        }

        private void ScaleYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdateScale)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeScale(entityIndex, nodeIndex, x, y, z);
                }

            }
        }

        private void ScaleZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem)
            {

                if (selectedItem.Tag is HierarchyTag tag && (tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdateScale)
                {
                    int entityIndex = tag.EntityId;
                    int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                        ? -1
                        : tag.NodeIndex!.Value;
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeScale(entityIndex, nodeIndex, x, y, z);
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

        private void CreateEntityButton_Click(object sender, RoutedEventArgs e)
        {
            int id = coreEngine.hostNewEntity();
            if (id < 0)
            {
                ImportStatusText.Text = "Could not create entity.";
                return;
            }

            HierarchyTag tag = new HierarchyTag(id,HierarchyItemKind.Entity);
            var entityItem = new TreeViewItem
            {
                Header = "[Entity]",
                Tag = tag,
                IsExpanded = false
            };
            globalTree.Add(tag,entityItem);
            HierarchyTree.Items.Add(entityItem);
        }

        private void WireframeMode_Click(object sender, RoutedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is not TreeViewItem item ||
                item.Tag is not HierarchyTag tag)
                return;


            if (WireframeCheckBox.IsChecked == true)
                coreEngine.hostEnableWireframe(tag.EntityId);
            else
                coreEngine.hostDisableWireframe(tag.EntityId);
        }

    }
}
