using Microsoft.Win32;
using System;
using System.Globalization;
using System.Reflection.Metadata.Ecma335;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
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
        bool _shouldUpdateEntityCam = true;
        bool _shouldUpdateLight = true;
        int currentEntityIndex = -1;
        int dragCandidate = -2;
        int draggingEntity = -2;
        Point dragStart;
        string lastFileName = "";
        private readonly DispatcherTimer _renderStatsTimer = new();
        private bool _renderStatsExportAvailable = true;
        private bool _isImporting;
        private bool _refreshingViewportCameraFields;
        public long TreeSelectionRevision { get; private set; }

        enum HierarchyItemKind { Entity, Model, ModelNode, Camera, Light }

        public enum PickTarget
        {
            Entity,
            Model,
            Node
        }

        public PickTarget CurrentPickTarget => EntityTargetRadio.IsChecked == true ?
               PickTarget.Entity : NodeTargetRadio.IsChecked == true ?
               PickTarget.Node : PickTarget.Model;

        record HierarchyTag(
            int EntityId,
            HierarchyItemKind Kind,
            int? NodeIndex = -1);

        private Dictionary<HierarchyTag, TreeViewItem> globalTree = new();
        private TreeViewItem? _contextTreeItem;
        int pickedNode;
        private static bool TryParseCoordinate(string text, out float value)
        {
            return (float.TryParse(
                       text, NumberStyles.Float, CultureInfo.CurrentCulture, out value)
                || float.TryParse(
                       text.Replace(',', '.'),
                       NumberStyles.Float, CultureInfo.InvariantCulture, out value)) && float.IsFinite(value);
        }
        public MainWindow()
        {
            InitializeComponent();

            _renderStatsTimer.Interval = TimeSpan.FromMilliseconds(500);
            _renderStatsTimer.Tick += UpdateRenderStats;
            _renderStatsTimer.Start();
        }

        private T ShowModal<T>(Func<T> show)
        {
            bool wasSuspended = ViewportControl.InputSuspended;
            ViewportControl.InputSuspended = true;
            try
            {
                return show();
            }
            finally
            {
                ViewportControl.InputSuspended = wasSuspended;
            }
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

            if (ShowModal(() => openFileDialog.ShowDialog()) != true)
                return;

            _isImporting = true;
            ImportAssetButton.IsEnabled = false;
            lastFileName = openFileDialog.FileName;
            ImportStatusText.Text = $"Importing {System.IO.Path.GetFileName(lastFileName)}...";

            try
            {
                coreEngine.hostLoadModel(lastFileName, -1);

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

                currentEntityIndex = coreEngine.hostGetImportedEntityIndex();
                if (currentEntityIndex < 0)
                {
                    ImportStatusText.Text = "Import failed; the engine returned no model.";
                    ShowModal(() => MessageBox.Show("The engine finished loading but returned no entity index.", "Import Error"));
                    return;
                }

                int nodeCount = coreEngine.hostGetNodeCount(currentEntityIndex);
                if (nodeCount > 0)
                {
                    await PopulateHierarchyAsync(currentEntityIndex, nodeCount);
                    ImportStatusText.Text = $"Imported {System.IO.Path.GetFileName(lastFileName)}";
                    currentEntityIndex = -1;
                }
                else
                {
                    ImportStatusText.Text = "Import completed, but the model has no nodes.";
                    ShowModal(() => MessageBox.Show("The engine loaded a model with no nodes.", "Import Error"));
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
                    AddModelButton.IsEnabled = true;
                }
                else if (indexes.Kind == HierarchyItemKind.Camera)
                {
                    coreEngine.hostDeleteCamera(entityIndex);
                    globalTree.Remove(indexes);
                    if (selectedItem.Parent is TreeViewItem owner)
                    {
                        owner.Items.Remove(selectedItem);
                        owner.IsSelected = true;
                    }
                    AddCameraButton.IsEnabled = true;
                }
                else if(indexes.Kind == HierarchyItemKind.Light)
                {
                    coreEngine.hostDeleteLight(entityIndex);
                    globalTree.Remove(indexes);
                    if(selectedItem.Parent is TreeViewItem owner)
                    {
                        owner.Items.Remove(selectedItem);
                        owner.IsSelected = true;
                    }
                    AddLightButton.IsEnabled = true;
                }
            }
        }

        private void DeleteEntityFromTree(int entityIndex)
        {
            if (!globalTree.TryGetValue(new HierarchyTag(entityIndex, HierarchyItemKind.Entity), out var entityRoot)) return;
            var deletedIds = new HashSet<int>();
            var pending = new Stack<TreeViewItem>();
            pending.Push(entityRoot);
            while (pending.Count > 0)
            {
                var item = pending.Pop();
                if (item.Tag is HierarchyTag tag) deletedIds.Add(tag.EntityId);
                foreach (TreeViewItem child in item.Items) pending.Push(child);
            }
            coreEngine.hostDeleteEntity(entityIndex);
            foreach (var key in globalTree.Keys.Where(key => deletedIds.Contains(key.EntityId)).ToList())
                globalTree.Remove(key);

            if (entityRoot.Parent is TreeViewItem parent)
            {
                parent.Items.Remove(entityRoot);
                parent.IsSelected = true;
            }
            else
            {
                HierarchyTree.Items.Remove(entityRoot);
                EntityInspectorSection.Visibility = Visibility.Collapsed;
                ModelInspectorSection.Visibility = Visibility.Collapsed;
                CameraInspectorSection.Visibility = Visibility.Collapsed;
                btnDeleteSelection.Visibility = Visibility.Collapsed;
            }
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
        private async Task addModelToEntityAsync(int entityIndex, int nodeCount)
        {
            Dictionary<HierarchyTag, TreeViewItem> nodeMap = new Dictionary<HierarchyTag, TreeViewItem>();

            string filenameNoExt = System.IO.Path.GetFileNameWithoutExtension(lastFileName);

            HierarchyTag tag = new HierarchyTag(entityIndex, HierarchyItemKind.Entity);


            TreeViewItem entityItem = globalTree[tag];

            var modelItem = new TreeViewItem
            {
                Header = $"[Model] {filenameNoExt}",
                Tag = new HierarchyTag(entityIndex, HierarchyItemKind.Model),
                IsExpanded = false
            };

            globalTree[(HierarchyTag)modelItem.Tag] = modelItem;
            entityItem.Items.Add(modelItem);

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
            dragCandidate = -2;
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
                (tag.Kind == HierarchyItemKind.Entity || tag.Kind == HierarchyItemKind.Light || tag.Kind == HierarchyItemKind.Camera ||tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)))
            {

                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.ModelNode
                    ? tag.NodeIndex!.Value
                    : -1;

                _shouldUpdatePos = false;
                _shouldUpdateRotation = false;
                _shouldUpdateScale = false;
                _shouldUpdateEntityCam = false;
                _shouldUpdateLight = false;

                if (tag.Kind == HierarchyItemKind.Entity)
                {
                    btnDeleteSelection.Content = "Delete Entity";
                }
                else if(tag.Kind == HierarchyItemKind.Model)
                {
                    btnDeleteSelection.Content = "Delete Model";
                }
                else if(tag.Kind == HierarchyItemKind.Camera)
                {
                    btnDeleteSelection.Content = "Delete Camera";
                }
                else if(tag.Kind == HierarchyItemKind.Light)
                {
                    btnDeleteSelection.Content = "Delete Light";
                }

                    //btnDeleteSelection.Content = tag.Kind == HierarchyItemKind.Model ? "Remove Model" : "Delete Entity";
                    btnDeleteSelection.IsEnabled = tag.Kind == HierarchyItemKind.Entity ||
                    tag.Kind == HierarchyItemKind.Model || tag.Kind == HierarchyItemKind.Light ||tag.Kind == HierarchyItemKind.Camera;

                try
                {
                    LightInspectorSection.Visibility = tag.Kind == HierarchyItemKind.Light ? Visibility.Visible : Visibility.Collapsed;
                    AddLightButton.IsEnabled = !globalTree.ContainsKey(new HierarchyTag(entityIndex, HierarchyItemKind.Light));

                    AddCameraButton.IsEnabled = !globalTree.ContainsKey(new HierarchyTag(entityIndex, HierarchyItemKind.Camera));
                    switch ((HierarchyItemKind)tag.Kind)
                    {
                        case HierarchyItemKind.Entity:
                            ModelInspectorSection.Visibility = Visibility.Collapsed;
                            EntityInspectorSection.Visibility = Visibility.Visible;
                            btnDeleteSelection.Visibility = Visibility.Visible;
                            CameraInspectorSection.Visibility = Visibility.Collapsed;
                            if (coreEngine.hostGetEntityPos(entityIndex, out float e_px, out float e_py, out float e_pz) != 0)
                            {
                                PositionXTextBox.Text = e_px.ToString("F3");
                                PositionYTextBox.Text = e_py.ToString("F3");
                                PositionZTextBox.Text = e_pz.ToString("F3");
                            }

                            if (coreEngine.hostGetEntityRotation(entityIndex, out float e_rx, out float e_ry, out float e_rz) != 0)
                            {
                                RotationXTextBox.Text = e_rx.ToString("F3");
                                RotationYTextBox.Text = e_ry.ToString("F3");
                                RotationZTextBox.Text = e_rz.ToString("F3");
                            }
                            if (coreEngine.hostGetEntityScale(entityIndex, out float e_sx, out float e_sy, out float e_sz) != 0)
                            {
                                ScaleXTextBox.Text = e_sx.ToString("F3");
                                ScaleYTextBox.Text = e_sy.ToString("F3");
                                ScaleZTextBox.Text = e_sz.ToString("F3");
                            }
                            break;

                        case HierarchyItemKind.ModelNode:
                            coreEngine.hostSelectNode(entityIndex, nodeIndex);
                            ModelInspectorSection.Visibility = Visibility.Visible;
                            EntityInspectorSection.Visibility = Visibility.Collapsed;
                            btnDeleteSelection.Visibility = Visibility.Collapsed;
                            CameraInspectorSection.Visibility = Visibility.Collapsed;
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
                            CameraInspectorSection.Visibility = Visibility.Collapsed;
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
                        case HierarchyItemKind.Camera:
                            CameraInspectorSection.Visibility = Visibility.Visible;
                            btnDeleteSelection.Visibility = Visibility.Visible;
                            EntityInspectorSection.Visibility = Visibility.Collapsed;
                            ModelInspectorSection.Visibility = Visibility.Collapsed;
                            if (coreEngine.hostGetEntityCamPos(entityIndex, out float c_px, out float c_py, out float c_pz) != 0)
                            {
                                PositionXTextBox.Text = c_px.ToString("F3");
                                PositionYTextBox.Text = c_py.ToString("F3");
                                PositionZTextBox.Text = c_pz.ToString("F3");
                            }
                            if (coreEngine.hostGetEntityCamRotation(entityIndex, out float c_rx, out float c_ry, out float c_rz) != 0)
                            {
                                RotationXTextBox.Text = c_rx.ToString("F3");
                                RotationYTextBox.Text = c_ry.ToString("F3");
                                RotationZTextBox.Text = c_rz.ToString("F3");
                            }
                            if(coreEngine.hostGetSelectedCamera(tag.EntityId, out float fov, out float speed, out float nearPlane,out float farPlane) != 0)
                            {
                                CameraFovTextBox.Text = fov.ToString("F3");
                                CameraSpeedTextBox.Text = speed.ToString("F3");
                                CameraNearTextBox.Text = nearPlane.ToString("F3");
                                CameraFarTextBox.Text = farPlane.ToString("F3");

                            }
                            break;
                        case HierarchyItemKind.Light:
                            CameraInspectorSection.Visibility = Visibility.Collapsed;
                            btnDeleteSelection.Visibility = Visibility.Visible;
                            EntityInspectorSection.Visibility = Visibility.Collapsed;
                            ModelInspectorSection.Visibility = Visibility.Collapsed;
                            LightInspectorSection.Visibility = Visibility.Visible;
                            if (coreEngine.hostGetLight(tag.EntityId, out LightConfig config))
                            {
                                LightTypeComboBox.SelectedIndex = (int)config.type;

                                Color color = Color.FromRgb((byte)Math.Clamp(config.color.x * 255f, 0f, 255f),(byte)Math.Clamp(config.color.y * 255f, 0f, 255f),(byte)Math.Clamp(config.color.z * 255f, 0f, 255f));
                                SolidColorBrush brush = new SolidColorBrush(color);
                                LightColorPreview.Background = brush;

                                LightIntensitySlider.Value = config.intensity;
                                LightDirectionXTextBox.Text = config.direction.x.ToString("F3");
                                LightDirectionYTextBox.Text = config.direction.y.ToString("F3");
                                LightDirectionZTextBox.Text = config.direction.z.ToString("F3");

                                LightPositionXTextBox.Text = config.position.x.ToString("F3");
                                LightPositionYTextBox.Text = config.position.y.ToString("F3");
                                LightPositionZTextBox.Text = config.position.z.ToString("F3");

                                LightAmbientStrengthTextBox.Text = config.ambientStr.ToString("F3");
                                LightDiffuseStrengthTextBox.Text = config.diffuseStr.ToString("F3");
                                LightSpecularStrengthTextBox.Text = config.specularStr.ToString("F3");

                                LightCutoffTextBox.Text = (MathF.Acos(Math.Clamp(config.cutOff, -1f, 1f)) * 180f / MathF.PI).ToString("F3");
                                LightOuterCutoffTextBox.Text = (MathF.Acos(Math.Clamp(config.outerCutOff, -1f, 1f)) * 180f / MathF.PI).ToString("F3");

                                LightLinearTextBox.Text = config.linear.ToString("F3");
                                LightConstantTextBox.Text = config.constant.ToString("F3");
                                LightQuadraticTextBox.Text = config.quadratic.ToString("F3");
                            }
                            break;

                    }
                }
                finally
                {
                    _shouldUpdatePos = true;
                    _shouldUpdateRotation = true;
                    _shouldUpdateScale = true;
                    _shouldUpdateEntityCam = true;
                    _shouldUpdateLight = true;
                }
            }
        }


        private void PositionXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {


                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetEntityPos(entityIndex, x, y, z);
                }
                else if(tag.Kind == HierarchyItemKind.Camera && _shouldUpdateEntityCam)
                {
                    if (!ParseCoordinateOrBlank(PositionXTextBox.Text,0f, out float x) ||
                        !ParseCoordinateOrBlank(PositionYTextBox.Text,0f, out float y) ||
                        !ParseCoordinateOrBlank(PositionZTextBox.Text,0f, out float z)) return;
                    coreEngine.hostSetEntityCamPos(entityIndex, x, y, z);
                }

            }
        }

        private void PositionYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {


                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetEntityPos(entityIndex, x, y, z);
                }
                else if (tag.Kind == HierarchyItemKind.Camera && _shouldUpdateEntityCam)
                {
                    if (!ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out float x) ||
                        !ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out float y) ||
                        !ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out float z)) return;
                    coreEngine.hostSetEntityCamPos(entityIndex, x, y, z);
                }
            }
        }

        private void PositionZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {


                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodePos(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out z);

                    coreEngine.hostSetEntityPos(entityIndex, x, y, z);
                }
                else if (tag.Kind == HierarchyItemKind.Camera && _shouldUpdateEntityCam)
                {
                    if (!ParseCoordinateOrBlank(PositionXTextBox.Text, 0f, out float x) ||
                        !ParseCoordinateOrBlank(PositionYTextBox.Text, 0f, out float y) ||
                        !ParseCoordinateOrBlank(PositionZTextBox.Text, 0f, out float z)) return;
                    coreEngine.hostSetEntityCamPos(entityIndex, x, y, z);
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
        public void SelectEntityFromViewport(int entityIndex)
        {
            var key = new HierarchyTag(entityIndex, HierarchyItemKind.Entity);
            if (globalTree.TryGetValue(key, out var item))
                item.IsSelected = true;
        }
        private void RotationXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {


                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetEntityRotation(entityIndex, x, y, z);
                }
                else if (tag.Kind == HierarchyItemKind.Camera && _shouldUpdateEntityCam)
                {
                    if (!ParseCoordinateOrBlank(RotationXTextBox.Text,0f, out float x) ||
                        !ParseCoordinateOrBlank(RotationYTextBox.Text,0f, out float y) ||
                        !ParseCoordinateOrBlank(RotationZTextBox.Text,0f, out float z)) return;
                    coreEngine.hostSetEntityCamRotation(entityIndex, x, y, z);
                }
            }
        }

        private void RotationYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {

            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {


                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetEntityRotation(entityIndex, x, y, z);
                }
                else if (tag.Kind == HierarchyItemKind.Camera && _shouldUpdateEntityCam)
                {
                    if (!ParseCoordinateOrBlank(RotationXTextBox.Text,0f, out float x) ||
                        !ParseCoordinateOrBlank(RotationYTextBox.Text,0f, out float y) ||
                        !ParseCoordinateOrBlank(RotationZTextBox.Text,0f, out float z)) return;
                    coreEngine.hostSetEntityCamRotation(entityIndex, x, y, z);
                }
            }
        }

        private void RotationZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {


                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetNodeRotation(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {
                    float x, y, z;
                    ParseCoordinateOrBlank(RotationXTextBox.Text, 0f, out x);
                    ParseCoordinateOrBlank(RotationYTextBox.Text, 0f, out y);
                    ParseCoordinateOrBlank(RotationZTextBox.Text, 0f, out z);

                    coreEngine.hostSetEntityRotation(entityIndex, x, y, z);
                }
                else if (tag.Kind == HierarchyItemKind.Camera && _shouldUpdateEntityCam)
                {
                    if (!ParseCoordinateOrBlank(RotationXTextBox.Text,0f, out float x) ||
                        !ParseCoordinateOrBlank(RotationYTextBox.Text,0f, out float y) ||
                        !ParseCoordinateOrBlank(RotationZTextBox.Text,0f, out float z)) return;
                    coreEngine.hostSetEntityCamRotation(entityIndex, x, y, z);
                }
            }
        }

        private void ScaleXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {

                    if (UniformScaleCheckBox.IsChecked == true)
                    {
                        ScaleYTextBox.Text = ScaleXTextBox.Text;
                        ScaleZTextBox.Text = ScaleXTextBox.Text;

                    }
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 1f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 1f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 1f, out z);

                    coreEngine.hostSetNodeScale(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {

                    if (UniformScaleCheckBox.IsChecked == true)
                    {
                        ScaleYTextBox.Text = ScaleXTextBox.Text;
                        ScaleZTextBox.Text = ScaleXTextBox.Text;

                    }
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 1f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 1f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 1f, out z);

                    coreEngine.hostSetEntityScale(entityIndex, x, y, z);
                }

            }
        }

        private void ScaleYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {

                    if (UniformScaleCheckBox.IsChecked == true)
                    {
                        ScaleXTextBox.Text = ScaleYTextBox.Text;
                        ScaleZTextBox.Text = ScaleYTextBox.Text;

                    }
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 1f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 1f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 1f, out z);

                    coreEngine.hostSetNodeScale(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {

                    if (UniformScaleCheckBox.IsChecked == true)
                    {
                        ScaleXTextBox.Text = ScaleYTextBox.Text;
                        ScaleZTextBox.Text = ScaleYTextBox.Text;

                    }
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 1f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 1f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 1f, out z);

                    coreEngine.hostSetEntityScale(entityIndex, x, y, z);
                }

            }
        }

        private void ScaleZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag)
            {
                int entityIndex = tag.EntityId;
                int nodeIndex = tag.Kind == HierarchyItemKind.Entity
                    ? -1
                    : tag.NodeIndex!.Value;

                if ((tag.Kind == HierarchyItemKind.Model || (tag.Kind == HierarchyItemKind.ModelNode && tag.NodeIndex.HasValue)) && _shouldUpdatePos)
                {
                    if (UniformScaleCheckBox.IsChecked == true)
                    {
                        ScaleXTextBox.Text = ScaleZTextBox.Text;
                        ScaleYTextBox.Text = ScaleZTextBox.Text;

                    }

                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 1f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 1f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 1f, out z);

                    coreEngine.hostSetNodeScale(entityIndex, nodeIndex, x, y, z);

                }
                else if (tag.Kind == HierarchyItemKind.Entity && _shouldUpdatePos)
                {

                    if (UniformScaleCheckBox.IsChecked == true)
                    {
                        ScaleXTextBox.Text = ScaleZTextBox.Text;
                        ScaleYTextBox.Text = ScaleZTextBox.Text;

                    }
                    float x, y, z;
                    ParseCoordinateOrBlank(ScaleXTextBox.Text, 1f, out x);
                    ParseCoordinateOrBlank(ScaleYTextBox.Text, 1f, out y);
                    ParseCoordinateOrBlank(ScaleZTextBox.Text, 1f, out z);

                    coreEngine.hostSetEntityScale(entityIndex, x, y, z);
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

        private void EntityNameTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (sender is TextBox nameBox &&
                HierarchyTree.SelectedItem is TreeViewItem selectedItem &&
                selectedItem.Tag is HierarchyTag tag &&
                tag.Kind == HierarchyItemKind.Entity)
            {
                selectedItem.Header = nameBox.Text;
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

            HierarchyTag tag = new HierarchyTag(id, HierarchyItemKind.Entity);
            var entityItem = new TreeViewItem
            {
                Header = "[Entity]",
                Tag = tag,
                IsExpanded = false
            };
            globalTree.Add(tag, entityItem);
            HierarchyTree.Items.Add(entityItem);
        }

        private async void AddModelButton_Click(object sender, RoutedEventArgs e)
        {
            if (_isImporting)
                return;
            if (HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind is HierarchyItemKind.Entity)
            {
                if (!item.IsSelected) return;
                OpenFileDialog openFileDialog = new()
                {
                    Title = "Import 3D Asset",
                    Filter = "3D Models (*.obj;*.gltf;*.glb;*.fbx)|*.obj;*.gltf;*.glb;*.fbx|All Files (*.*)|*.*",
                    CheckFileExists = true,
                    Multiselect = false
                };

                if (ShowModal(() => openFileDialog.ShowDialog()) != true)
                    return;

                _isImporting = true;
                AddModelButton.IsEnabled = false;
                lastFileName = openFileDialog.FileName;
                ImportStatusText.Text = $"Importing {System.IO.Path.GetFileName(lastFileName)}...";

                try
                {

                    coreEngine.hostLoadModel(lastFileName, tag.EntityId);

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

                    currentEntityIndex = tag.EntityId;
                    if (currentEntityIndex < 0)
                    {
                        ImportStatusText.Text = "Import failed; the engine returned no model.";
                        ShowModal(() => MessageBox.Show("The engine finished loading but returned no entity index.", "Import Error"));
                        return;
                    }
                    int loadedEntityIndex = coreEngine.hostGetImportedEntityIndex();
                    if (currentEntityIndex != loadedEntityIndex)
                    {
                        return;
                    }
                    int nodeCount = coreEngine.hostGetNodeCount(currentEntityIndex);
                    if (nodeCount > 0)
                    {
                        await addModelToEntityAsync(currentEntityIndex, nodeCount);
                        ImportStatusText.Text = $"Imported {System.IO.Path.GetFileName(lastFileName)}";
                        currentEntityIndex = -1;
                    }
                    else
                    {
                        ImportStatusText.Text = "Import completed, but the model has no nodes.";
                        ShowModal(() => MessageBox.Show("The engine loaded a model with no nodes.", "Import Error"));
                    }

                }
                finally
                {
                    _isImporting = false;
                    AddModelButton.IsEnabled = true;
                    ViewportControl.Focus();
                    Keyboard.Focus(ViewportControl);
                }
            }
        }

        private void AddCameraButton_Click(object sender, RoutedEventArgs e)
        {
            if(HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind == HierarchyItemKind.Entity)
            {

                AddCameraButton.IsEnabled = false;
                if (coreEngine.hostAddCamera(tag.EntityId) == 0) return;
                var cameraTag = new HierarchyTag(tag.EntityId,HierarchyItemKind.Camera);

                var cameraItem = new TreeViewItem
                {
                    Header = "[Camera]",
                    Tag = cameraTag,
                };

                globalTree.Add(cameraTag, cameraItem);
                item.Items.Add(cameraItem);
                item.IsExpanded = true;
                cameraItem.IsSelected = true;
            }
        }

        private void AddLightButton_Click(object sender, RoutedEventArgs e)
        {
            if(HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind == HierarchyItemKind.Entity)
            {
                AddLightButton.IsEnabled = false;
               if(!coreEngine.hostAddLight(tag.EntityId)) return;
                var lightTag = new HierarchyTag(tag.EntityId, HierarchyItemKind.Light);
                var lightItem = new TreeViewItem
                {
                    Header = "[light]",
                    Tag = lightTag
                };
                globalTree.Add(lightTag, lightItem);
                item.Items.Add(lightItem);
                item.IsExpanded = true;
                lightItem.IsSelected = true;
            }
        }

        private void HierarchyTree_PreviewMouseMove(object sender, MouseEventArgs e)
        {
            if (e.LeftButton == MouseButtonState.Pressed && dragCandidate != -2)
            {
                Point current = e.GetPosition(HierarchyTree);

                bool isEnough = Math.Abs(dragStart.X - current.X) > SystemParameters.MinimumHorizontalDragDistance || Math.Abs(dragStart.Y - current.Y) > SystemParameters.MinimumVerticalDragDistance;
                if (isEnough)
                {
                    int draggedId = dragCandidate;
                    dragCandidate = -2;
                    draggingEntity = draggedId;
                    bool wasSuspended = ViewportControl.InputSuspended;
                    ViewportControl.InputSuspended = true;
                    try
                    {
                        DragDrop.DoDragDrop(HierarchyTree, draggedId, DragDropEffects.Move);
                    }
                    finally
                    {
                        dragCandidate = -2;
                        draggingEntity = -2;
                        ViewportControl.InputSuspended = wasSuspended;
                    }
                }
            }

        }

        private void HierarchyTree_PreviewMouseLeftButtonDown(object sender, MouseButtonEventArgs e)
        {
            dragCandidate = -2;
            DependencyObject? source = e.OriginalSource as DependencyObject;
            while (source != null && source is not TreeViewItem)
            {
                if (source is ToggleButton || source is ScrollBar) return;
                source = VisualTreeHelper.GetParent(source);
            }

            if (source is TreeViewItem item && item.Tag is HierarchyTag Tag && Tag.Kind == HierarchyItemKind.Entity)
            {
                dragCandidate = Tag.EntityId;
                dragStart = e.GetPosition(HierarchyTree);
            }
        }

        private void HierarchyTree_DragOver(object sender, DragEventArgs e)
        {
            e.Effects = DragDropEffects.None;
            e.Handled = true;

            if (draggingEntity != -2)
            {
                int draggedId = draggingEntity;
                if (!globalTree.TryGetValue(new HierarchyTag(draggedId, HierarchyItemKind.Entity), out var childRow)) return;

                DependencyObject? source = e.OriginalSource as DependencyObject;
                while (source != null &&
                       source is not TreeViewItem &&
                       source != HierarchyTree)
                {
                    if (source is ScrollBar) return;
                    source = VisualTreeHelper.GetParent(source);
                }

                if (source is TreeViewItem item)
                {
                    if (item.Tag is not HierarchyTag tag || tag.Kind != HierarchyItemKind.Entity) return;
                    for (ItemsControl? parent = item; parent is TreeViewItem; parent = ItemsControl.ItemsControlFromItemContainer(parent))
                    {
                        if (parent == childRow) return;
                    }
                }
                else if (source != HierarchyTree) return;

                ItemsControl oldOwner = ItemsControl.ItemsControlFromItemContainer(childRow);
                if (oldOwner == null || oldOwner == source) return;
                e.Effects = DragDropEffects.Move;
            }
        }

        private void HierarchyTree_Drop(object sender, DragEventArgs e)
        {
            e.Effects = DragDropEffects.None;
            e.Handled = true;
            if (draggingEntity != -2)
            {
                int draggedId = draggingEntity;
                if (!globalTree.TryGetValue(new HierarchyTag(draggedId, HierarchyItemKind.Entity), out var childRow)) return;

                DependencyObject? source = e.OriginalSource as DependencyObject;
                while (source != null &&
                       source is not TreeViewItem &&
                       source != HierarchyTree)
                {
                    if (source is ScrollBar) return;
                    source = VisualTreeHelper.GetParent(source);
                }

                int parentId = -1;
                ItemsControl ParentRow = HierarchyTree;
                if (source is TreeViewItem item)
                {
                    if (item.Tag is not HierarchyTag tag || tag.Kind != HierarchyItemKind.Entity) return;
                    for (ItemsControl? parent = item; parent is TreeViewItem; parent = ItemsControl.ItemsControlFromItemContainer(parent))
                    {
                        if (parent == childRow) return;
                    }
                    parentId = tag.EntityId;
                    ParentRow = item;
                }
                else if (source != HierarchyTree) return;

                ItemsControl oldOwner = ItemsControl.ItemsControlFromItemContainer(childRow);
                if (oldOwner == null || oldOwner == ParentRow) return;
                if (!coreEngine.hostSetParent(draggedId, parentId)) return;

                oldOwner.Items.Remove(childRow);
                ParentRow.Items.Add(childRow);
                if (ParentRow is TreeViewItem parentRow) parentRow.IsExpanded = true;
                childRow.IsSelected = true;
                childRow.BringIntoView();
                e.Effects = DragDropEffects.Move;
            }
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

        private void RightPanelTabs_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if(e.Source == RightPanelTabs && RightPanelTabs.SelectedItem == ViewportTab)
            {
                refreshViewportCamera();
            }
        }

        private void UseSelectedCameraButton_Click(object sender, RoutedEventArgs e)
        {
            if(HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind is HierarchyItemKind.Camera)
            {
                coreEngine.hostSwitchCamera(tag.EntityId);
            }
        }

        private void UseEditorCameraButton_Click(object sender, RoutedEventArgs e)
        {
            coreEngine.hostSwitchToEditor();
        }

        private void CameraFovTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (!IsLoaded || !_shouldUpdateEntityCam) return;
            if (HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind == HierarchyItemKind.Camera &&
                ParseCoordinateOrBlank(CameraFovTextBox.Text,0f, out float fov) &&
                 ParseCoordinateOrBlank(CameraSpeedTextBox.Text,0f, out float speed) &&
                 ParseCoordinateOrBlank(CameraNearTextBox.Text,0f, out float near) &&
                 ParseCoordinateOrBlank(CameraFarTextBox.Text,0f, out float far) &&
                fov > 0f && fov < 180f && speed > 0f && near > 0f && far > near)
            {
                coreEngine.hostSetSelectedCamera(tag.EntityId, fov, speed, near, far);
            }
        }

        private void CameraSpeedTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (!IsLoaded || !_shouldUpdateEntityCam) return;
            if (HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind == HierarchyItemKind.Camera &&
                ParseCoordinateOrBlank(CameraFovTextBox.Text, 0f, out float fov) &&
                 ParseCoordinateOrBlank(CameraSpeedTextBox.Text, 0f, out float speed) &&
                 ParseCoordinateOrBlank(CameraNearTextBox.Text, 0f, out float near) &&
                 ParseCoordinateOrBlank(CameraFarTextBox.Text, 0f, out float far) &&
                fov > 0f && fov < 180f && speed > 0f && near > 0f && far > near)
            {
                coreEngine.hostSetSelectedCamera(tag.EntityId, fov, speed, near, far);
            }
        }

        private void CameraNearTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (!IsLoaded || !_shouldUpdateEntityCam) return;
            if (HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind == HierarchyItemKind.Camera &&
                ParseCoordinateOrBlank(CameraFovTextBox.Text, 0f, out float fov) &&
                 ParseCoordinateOrBlank(CameraSpeedTextBox.Text, 0f, out float speed) &&
                 ParseCoordinateOrBlank(CameraNearTextBox.Text, 0f, out float near) &&
                 ParseCoordinateOrBlank(CameraFarTextBox.Text, 0f, out float far) &&
                fov > 0f && fov < 180f && speed > 0f && near > 0f && far > near)
            {
                coreEngine.hostSetSelectedCamera(tag.EntityId, fov, speed, near, far);
            }
        }

        private void CameraFarTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (!IsLoaded || !_shouldUpdateEntityCam) return;
            if (HierarchyTree.SelectedItem is TreeViewItem item && item.Tag is HierarchyTag tag && tag.Kind == HierarchyItemKind.Camera &&
                ParseCoordinateOrBlank(CameraFovTextBox.Text, 0f, out float fov) &&
                 ParseCoordinateOrBlank(CameraSpeedTextBox.Text, 0f, out float speed) &&
                 ParseCoordinateOrBlank(CameraNearTextBox.Text, 0f, out float near) &&
                 ParseCoordinateOrBlank(CameraFarTextBox.Text, 0f, out float far) &&
                fov > 0f && fov < 180f && speed > 0f && near > 0f && far > near)
            {
                coreEngine.hostSetSelectedCamera(tag.EntityId, fov, speed, near, far);
            }
        }

        private static bool IsAllowedViewportCameraText(string text) =>
            Regex.IsMatch(text, @"^[0-9]*(?:[.,][0-9]*)?(?:[eE][+-]?[0-9]*)?$");

        private void ViewportCameraTextBox_PreviewTextInput(object sender, TextCompositionEventArgs e)
        {
            if (sender is TextBox box)
                e.Handled = !IsAllowedViewportCameraText(CandidateText(box, e.Text));
        }

        private void ViewportCameraTextBox_Pasting(object sender, DataObjectPastingEventArgs e)
        {
            if (sender is not TextBox box ||
                !e.DataObject.GetDataPresent(DataFormats.UnicodeText) ||
                e.DataObject.GetData(DataFormats.UnicodeText) is not string pasted ||
                !IsAllowedViewportCameraText(CandidateText(box, pasted)))
            {
                e.CancelCommand();
            }
        }

        private void ViewportCameraTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if (!IsLoaded || _refreshingViewportCameraFields)
                return;


           if(ParseCoordinateOrBlank(EditorCameraFovTextBox.Text, 0f, out float fov) &&
                ParseCoordinateOrBlank(EditorCameraSpeedTextBox.Text, 0f, out float speed) &&
                ParseCoordinateOrBlank(EditorCameraNearTextBox.Text, 0f, out float near) &&
                ParseCoordinateOrBlank(EditorCameraFarTextBox.Text, 0f, out float far) &&
               fov > 0f && fov < 180f && speed > 0f && near > 0f && far > near)
            {
                coreEngine.hostSetViewportCamera(fov, speed, near, far);
            }
        }

        private void ViewportCameraTextBox_LostFocus(object sender, RoutedEventArgs e) =>
            refreshViewportCamera();

        private void ViewportCameraTextBox_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.Key != Key.Enter)
                return;

            refreshViewportCamera();
            e.Handled = true;
        }

        private void LightColorButton_Click(object sender, RoutedEventArgs e)
        {
            if (HierarchyTree.SelectedItem is not TreeViewItem item ||
                item.Tag is not HierarchyTag tag ||
                tag.Kind != HierarchyItemKind.Light) return;

            Color initialColor = (LightColorPreview.Background as SolidColorBrush)?.Color ?? Colors.White;
            var picker = new LightColorPickerWindow(initialColor) { Owner = this };
            if (ShowModal(() => picker.ShowDialog()) == true)
            {
                Color selected = picker.SelectedColor;
                if (coreEngine.hostSetLightColor(
                        tag.EntityId,
                        selected.R / 255f,
                        selected.G / 255f,
                        selected.B / 255f))
                {
                    LightColorPreview.Background = new SolidColorBrush(selected);
                }
            }
        }

        private void LightTypeComboBox_SelectionChanged(object sender, SelectionChangedEventArgs e)
        {
            if (!IsLoaded || LightTypeComboBox.SelectedIndex < 0 || HierarchyTree.SelectedItem is not TreeViewItem item || item.Tag is not HierarchyTag tag || tag.Kind != HierarchyItemKind.Light) return;

            coreEngine.hostSetLightType(tag.EntityId, LightTypeComboBox.SelectedIndex);
        }


        private void LightIntensitySlider_ValueChanged(object sender, RoutedPropertyChangedEventArgs<double> e)
        {
            if (!IsLoaded || HierarchyTree.SelectedItem is not TreeViewItem item || item.Tag is not HierarchyTag tag || tag.Kind != HierarchyItemKind.Light) return;

            coreEngine.hostSetLightIntensity(tag.EntityId, (float)LightIntensitySlider.Value);
        }

        private void LightDirectionXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float x, y, z;
                if (!ParseCoordinateOrBlank(LightDirectionXTextBox.Text, 0f, out x) ||
                 !ParseCoordinateOrBlank(LightDirectionYTextBox.Text, 0f, out y) ||
                 !ParseCoordinateOrBlank(LightDirectionZTextBox.Text, 0f, out z)) return;
                coreEngine.hostSetLightDir(tag.EntityId, x,y,z);
            }
        }

        private void LightDirectionYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float x, y, z;
                if (!ParseCoordinateOrBlank(LightDirectionXTextBox.Text, 0f, out x) ||
                 !ParseCoordinateOrBlank(LightDirectionYTextBox.Text, 0f, out y) ||
                 !ParseCoordinateOrBlank(LightDirectionZTextBox.Text, 0f, out z)) return;
                coreEngine.hostSetLightDir(tag.EntityId, x, y, z);
            }
        }

        private void LightDirectionZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float x, y, z;
                if (!ParseCoordinateOrBlank(LightDirectionXTextBox.Text, 0f, out x) ||
                 !ParseCoordinateOrBlank(LightDirectionYTextBox.Text, 0f, out y) ||
                 !ParseCoordinateOrBlank(LightDirectionZTextBox.Text, 0f, out z)) return;
                coreEngine.hostSetLightDir(tag.EntityId, x, y, z);
            }
        }

        private void LightPositionXTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float x, y, z;
                if (!ParseCoordinateOrBlank(LightPositionXTextBox.Text, 0f, out x) ||
                 !ParseCoordinateOrBlank(LightPositionYTextBox.Text, 0f, out y) ||
                 !ParseCoordinateOrBlank(LightPositionZTextBox.Text, 0f, out z)) return;
                coreEngine.hostSetLightPos(tag.EntityId, x, y, z);
            }
        }

        private void LightPositionYTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float x, y, z;
                if (!ParseCoordinateOrBlank(LightPositionXTextBox.Text, 0f, out x) ||
                 !ParseCoordinateOrBlank(LightPositionYTextBox.Text, 0f, out y) ||
                 !ParseCoordinateOrBlank(LightPositionZTextBox.Text, 0f, out z)) return;
                coreEngine.hostSetLightPos(tag.EntityId, x, y, z);
            }
        }

        private void LightPositionZTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float x, y, z;
                if (!ParseCoordinateOrBlank(LightPositionXTextBox.Text, 0f, out x) ||
                 !ParseCoordinateOrBlank(LightPositionYTextBox.Text, 0f, out y) ||
                 !ParseCoordinateOrBlank(LightPositionZTextBox.Text, 0f, out z)) return;
                coreEngine.hostSetLightPos(tag.EntityId, x, y, z);
            }
        }

        private void LightAmbientStrengthTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float ambient;
                if (!ParseCoordinateOrBlank(LightAmbientStrengthTextBox.Text,0f,out ambient)) return;
                coreEngine.hostSetLightAmbientStr(tag.EntityId, ambient);
            }
        }

        private void LightDiffuseStrengthTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float diffuse;
                if (!ParseCoordinateOrBlank(LightDiffuseStrengthTextBox.Text, 0f, out diffuse)) return;
                coreEngine.hostSetLightDiffuseStr(tag.EntityId, diffuse);
            }
        }

        private void LightSpecularStrengthTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float specular;
                if (!ParseCoordinateOrBlank(LightSpecularStrengthTextBox.Text, 0f, out specular)) return;
                coreEngine.hostSetLightSpecularStr(tag.EntityId, specular);
            }
        }

        private void LightCutoffTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float cutOff;
                float outerCutOff;
                if (!ParseCoordinateOrBlank(LightCutoffTextBox.Text, 0f, out cutOff) || !ParseCoordinateOrBlank(LightOuterCutoffTextBox.Text, 0f, out outerCutOff)) return;
                coreEngine.hostSetLightCutOff(tag.EntityId, cutOff,outerCutOff);
            }
        }

        private void LightOuterCutoffTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float cutOff;
                float outerCutOff;
                if (!ParseCoordinateOrBlank(LightCutoffTextBox.Text, 0f, out cutOff) || !ParseCoordinateOrBlank(LightOuterCutoffTextBox.Text, 0f, out outerCutOff)) return;
                coreEngine.hostSetLightCutOff(tag.EntityId, cutOff,outerCutOff);
            }
        }

        private void LightConstantTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float constant;
                if (!ParseCoordinateOrBlank(LightConstantTextBox.Text, 0f, out constant)) return;
                coreEngine.hostSetLightConstant(tag.EntityId, constant);
            }
        }

        private void LightLinearTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float linear;
                if (!ParseCoordinateOrBlank(LightLinearTextBox.Text, 0f, out linear)) return;
                coreEngine.hostSetLightLinear(tag.EntityId, linear);
            }
        }

        private void LightQuadraticTextBox_TextChanged(object sender, TextChangedEventArgs e)
        {
            if ((HierarchyTree.SelectedItem is TreeViewItem selectedItem && selectedItem.Tag is HierarchyTag tag) && _shouldUpdateLight)
            {
                float quadratic;
                if (!ParseCoordinateOrBlank(LightQuadraticTextBox.Text, 0f, out quadratic)) return;
                coreEngine.hostSetLightQuadratic(tag.EntityId, quadratic);
            }
        }

        private void refreshViewportCamera()
        {
            float fov, speed, far, near;

            if (coreEngine.hostGetViewportCamera(out fov, out speed, out near, out far) != 1) return;
            _refreshingViewportCameraFields = true;
            try
            {
                EditorCameraSpeedTextBox.Text = speed.ToString("G7", CultureInfo.InvariantCulture);
                EditorCameraFovTextBox.Text = fov.ToString("G7", CultureInfo.InvariantCulture);
                EditorCameraNearTextBox.Text = near.ToString("G7", CultureInfo.InvariantCulture);
                EditorCameraFarTextBox.Text = far.ToString("G7", CultureInfo.InvariantCulture);
            }
            finally
            {
                _refreshingViewportCameraFields = false;
            }
        }
    }
}
