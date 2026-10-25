#include "MainWindow.hpp"
#include "LayerCanvas.hpp"
#include "Viewport.hpp"
#include "io/ProjectIO.hpp"
#include "core/Modeling.hpp"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSettings>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QSignalBlocker>
#include <QShortcut>
#include <QColor>
#include <QFrame>
#include <QHBoxLayout>
#include <QKeySequence>

#include <algorithm>
#include <cmath>
#include <limits>

namespace studio {

static QString q(const std::string& value) {return QString::fromStdString(value);}
static QDoubleSpinBox* coordinateSpin(QWidget* parent) {
    auto* box=new QDoubleSpinBox(parent);
    box->setRange(-100000.0,100000.0);box->setDecimals(4);
    box->setSingleStep(1.0/16.0);
    box->setKeyboardTracking(false);
    return box;
}
MainWindow::MainWindow(QWidget* parent):QMainWindow(parent),project_(makeDemo()) {
    buildUI();
    createMenus();
    // The bundled demo is a formed model: show its geometry when opening the
    // application. New empty documents start in Construction mode instead.
    mode_->setCurrentIndex(1);
    for(int i=0;i<int(project_.parts.size());i++) {
        if(project_.parts[std::size_t(i)].name=="Drum") { selected_=i;break; }
    }
    QSettings pref;
    const QByteArray geometry=pref.value("window/geometry").toByteArray();
    if(!geometry.isEmpty())restoreGeometry(geometry);
    else resize(1420,900);
    const QByteArray state=pref.value("window/state").toByteArray();
    if(!state.isEmpty())restoreState(state);
    refreshAll();viewport_->resetCamera();
}
void MainWindow::buildUI(){
    setObjectName("MainWindow");
    setDockOptions(QMainWindow::AllowNestedDocks|QMainWindow::AllowTabbedDocks|QMainWindow::AnimatedDocks);
    viewport_=new Viewport(this);
    layers_=new LayerCanvas(this);
    auto* splitter=new QSplitter(Qt::Vertical,this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(viewport_);
    layerContainer_=new QWidget(this);
    auto* layerLayout=new QVBoxLayout(layerContainer_);
    layerLayout->setContentsMargins(0,0,0,0);
    auto* layerToolbar=new QWidget(this);
    auto* layerRow=new QHBoxLayout(layerToolbar);layerRow->setContentsMargins(12,4,12,4);
    layerRow->addWidget(new QLabel("CONSTRUCTION LAYER",this));
    layerSpinner_=new QSpinBox(this);layerSpinner_->setRange(0,project_.dimensions[1]-1);
    layerSpinner_->setMaximumWidth(85);layerRow->addWidget(layerSpinner_);
    layerRow->addStretch(1);
    layerRow->addWidget(new QLabel("BRUSH",this));
    role_=new QComboBox(this);role_->setMinimumWidth(160);layerRow->addWidget(role_);
    layerLayout->addWidget(layerToolbar);
    layerLayout->addWidget(layers_,1);
    splitter->addWidget(layerContainer_);splitter->setStretchFactor(0,3);splitter->setStretchFactor(1,1);
    setCentralWidget(splitter);

    auto* objects=new QDockWidget("MODEL OUTLINER  /  GROUPS",this);
    objects->setObjectName("PartsDock");
    tree_=new QTreeWidget(this);
    tree_->setHeaderHidden(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    objects->setWidget(tree_);addDockWidget(Qt::LeftDockWidgetArea,objects);
    objects->setMinimumWidth(220);
    auto* inspector=new QDockWidget("PROPERTIES",this);
    inspector->setObjectName("PropertiesDock");
    QWidget* container=new QWidget(this);
    auto* scroller=new QScrollArea(this);
    scroller->setWidgetResizable(true);scroller->setFrameShape(QFrame::NoFrame);
    scroller->setWidget(container);
    inspector->setWidget(scroller);addDockWidget(Qt::RightDockWidgetArea,inspector);
    inspector->setMinimumWidth(250);
    createInspector();
    statusBar()->showMessage("Native GPU viewport  |  Mouse orbit: left-drag empty space  |  Pan: middle-drag  |  Zoom: wheel");

    connect(role_,&QComboBox::currentTextChanged,layers_,&LayerCanvas::setRole);
    connect(layerSpinner_,QOverload<int>::of(&QSpinBox::valueChanged),this,[this](int n){
        layer_=n;layers_->setLayer(n);refreshScene();
    });
    connect(layers_,&LayerCanvas::paintRequested,this,&MainWindow::editCell);
    connect(tree_,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem* item,QTreeWidgetItem*){
        if(updating_)return;
        const int index=item?item->data(0,Qt::UserRole).toInt():-1;
        setSelected(index);
    });
    connect(viewport_,&Viewport::partSelected,this,&MainWindow::setSelected);
    connect(viewport_,&Viewport::beginEdit,this,[this](){dragChanged_=false;});
    connect(viewport_,&Viewport::deltaEdit,this,&MainWindow::applyTransformDelta);
    connect(viewport_,&Viewport::endEdit,this,[this](){dragChanged_=false;});
    connect(viewport_,&Viewport::boxDrawn,this,&MainWindow::addDrawnBox);
    connect(viewport_,&Viewport::axisPicked,this,[this](char axis){
        if(!axis_)return;
        const auto index=axis_->findData(int(axis));
        if(index>=0)axis_->setCurrentIndex(index);
    });
    connect(viewport_,&Viewport::cameraPresetChanged,this,[this](int preset){
        if(viewPreset_)viewPreset_->setCurrentIndex(preset);
    });
}
void MainWindow::createInspector(){
    QWidget* contents=static_cast<QScrollArea*>(findChild<QDockWidget*>("PropertiesDock")->widget())->widget();
    auto* layout=new QVBoxLayout(contents);layout->setContentsMargins(12,12,12,12);layout->setSpacing(14);
    auto* appearance=new QGroupBox("PART",contents);
    auto* form=new QFormLayout(appearance);
    partName_=new QLineEdit(contents);groupName_=new QLineEdit(contents);
    texture_=new QLineEdit(contents);
    texture_->setPlaceholderText("magneticraft2:block/texture_name");
    material_=new QComboBox(contents);
    material_->addItems({"wood","copper","stone","iron","dark","glass","water"});
    visible_=new QCheckBox("Show in model",contents);
    form->addRow("Name",partName_);form->addRow("Group",groupName_);
    form->addRow("Material",material_);form->addRow("Texture ID",texture_);
    form->addRow("Visibility",visible_);
    layout->addWidget(appearance);
    auto* transform=new QGroupBox("TRANSFORM  /  BLOCK UNITS",contents);
    auto* tf=new QFormLayout(transform);
    const char* axes[]={"X","Y","Z"};
    for(int i=0;i<3;i++){
        from_[i]=coordinateSpin(contents);to_[i]=coordinateSpin(contents);
        tf->addRow(QString("Min ")+axes[i],from_[i]);tf->addRow(QString("Max ")+axes[i],to_[i]);
    }
    for(int i=0;i<3;i++){
        angle_[i]=coordinateSpin(contents);angle_[i]->setRange(-3600,3600);
        angle_[i]->setSingleStep(5);tf->addRow(QString("Angle ")+axes[i],angle_[i]);
    }
    for(int i=0;i<3;i++){
        pivot_[i]=coordinateSpin(contents);tf->addRow(QString("Pivot ")+axes[i],pivot_[i]);
    }
    layout->addWidget(transform);
    auto* help=new QLabel("MODEL TOOLS\nV Select  /  B Draw cube\nW Move  /  E Rotate  /  R Resize\nX / Y / Z: active handle axis\nLeft-drag workplane to draw new geometry.\n1 unit = 1 Minecraft block (16 pixels).\nTexture ID is saved; textured preview is upcoming.",contents);
    help->setWordWrap(true);help->setObjectName("InspectorHelp");layout->addWidget(help);layout->addStretch();
    const auto connectEdit=[this](){
        // Spinboxes use keyboardTracking=false, so user typing a number commits
        // only once rather than generating hundreds of transient undo states.
        applyInspector();
    };
    connect(partName_,&QLineEdit::editingFinished,this,connectEdit);
    connect(groupName_,&QLineEdit::editingFinished,this,connectEdit);
    connect(texture_,&QLineEdit::editingFinished,this,connectEdit);
    connect(material_,&QComboBox::currentTextChanged,this,connectEdit);
    connect(visible_,&QCheckBox::toggled,this,connectEdit);
    for(int i=0;i<3;i++){
        connect(from_[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,connectEdit);
        connect(to_[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,connectEdit);
        connect(angle_[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,connectEdit);
        connect(pivot_[i],QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,connectEdit);
    }
}
void MainWindow::createMenus(){
    auto* file=menuBar()->addMenu("&File");
    auto* newAction=file->addAction("New multiblock",QKeySequence::New,this,&MainWindow::createNew);
    Q_UNUSED(newAction);
    file->addAction("Open project / legacy JSON...",QKeySequence::Open,this,&MainWindow::openDialog);
    file->addAction("Save project",QKeySequence::Save,this,&MainWindow::save);
    file->addAction("Save as...",QKeySequence::SaveAs,this,&MainWindow::saveAs);
    file->addSeparator();
    file->addAction("Export Forge construction JSONs...",this,&MainWindow::exportDialog);
    file->addSeparator();file->addAction("Quit",QKeySequence::Quit,this,&QWidget::close);

    auto* edit=menuBar()->addMenu("&Edit");
    undoAction_=edit->addAction("Undo",QKeySequence::Undo,this,&MainWindow::undo);
    redoAction_=edit->addAction("Redo",QKeySequence::Redo,this,&MainWindow::redo);
    edit->addSeparator();
    edit->addAction("Add cuboid",QKeySequence("Ctrl+Shift+N"),this,&MainWindow::addPart);
    edit->addAction("Duplicate selected",QKeySequence("Ctrl+D"),this,&MainWindow::duplicatePart);
    edit->addAction("Delete selected",QKeySequence::Delete,this,&MainWindow::removePart);
    edit->addAction("Resize structure...",this,&MainWindow::resizeDocument);

    auto* view=menuBar()->addMenu("&View");
    view->addAction("Fit entire structure",QKeySequence("F"),viewport_,&Viewport::resetCamera);
    auto* help=menuBar()->addMenu("&Help");
    help->addAction("About Magneticraft2 Studio",this,[this](){
        QMessageBox::about(this,"Magneticraft2 Studio", "<b>Magneticraft2 Studio — Native Linux Model Workspace v0.3</b><br>"
            "C++17 + Qt 6 + OpenGL 3.3 Core<br><br>"
            "Dedicated 3D model workspace with direct cube drawing, grid snapping, views and Studio v1 save/load.<br>"
            "<b>Not yet:</b> real textured UV preview, animation authoring, or mod-side tiled rendering.");
    });

    auto* toolbar=addToolBar("Modeling");toolbar->setObjectName("ModelToolbar");
    toolbar->setMovable(false);
    toolbar->addAction("New",this,&MainWindow::createNew);
    toolbar->addAction("Open",this,&MainWindow::openDialog);
    toolbar->addAction("Save",this,&MainWindow::save);
    toolbar->addSeparator();
    toolbar->addAction("+ Box",this,&MainWindow::addPart);
    toolbar->addAction("Duplicate",this,&MainWindow::duplicatePart);
    toolbar->addAction("Delete",this,&MainWindow::removePart);
    toolbar->addSeparator();
    toolbar->addWidget(new QLabel("  WORKSPACE  ",this));
    mode_=new QComboBox(this);mode_->addItems({"Construction", "Model"});
    toolbar->addWidget(mode_);
    ghost_=new QCheckBox("Ghost structure",this);toolbar->addWidget(ghost_);
    toolbar->addAction("Fit",viewport_,&Viewport::resetCamera);
    addToolBarBreak(Qt::TopToolBarArea);
    modelToolbar_=addToolBar("Model authoring tools");
    modelToolbar_->setObjectName("ModelAuthoringToolbar");
    modelToolbar_->setMovable(false);
    modelToolbar_->addWidget(new QLabel("  MODEL TOOL  ",this));
    tool_=new QComboBox(this);
    tool_->addItem("V · Select",int('v'));
    tool_->addItem("B · Draw cube",int('b'));
    tool_->addItem("W · Move",int('w'));
    tool_->addItem("E · Rotate",int('e'));
    tool_->addItem("R · Resize",int('r'));
    modelToolbar_->addWidget(tool_);
    modelToolbar_->addWidget(new QLabel("  AXIS  ",this));
    axis_=new QComboBox(this);
    axis_->addItem("X",int('x'));axis_->addItem("Y",int('y'));axis_->addItem("Z",int('z'));
    modelToolbar_->addWidget(axis_);
    modelToolbar_->addSeparator();
    modelToolbar_->addWidget(new QLabel("  SNAP  ",this));
    snap_=new QComboBox(this);
    snap_->addItem("1/16",1.0/16.0);
    snap_->addItem("1/8",1.0/8.0);
    snap_->addItem("1/4",1.0/4.0);
    snap_->addItem("1/2",1.0/2.0);
    snap_->addItem("1",1.0);
    modelToolbar_->addWidget(snap_);
    modelToolbar_->addWidget(new QLabel("  PLANE Y  ",this));
    workPlane_=coordinateSpin(this);workPlane_->setValue(0);workPlane_->setMaximumWidth(94);
    modelToolbar_->addWidget(workPlane_);
    modelToolbar_->addWidget(new QLabel("  HEIGHT  ",this));
    drawHeight_=coordinateSpin(this);drawHeight_->setRange(1.0/16.0,128);drawHeight_->setValue(1.0);
    drawHeight_->setMaximumWidth(94);modelToolbar_->addWidget(drawHeight_);
    modelToolbar_->addSeparator();
    modelToolbar_->addWidget(new QLabel("  VIEW  ",this));
    viewPreset_=new QComboBox(this);
    viewPreset_->addItems({"Perspective","Top","Front","Right"});
    modelToolbar_->addWidget(viewPreset_);
    connect(mode_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){switchWorkspace();});
    connect(ghost_,&QCheckBox::toggled,this,[this](bool){refreshScene();});
    connect(tool_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){
        const char t=char(tool_->currentData().toInt());
        viewport_->setTool(t);
        if(t=='b' && viewPreset_->currentIndex()>=2) viewPreset_->setCurrentIndex(1);
        drawHeight_->setEnabled(t=='b');
    });
    connect(axis_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){viewport_->setAxis(char(axis_->currentData().toInt()));});
    connect(snap_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){viewport_->setGridStep(snap_->currentData().toDouble());});
    connect(workPlane_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double y){viewport_->setWorkPlane(y);});
    connect(drawHeight_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double h){viewport_->setDrawHeight(h);});
    connect(viewPreset_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int view){viewport_->setCameraPreset(view);});
    viewport_->setTool('v');viewport_->setAxis('x');
    // Shortcuts apply to the focused viewport; editing text fields never steals them.
    for(const auto key : {Qt::Key_V,Qt::Key_B,Qt::Key_W,Qt::Key_E,Qt::Key_R,Qt::Key_X,Qt::Key_Y,Qt::Key_Z}) {
        auto* shortcut=new QShortcut(QKeySequence(key),viewport_);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut,&QShortcut::activated,this,[this,key](){
            const char c=char(key);
            if(c=='V'||c=='B'||c=='W'||c=='E'||c=='R'){
                const char lower=char(c+('a'-'A'));
                int index=tool_->findData(int(lower));
                if(index>=0)tool_->setCurrentIndex(index);
            }else axis_->setCurrentIndex(c=='X'?0:c=='Y'?1:2);
        });
    }
    undoAction_->setEnabled(false);redoAction_->setEnabled(false);
    switchWorkspace();
}
void MainWindow::refreshRolePalette(){
    const auto previously=role_->currentText();
    QSignalBlocker blocker(role_);
    role_->clear();
    role_->addItem("Air");
    for(const auto& [key,value]:project_.palette){Q_UNUSED(value);if(key!="Air")role_->addItem(q(key));}
    int which=role_->findText(previously);
    if(which<0)which=std::max(0,role_->findText("Frame"));
    role_->setCurrentIndex(which);
    layers_->setRole(role_->currentText());
}
void MainWindow::refreshPartTree(){
    QSignalBlocker blocker(tree_);
    tree_->clear();
    std::map<std::string,QTreeWidgetItem*> groups;
    for(std::size_t i=0;i<project_.parts.size();i++){
        const auto& part=project_.parts[i];
        const auto group=part.group.empty()?std::string("Ungrouped"):part.group;
        if(!groups.count(group)){
            auto* header=new QTreeWidgetItem(tree_);
            header->setText(0,q(group));
            header->setData(0,Qt::UserRole,-1);
            header->setExpanded(true);
            header->setForeground(0,QColor("#e5b483"));
            groups[group]=header;
        }
        auto* item=new QTreeWidgetItem(groups[group]);
        item->setText(0,q(part.name));
        item->setToolTip(0,QString("%1 · %2").arg(q(part.material),q(part.texture)));
        item->setData(0,Qt::UserRole,int(i));
        if(!part.visible)item->setForeground(0,QColor("#667681"));
        if(int(i)==selected_)tree_->setCurrentItem(item);
    }
}
void MainWindow::refreshInspector(){
    updating_=true;
    const bool enabled=selected_>=0&&selected_<int(project_.parts.size());
    partName_->setEnabled(enabled);groupName_->setEnabled(enabled);material_->setEnabled(enabled);
    texture_->setEnabled(enabled);
    visible_->setEnabled(enabled);
    for(int i=0;i<3;i++){
        from_[i]->setEnabled(enabled);to_[i]->setEnabled(enabled);
        angle_[i]->setEnabled(enabled);pivot_[i]->setEnabled(enabled);
    }
    if(enabled){
        const auto& part=project_.parts[std::size_t(selected_)];
        partName_->setText(q(part.name));groupName_->setText(q(part.group));texture_->setText(q(part.texture));
        int material=material_->findText(q(part.material));
        if(material<0){material_->addItem(q(part.material));material=material_->count()-1;}
        material_->setCurrentIndex(material);visible_->setChecked(part.visible);
        const double f[]={part.bounds.from.x,part.bounds.from.y,part.bounds.from.z};
        const double t[]={part.bounds.to.x,part.bounds.to.y,part.bounds.to.z};
        const double a[]={part.rotationDegrees.x,part.rotationDegrees.y,part.rotationDegrees.z};
        const double pivot[]={part.pivot.x,part.pivot.y,part.pivot.z};
        for(int i=0;i<3;i++){from_[i]->setValue(f[i]);to_[i]->setValue(t[i]);angle_[i]->setValue(a[i]);pivot_[i]->setValue(pivot[i]);}
    }else {
        partName_->clear();groupName_->clear();texture_->clear();visible_->setChecked(false);
    }
    updating_=false;
}
void MainWindow::refreshScene(){
    BuildOptions view;
    const bool formed=mode_ && mode_->currentIndex()==1;
    view.showConstruction=!formed || (ghost_ && ghost_->isChecked());
    view.showFormed=formed;
    view.ghostStructure=formed;
    view.layer=formed?-1:layer_;
    view.gridY=formed?workPlane_->value():0.0;
    view.selectedPart=selected_;
    viewport_->setScene(project_,view);
    layers_->setProject(&project_);layers_->setLayer(layer_);
    statusBar()->showMessage(QString("%1 x %2 x %3 blocks  |  %4 placed cells  |  %5 formed parts  |  Mesh built on worker thread")
        .arg(project_.dimensions[0]).arg(project_.dimensions[1]).arg(project_.dimensions[2])
        .arg(project_.cells.size()).arg(project_.parts.size()));
}
void MainWindow::updateTitle(){
    setWindowTitle(QString("%1%2 — Magneticraft2 Studio (Native Linux)")
        .arg(q(project_.name),dirty_?" *":""));
    undoAction_->setEnabled(!undo_.empty());redoAction_->setEnabled(!redo_.empty());
}
void MainWindow::refreshAll(){
    layerSpinner_->setRange(0,project_.dimensions[1]-1);
    layer_=std::min(layer_,project_.dimensions[1]-1);
    layerSpinner_->setValue(layer_);
    refreshRolePalette();refreshPartTree();refreshInspector();refreshScene();updateTitle();
}
void MainWindow::setSelected(int index){
    if(index>=int(project_.parts.size())||index<0)index=-1;
    if(selected_==index)return;
    selected_=index;
    if(index>=0 && mode_)mode_->setCurrentIndex(1);
    refreshPartTree();refreshInspector();refreshScene();
}
void MainWindow::snapshot(){
    undo_.push_back(project_);
    if(undo_.size()>80)undo_.erase(undo_.begin());
    redo_.clear();
}
void MainWindow::commitChanged(){dirty_=true;refreshAll();}
void MainWindow::undo(){
    if(undo_.empty())return;
    redo_.push_back(project_);project_=std::move(undo_.back());undo_.pop_back();
    if(selected_>=int(project_.parts.size()))selected_=-1;
    commitChanged();
}
void MainWindow::redo(){
    if(redo_.empty())return;
    undo_.push_back(project_);project_=std::move(redo_.back());redo_.pop_back();
    if(selected_>=int(project_.parts.size()))selected_=-1;
    commitChanged();
}
void MainWindow::addPart(){
    snapshot();
    const double x=project_.dimensions[0]*0.5;
    const double z=project_.dimensions[2]*0.5;
    const double y=workPlane_->value();
    auto part=makeBox(project_,{{x-0.5,y,z-0.5},{x+0.5,y+drawHeight_->value(),z+0.5}});
    part.group="Model";part.name="Cube "+std::to_string(project_.parts.size()+1);
    project_.parts.push_back(part);selected_=int(project_.parts.size()-1);
    mode_->setCurrentIndex(1);commitChanged();
}
void MainWindow::addDrawnBox(double x0,double z0,double x1,double z1){
    if(mode_->currentIndex()!=1)return;
    auto bounds=drawnCuboid({x0,0,z0},{x1,0,z1},workPlane_->value(),
        drawHeight_->value(),snap_->currentData().toDouble());
    snapshot();
    auto part=makeBox(project_,bounds);
    part.name="Cube "+std::to_string(project_.parts.size()+1);
    part.group="Model";
    // Inherit material from the last selected cube when available.
    if(selected_>=0 && selected_<int(project_.parts.size()))
        part.material=project_.parts[std::size_t(selected_)].material;
    project_.parts.push_back(part);
    selected_=int(project_.parts.size())-1;
    commitChanged();
}
void MainWindow::switchWorkspace(){
    if(!mode_ || !viewport_ || !layerContainer_)return;
    const bool isModel=mode_->currentIndex()==1;
    layerContainer_->setVisible(!isModel);
    tool_->setEnabled(isModel);axis_->setEnabled(isModel);
    snap_->setEnabled(isModel);workPlane_->setEnabled(isModel);
    drawHeight_->setEnabled(isModel && tool_->currentData().toInt()==int('b'));
    ghost_->setEnabled(isModel);
    modelToolbar_->setVisible(isModel);
    refreshScene();
}
void MainWindow::duplicatePart(){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    snapshot();Part part=project_.parts[std::size_t(selected_)];
    part.id=nextPartId(project_);part.name+=" Copy";
    const Vec3 offset{0.25,0,0.25};
    part.bounds.from=part.bounds.from+offset;part.bounds.to=part.bounds.to+offset;part.pivot=part.pivot+offset;
    project_.parts.push_back(part);selected_=int(project_.parts.size())-1;
    mode_->setCurrentIndex(1);commitChanged();
}
void MainWindow::removePart(){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    snapshot();project_.parts.erase(project_.parts.begin()+selected_);
    selected_=-1;commitChanged();
}
void MainWindow::applyInspector(){
    if(updating_||selected_<0||selected_>=int(project_.parts.size()))return;
    auto part=project_.parts[std::size_t(selected_)];
    part.name=partName_->text().toStdString();part.group=groupName_->text().toStdString();
    part.texture=texture_->text().trimmed().toStdString();
    part.material=material_->currentText().toStdString();part.visible=visible_->isChecked();
    part.bounds.from={from_[0]->value(),from_[1]->value(),from_[2]->value()};
    part.bounds.to={to_[0]->value(),to_[1]->value(),to_[2]->value()};
    part.rotationDegrees={angle_[0]->value(),angle_[1]->value(),angle_[2]->value()};
    part.pivot={pivot_[0]->value(),pivot_[1]->value(),pivot_[2]->value()};
    if(!validBounds(part.bounds)){
        statusBar()->showMessage("Invalid cuboid dimensions: each Min must be smaller than Max",5000);
        return;
    }
    const auto& old=project_.parts[std::size_t(selected_)];
    if(part.name==old.name && part.group==old.group && part.material==old.material &&
       part.texture==old.texture && part.visible==old.visible &&
       part.bounds.from.x==old.bounds.from.x && part.bounds.from.y==old.bounds.from.y && part.bounds.from.z==old.bounds.from.z &&
       part.bounds.to.x==old.bounds.to.x && part.bounds.to.y==old.bounds.to.y && part.bounds.to.z==old.bounds.to.z &&
       part.rotationDegrees.x==old.rotationDegrees.x&&part.rotationDegrees.y==old.rotationDegrees.y&&part.rotationDegrees.z==old.rotationDegrees.z&&
       part.pivot.x==old.pivot.x&&part.pivot.y==old.pivot.y&&part.pivot.z==old.pivot.z)return;
    snapshot();project_.parts[std::size_t(selected_)]=std::move(part);
    commitChanged();
}
void MainWindow::applyTransformDelta(double amount,char axis,char tool){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    if(!dragChanged_){snapshot();dragChanged_=true;}
    auto& part=project_.parts[std::size_t(selected_)];
    double* lo=axis=='x'?&part.bounds.from.x:axis=='y'?&part.bounds.from.y:&part.bounds.from.z;
    double* hi=axis=='x'?&part.bounds.to.x:axis=='y'?&part.bounds.to.y:&part.bounds.to.z;
    double* pivot=axis=='x'?&part.pivot.x:axis=='y'?&part.pivot.y:&part.pivot.z;
    double* angle=axis=='x'?&part.rotationDegrees.x:axis=='y'?&part.rotationDegrees.y:&part.rotationDegrees.z;
    if(tool=='w'){*lo+=amount;*hi+=amount;*pivot+=amount;}
    else if(tool=='e')*angle+=amount;
    else if(tool=='r')*hi=std::max(*lo+1.0/16.0,*hi+amount);
    dirty_=true;refreshScene();refreshInspector();updateTitle();
}
void MainWindow::editCell(int x,int y,int z,const QString& role){
    auto cell=Cell{x,y,z};
    const auto before=project_.cells.find(cell);
    const std::string value=role.toStdString();
    if(before!=project_.cells.end()&&before->second==value)return;
    if(before==project_.cells.end()&&value=="Air")return;
    snapshot();
    if(value=="Controller"){
        for(auto it=project_.cells.begin();it!=project_.cells.end();){
            if(it->second=="Controller")it=project_.cells.erase(it);else ++it;
        }
    }
    setCell(project_,cell,value);
    dirty_=true;refreshScene();updateTitle();
}

static bool sizeDialog(QWidget* parent,const QString& title,QString& name,Dimensions& size,bool rename){
    QDialog dialog(parent);dialog.setWindowTitle(title);
    auto* outer=new QVBoxLayout(&dialog);
    auto* form=new QFormLayout;
    QLineEdit* nameField=nullptr;
    if(rename){nameField=new QLineEdit(name,&dialog);form->addRow("Name",nameField);}
    QSpinBox* axes[3];
    const char* labels[]={"Width (X)","Height (Y)","Depth (Z)"};
    for(int i=0;i<3;i++){axes[i]=new QSpinBox(&dialog);axes[i]->setRange(1,128);axes[i]->setValue(size[i]);form->addRow(labels[i],axes[i]);}
    outer->addLayout(form);
    auto* buttons=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel,&dialog);
    outer->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);
    if(dialog.exec()!=QDialog::Accepted)return false;
    if(nameField)name=nameField->text().trimmed();
    if(name.isEmpty())name="New Machine";
    for(int i=0;i<3;i++)size[i]=axes[i]->value();
    return true;
}
void MainWindow::createNew(){
    if(!maybeSave())return;
    QString name="New Multiblock";Dimensions dims{7,5,9};
    if(!sizeDialog(this,"New Multiblock",name,dims,true))return;
    project_=makeProject(name.toStdString(),dims);
    mode_->setCurrentIndex(0);
    dirty_=false;filename_.clear();loadedJson_={};selected_=-1;undo_.clear();redo_.clear();layer_=0;
    refreshAll();viewport_->resetCamera();
}
void MainWindow::resizeDocument(){
    QString name=q(project_.name);Dimensions dims=project_.dimensions;
    if(!sizeDialog(this,"Resize Multiblock",name,dims,false))return;
    snapshot();studio::resize(project_,dims);selected_=-1;commitChanged();viewport_->resetCamera();
}
void MainWindow::openDialog(){
    if(!maybeSave())return;
    QString path=QFileDialog::getOpenFileName(this,"Open Studio / Magneticraft2 JSON",{},
        "Magneticraft2 JSON (*.json *.mcraftstudio);;All files (*)");
    if(!path.isEmpty())openPath(path);
}
void MainWindow::openPath(const QString& path){
    LoadResult result;QString error;
    if(!ProjectIO::read(path,result,error)){
        QMessageBox::warning(this,"Cannot open project",error);return;
    }
    project_=std::move(result.project);loadedJson_=std::move(result.original);
    mode_->setCurrentIndex(project_.parts.empty()?0:1);
    selected_=-1;layer_=0;filename_=result.fromLegacy ? QString() : path;dirty_=false;undo_.clear();redo_.clear();
    refreshAll();viewport_->resetCamera();
}
void MainWindow::save(){
    if(filename_.isEmpty()){saveAs();return;}
    saveTo(filename_);
}
void MainWindow::saveAs(){
    QString path=QFileDialog::getSaveFileName(this,"Save Studio Project",filename_.isEmpty()?q(project_.id)+".mcraftstudio.json":filename_,
        "Studio project (*.mcraftstudio.json);;JSON (*.json)");
    if(path.isEmpty())return;
    if(!path.endsWith(".json",Qt::CaseInsensitive))path+=".mcraftstudio.json";
    saveTo(path);
}
bool MainWindow::saveTo(const QString& path){
    QString error;
    if(!ProjectIO::write(path,project_,loadedJson_,error)){
        QMessageBox::warning(this,"Could not save project",error);return false;
    }
    loadedJson_=ProjectIO::encode(project_,loadedJson_);
    filename_=path;dirty_=false;updateTitle();
    statusBar()->showMessage("Saved "+path,4000);return true;
}
bool MainWindow::maybeSave(){
    if(!dirty_)return true;
    const auto answer=QMessageBox::question(this,"Unsaved changes",
        "Save changes to this multiblock before continuing?",
        QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
    if(answer==QMessageBox::Cancel)return false;
    if(answer==QMessageBox::Discard)return true;
    save();return !dirty_;
}
void MainWindow::exportDialog(){
    QString directory=QFileDialog::getExistingDirectory(this,"Choose Forge export directory");
    if(directory.isEmpty())return;
    QString error;
    if(!ProjectIO::exportForgeDirectory(directory,project_,loadedJson_,error)){
        QMessageBox::warning(this,"Forge export failed",error);return;
    }
    QMessageBox::information(this,"Construction exported",
        "Exported four rotated construction JSONs and a validation manifest.\n\n"
        "The formed model renderer is not integrated with Forge yet. "
        "Do not treat this export as a drop-in replacement for the current mod renderer.");
}
void MainWindow::closeEvent(QCloseEvent* event){
    if(!maybeSave()){event->ignore();return;}
    QSettings pref;
    pref.setValue("window/geometry",saveGeometry());pref.setValue("window/state",saveState());
    event->accept();
}

} // namespace studio
