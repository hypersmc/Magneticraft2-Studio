#include "MainWindow.hpp"
#include "LayerCanvas.hpp"
#include "UvEditor.hpp"
#include "UvFaceOverview.hpp"
#include "Viewport.hpp"
#include "StartPage.hpp"
#include "io/ProjectIO.hpp"
#include "io/BlockModelImport.hpp"
#include "io/TextureAssets.hpp"
#include "core/Modeling.hpp"

#include <QAction>
#include <QInputDialog>
#include <QToolButton>
#include <QButtonGroup>
#include <QFile>
#include <QMenu>
#include <QTreeWidgetItemIterator>
#include <QAbstractItemView>
#include <QSpacerItem>
#include <QStackedWidget>
#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>
#include <QCoreApplication>
#include <QPushButton>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QDir>
#include <QApplication>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>
#include <QCoreApplication>
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
    setupStartPage();
    // The bundled demo is a formed model: show its geometry when opening the
    // application. New empty documents start in Construction mode instead.
    mode_->setCurrentIndex(1);
    for(int i=0;i<int(project_.parts.size());i++) {
        if(project_.parts[std::size_t(i)].name=="Drum") { selected_=i;break; }
    }
    tool_->setCurrentIndex(tool_->findData(int('w')));
    QSettings pref;
    const QByteArray geometry=pref.value("window/v05/geometry").toByteArray();
    if(!geometry.isEmpty())restoreGeometry(geometry);
    else resize(1600,940);
    const QByteArray state=pref.value("window/v05/state").toByteArray();
    if(!state.isEmpty())restoreState(state);
    else QTimer::singleShot(0,this,[this](){
        auto* left=findChild<QDockWidget*>("PartsDock");
        auto* right=findChild<QDockWidget*>("PropertiesDock");
        if(left&&right)resizeDocks({left,right},{255,345},Qt::Horizontal);
    });
    refreshAll();viewport_->resetCamera();
    // An empty launch is intentional. The user chooses what to make or learn.
    openHome();
}
void MainWindow::buildUI(){
    setObjectName("MainWindow");
    setDockOptions(QMainWindow::AllowNestedDocks|QMainWindow::AllowTabbedDocks|QMainWindow::AnimatedDocks);
    viewport_=new Viewport(this);
    layers_=new LayerCanvas(this);

    auto* central=new QFrame(this);
    central->setObjectName("CentralShell");
    auto* rootLayout=new QHBoxLayout(central);
    rootLayout->setContentsMargins(0,0,0,0);rootLayout->setSpacing(0);
    toolRail_=new QWidget(central);
    toolRail_->setObjectName("ToolRail");toolRail_->setFixedWidth(86);
    railLayout_=new QVBoxLayout(toolRail_);
    railLayout_->setContentsMargins(3,10,3,8);railLayout_->setSpacing(1);
    auto* railTitle=new QLabel("TOOLS",toolRail_);
    railTitle->setObjectName("MicroLabel");railTitle->setAlignment(Qt::AlignCenter);
    railLayout_->addWidget(railTitle);
    railLayout_->addSpacing(7);
    rootLayout->addWidget(toolRail_);

    auto* canvasShell=new QWidget(central);
    auto* canvasLayout=new QVBoxLayout(canvasShell);
    canvasLayout->setContentsMargins(0,0,0,0);canvasLayout->setSpacing(0);
    auto* sceneHeader=new QFrame(canvasShell);sceneHeader->setObjectName("SceneHeader");
    auto* sceneRow=new QHBoxLayout(sceneHeader);sceneRow->setContentsMargins(15,8,15,8);sceneRow->setSpacing(9);
    sceneTitle_=new QLabel("MODEL CANVAS",sceneHeader);
    sceneTitle_->setObjectName("ContextTitle");sceneRow->addWidget(sceneTitle_);
    sceneRow->addStretch(1);
    sceneInfo_=new QLabel("",sceneHeader);sceneInfo_->setObjectName("ContextInfo");
    sceneRow->addWidget(sceneInfo_);
    auto* frame=new QPushButton("Frame  F",sceneHeader);
    frame->setToolTip("Fit entire machine into the viewport (F)");
    connect(frame,&QPushButton::clicked,viewport_,&Viewport::resetCamera);
    sceneRow->addWidget(frame);
    canvasLayout->addWidget(sceneHeader);

    auto* splitter=new QSplitter(Qt::Vertical,canvasShell);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(viewport_);
    layerContainer_=new QWidget(canvasShell);
    auto* layerLayout=new QVBoxLayout(layerContainer_);
    layerLayout->setContentsMargins(0,0,0,0);
    layerLayout->setSpacing(0);
    auto* layerToolbar=new QWidget(layerContainer_);
    auto* layerRow=new QHBoxLayout(layerToolbar);layerRow->setContentsMargins(14,6,14,6);
    layerRow->addWidget(new QLabel("BUILD LAYER",layerToolbar));
    layerSpinner_=new QSpinBox(layerToolbar);
    layerSpinner_->setObjectName("LayerSpinner");layerSpinner_->setRange(0,project_.dimensions[1]-1);
    layerSpinner_->setMaximumWidth(100);layerRow->addWidget(layerSpinner_);
    layerRow->addSpacing(12);
    layerRow->addWidget(new QLabel("PLACE",layerToolbar));
    role_=new QComboBox(layerToolbar);role_->setMinimumWidth(160);
    role_->setObjectName("ConstructionBrush");layerRow->addWidget(role_);
    layerRow->addStretch(1);
    layerLayout->addWidget(layerToolbar);
    layerLayout->addWidget(layers_,1);
    splitter->addWidget(layerContainer_);
    splitter->setStretchFactor(0,3);splitter->setStretchFactor(1,1);
    // The UV workspace is a first-class editing surface, not a small
    // rectangle buried in the properties dock. In Model mode it is hidden and
    // the same OpenGL viewport gets the full canvas width.
    auto* mainViews=new QSplitter(Qt::Horizontal,canvasShell);
    mainViews->setObjectName("UVWorkspaceSplitter");
    mainViews->setChildrenCollapsible(false);
    uvWorkspacePanel_=new QWidget(mainViews);
    uvWorkspacePanel_->setObjectName("UVWorkspacePanel");
    uvWorkspacePanel_->setMinimumWidth(330);
    auto* uvLayout=new QVBoxLayout(uvWorkspacePanel_);
    uvLayout->setContentsMargins(12,12,12,12);
    uvLayout->setSpacing(8);
    auto* heading=new QLabel("CUBOID  /  SIX FACES",uvWorkspacePanel_);
    heading->setObjectName("ContextTitle");
    uvLayout->addWidget(heading);
    auto* helper=new QLabel("Click a face in the net to select it. Each side keeps its own texture and UV mapping.",uvWorkspacePanel_);
    helper->setWordWrap(true);
    helper->setStyleSheet("color:#a9bbc9;");
    uvLayout->addWidget(helper);
    uvFaceOverview_=new UvFaceOverview(uvWorkspacePanel_);
    uvFaceOverview_->setObjectName("UvFaceOverview");
    uvLayout->addWidget(uvFaceOverview_,5);
    auto* atlasTitle=new QLabel("TEXTURE ATLAS  /  SELECTED FACE",uvWorkspacePanel_);
    atlasTitle->setObjectName("ContextTitle");
    uvLayout->addWidget(atlasTitle);
    auto* atlasHint=new QLabel("Solid amber: current face. Dashed colors: other faces using the same PNG. Drag handles below to edit this face.",uvWorkspacePanel_);
    atlasHint->setWordWrap(true);atlasHint->setStyleSheet("color:#a9bbc9;");
    uvLayout->addWidget(atlasHint);
    uvCanvas_=new UvEditor(uvWorkspacePanel_);
    uvCanvas_->setObjectName("UvCanvas");
    uvCanvas_->setMinimumHeight(210);
    uvLayout->addWidget(uvCanvas_,6);
    mainViews->addWidget(uvWorkspacePanel_);
    mainViews->addWidget(splitter);
    mainViews->setStretchFactor(0,3);
    mainViews->setStretchFactor(1,2);
    uvWorkspacePanel_->hide();
    canvasLayout->addWidget(mainViews,1);
    rootLayout->addWidget(canvasShell,1);
    setCentralWidget(central);

    auto* objects=new QDockWidget("OUTLINER",this);
    objects->setObjectName("PartsDock");
    objects->setFeatures(QDockWidget::DockWidgetMovable|QDockWidget::DockWidgetFloatable);
    auto* hierarchy=new QWidget(objects);
    auto* hierarchyLayout=new QVBoxLayout(hierarchy);
    hierarchyLayout->setContentsMargins(8,10,8,8);hierarchyLayout->setSpacing(8);
    outlinerSearch_=new QLineEdit(hierarchy);
    outlinerSearch_->setObjectName("OutlinerSearch");
    outlinerSearch_->setPlaceholderText("Search parts and groups...");
    outlinerSearch_->setClearButtonEnabled(true);
    hierarchyLayout->addWidget(outlinerSearch_);
    tree_=new QTreeWidget(hierarchy);
    tree_->setObjectName("PartHierarchy");
    tree_->setHeaderHidden(true);tree_->setIndentation(16);
    tree_->setUniformRowHeights(true);
    tree_->setSelectionMode(QAbstractItemView::SingleSelection);
    tree_->setContextMenuPolicy(Qt::CustomContextMenu);
    hierarchyLayout->addWidget(tree_,1);
    auto* outlinerActions=new QHBoxLayout;
    outlinerActions->setSpacing(6);
    auto* create=new QPushButton("+ Cube",hierarchy);
    create->setObjectName("AddCubeButton");
    auto* clone=new QPushButton("Duplicate",hierarchy);
    auto* remove=new QPushButton("Delete",hierarchy);
    outlinerActions->addWidget(create);outlinerActions->addWidget(clone);outlinerActions->addWidget(remove);
    hierarchyLayout->addLayout(outlinerActions);
    objects->setWidget(hierarchy);addDockWidget(Qt::LeftDockWidgetArea,objects);
    objects->setMinimumWidth(255);
    connect(create,&QPushButton::clicked,this,&MainWindow::addPart);
    connect(clone,&QPushButton::clicked,this,&MainWindow::duplicatePart);
    connect(remove,&QPushButton::clicked,this,&MainWindow::removePart);
    connect(outlinerSearch_,&QLineEdit::textChanged,this,&MainWindow::filterOutliner);
    connect(tree_,&QTreeWidget::itemDoubleClicked,this,[this](QTreeWidgetItem* item,int){
        if(item && item->data(0,Qt::UserRole).toInt()>=0)renameSelected();
    });
    connect(tree_,&QWidget::customContextMenuRequested,this,[this](const QPoint& point){
        auto* item=tree_->itemAt(point);
        if(!item || item->data(0,Qt::UserRole).toInt()<0)return;
        const int index=item->data(0,Qt::UserRole).toInt();
        setSelected(index);
        QMenu menu(this);
        menu.addAction("Rename...",this,&MainWindow::renameSelected);
        menu.addAction("Duplicate",this,&MainWindow::duplicatePart);
        menu.addAction("Frame selected",viewport_,&Viewport::focusSelected);
        const bool shown=project_.parts[std::size_t(index)].visible;
        menu.addAction(shown?"Hide part":"Show part",this,[this,index](){
            snapshot();project_.parts[std::size_t(index)].visible=!project_.parts[std::size_t(index)].visible;commitChanged();
        });
        menu.addSeparator();menu.addAction("Delete",this,&MainWindow::removePart);
        menu.exec(tree_->viewport()->mapToGlobal(point));
    });

    auto* inspector=new QDockWidget("OBJECT PROPERTIES",this);
    inspector->setObjectName("PropertiesDock");
    QWidget* container=new QWidget(this);
    auto* scroller=new QScrollArea(this);
    scroller->setWidgetResizable(true);scroller->setFrameShape(QFrame::NoFrame);
    scroller->setWidget(container);
    inspector->setWidget(scroller);addDockWidget(Qt::RightDockWidgetArea,inspector);
    inspector->setMinimumWidth(300);
    createInspector();createTextureDock();
    if(auto* props=findChild<QDockWidget*>("PropertiesDock"))props->raise();
    statusMetrics_=new QLabel(this);
    statusMetrics_->setObjectName("ProjectContext");
    statusBar()->addPermanentWidget(statusMetrics_);
    statusBar()->showMessage("Ready  ·  Orbit: left drag empty space  ·  Pan: middle drag  ·  Zoom: wheel",6000);

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
        const int i=axis_->findData(int(axis));
        if(i>=0)axis_->setCurrentIndex(i);
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
    auto* tf=new QGridLayout(transform);
    tf->setSpacing(6);tf->setContentsMargins(10,16,10,9);
    const char* axes[]={"X","Y","Z"};
    for(int i=0;i<3;i++){
        auto* head=new QLabel(axes[i],transform);head->setObjectName("MicroLabel");
        head->setAlignment(Qt::AlignCenter);
        tf->addWidget(head,0,i+1);
    }
    const char* rows[]={"Min","Max","Rotate°","Pivot"};
    for(int row=0;row<4;row++){
        auto* label=new QLabel(rows[row],transform);
        label->setObjectName("MicroLabel");tf->addWidget(label,row+1,0);
        for(int i=0;i<3;i++){
            QDoubleSpinBox* spin=coordinateSpin(transform);
            spin->setMinimumWidth(71);spin->setMaximumWidth(130);
            if(row==0)from_[i]=spin;
            if(row==1)to_[i]=spin;
            if(row==2){angle_[i]=spin;spin->setRange(-3600,3600);spin->setSingleStep(5);}
            if(row==3)pivot_[i]=spin;
            tf->addWidget(spin,row+1,i+1);
        }
    }
    layout->addWidget(transform);
    auto* help=new QLabel("<b>Direct modeling</b><br>"
        "Draw a beam on the workplane, then use the gizmo to move, rotate or resize it. "
        "F frames the full machine; Shift+F focuses the selection.<br><br>"
        "1 block = 16 Minecraft pixels. Imported textures and face UVs remain editable.",contents);
    help->setWordWrap(true);help->setObjectName("InspectorHelp");
    layout->addWidget(help);layout->addStretch(1);
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
void MainWindow::createTextureDock(){
    textureDock_=new QDockWidget("TEXTURES  /  UV",this);
    textureDock_->setObjectName("TextureDock");
    auto* props=findChild<QDockWidget*>("PropertiesDock");
    auto* panel=new QWidget(textureDock_);
    auto* layout=new QVBoxLayout(panel);
    layout->setContentsMargins(10,10,10,10);
    auto* sourceJar=new QPushButton("Load Minecraft assets (.jar / .zip)...",panel);
    sourceJar->setObjectName("ImportMinecraftAssetsButton");
    sourceJar->setToolTip("Import only model-referenced PNGs from your local Minecraft client JAR or resource-pack ZIP");
    layout->addWidget(sourceJar);
    auto* sourceDir=new QPushButton("Load unpacked assets folder...",panel);
    sourceDir->setObjectName("ImportMinecraftAssetsFolderButton");
    sourceDir->setToolTip("Read assets/<namespace>/textures/ from an unpacked resource pack or mod");
    layout->addWidget(sourceDir);
    missingAssetsLabel_=new QLabel(panel);
    missingAssetsLabel_->setObjectName("TextureAssetStatus");
    missingAssetsLabel_->setWordWrap(true);
    missingAssetsLabel_->setStyleSheet("QLabel { color:#e3af79; padding:6px 4px; }");
    layout->addWidget(missingAssetsLabel_);
    connect(sourceJar,&QPushButton::clicked,this,&MainWindow::importMinecraftAssetJar);
    connect(sourceDir,&QPushButton::clicked,this,&MainWindow::importMinecraftAssetFolder);
    auto* import=new QPushButton("+ Import PNG texture",panel);
    import->setObjectName("ImportPngButton");
    layout->addWidget(import);
    auto* fromMinecraft=new QPushButton("Import Java model JSON...",panel);
    fromMinecraft->setObjectName("ImportMinecraftModelButton");
    fromMinecraft->setToolTip("Bring existing Minecraft block-model cuboids into the active model workspace");
    layout->addWidget(fromMinecraft);
    connect(fromMinecraft,&QPushButton::clicked,this,&MainWindow::importBlockModels);
    connect(import,&QPushButton::clicked,this,&MainWindow::importPngTexture);
    auto* form=new QFormLayout;
    uvFace_=new QComboBox(panel);
    uvFace_->setObjectName("UvFaceSelect");
    uvFace_->addItems({"South (+Z)","North (-Z)","West (-X)","East (+X)","Up (+Y)","Down (-Y)"});
    form->addRow("Cuboid face",uvFace_);
    uvTexture_=new QComboBox(panel);
    uvTexture_->setObjectName("UvTextureSelect");
    form->addRow("Face texture",uvTexture_);
    layout->addLayout(form);
    applyTexture_=new QPushButton("Apply selected PNG to whole cube",panel);
    layout->addWidget(applyTexture_);
    connect(applyTexture_,&QPushButton::clicked,this,&MainWindow::applyTextureToCube);
    textureInfo_=new QLabel(panel);
    textureInfo_->setWordWrap(true);
    textureInfo_->setStyleSheet("color:#aec2cf;");
    layout->addWidget(textureInfo_);
    // The live atlas lives in the central UV workspace beside the 3D model.
    // The right-side dock contains numeric controls only; no tiny UV canvas.
    auto* faceHint=new QLabel("Select any of the six faces on the central cube net; edit its atlas region on the larger canvas.",panel);
    faceHint->setWordWrap(true);faceHint->setStyleSheet("color:#9fb2bf;");
    layout->addWidget(faceHint);
    auto* controls=new QGridLayout;
    controls->setSpacing(6);
    auto* autoUV=new QPushButton("Auto UV",panel);
    auto* rotateUV=new QPushButton("Rotate 90°",panel);
    auto* flipU=new QPushButton("Flip U",panel);
    auto* flipV=new QPushButton("Flip V",panel);
    controls->addWidget(autoUV,0,0);controls->addWidget(rotateUV,0,1);
    controls->addWidget(flipU,1,0);controls->addWidget(flipV,1,1);
    layout->addLayout(controls);
    auto* uvForm=new QFormLayout;
    const char* labels[]={"U min","V min","U max","V max"};
    for(int i=0;i<4;i++){
        uvNumbers_[i]=coordinateSpin(panel);
        uvNumbers_[i]->setObjectName(QString("UvNumber%1").arg(i));
        uvNumbers_[i]->setRange(-8.0,32.0);
        uvNumbers_[i]->setDecimals(4);
        uvForm->addRow(labels[i],uvNumbers_[i]);
        connect(uvNumbers_[i],&QDoubleSpinBox::editingFinished,this,&MainWindow::applyUvNumbers);
    }
    layout->addLayout(uvForm);
    auto* hint=new QLabel("UVs are measured in texture tiles (1.0 = full PNG).\n"
        "Auto UV repeats once per Minecraft block, avoiding stretched pixels.\n"
        "Drag corner handles or inside the outlined UV rectangle to edit.",panel);
    hint->setWordWrap(true);hint->setStyleSheet("color:#8fa3b0;");
    layout->addWidget(hint);
    layout->addStretch(1);
    textureDock_->setWidget(panel);
    textureDock_->setMinimumWidth(310);
    addDockWidget(Qt::RightDockWidgetArea,textureDock_);
    if(props)tabifyDockWidget(props,textureDock_);
    textureDock_->raise();
    connect(uvFace_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){refreshTexturePanel();});
    connect(uvFaceOverview_,&UvFaceOverview::facePicked,this,[this](int face){
        if(face>=0&&face<6)uvFace_->setCurrentIndex(face);
    });
    connect(uvTexture_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){applyFaceTexture();});
    connect(autoUV,&QPushButton::clicked,this,&MainWindow::setAutoUv);
    connect(rotateUV,&QPushButton::clicked,this,&MainWindow::rotateUv);
    connect(flipU,&QPushButton::clicked,this,[this](){flipUv(true);});
    connect(flipV,&QPushButton::clicked,this,[this](){flipUv(false);});
    connect(uvCanvas_,&UvEditor::editBegan,this,[this](){uvDragChanged_=false;});
    connect(uvCanvas_,&UvEditor::rectChanged,this,[this](double u0,double v0,double u1,double v1){
        if(selected_<0||selected_>=int(project_.parts.size()))return;
        if(!uvDragChanged_){snapshot();uvDragChanged_=true;}
        auto& part=project_.parts[std::size_t(selected_)];
        auto& face=part.faces[std::size_t(uvFace_->currentIndex())];
        face.uv={u0,v0,u1,v1};face.automatic=false;
        dirty_=true;refreshScene();updateTitle();
        const double vals[]={u0,v0,u1,v1};
        for(int i=0;i<4;i++){QSignalBlocker blocker(uvNumbers_[i]);uvNumbers_[i]->setValue(vals[i]);}
    });
    connect(uvCanvas_,&UvEditor::editEnded,this,[this](){uvDragChanged_=false;refreshTexturePanel();});
}
void MainWindow::refreshTexturePanel(){
    if(!textureDock_||!uvTexture_||!uvCanvas_)return;
    // Named resource paths are never silently approximated with material textures.
    const QStringList needed=referencedTextureIds(project_);
    const QStringList missing=missingTextureIds(project_);
    if(missingAssetsLabel_) {
        if(missing.isEmpty()) {
            missingAssetsLabel_->setText(needed.isEmpty()
                ? "No external Minecraft textures are referenced."
                : QString("All %1 referenced Minecraft PNGs are loaded.").arg(needed.size()));
            missingAssetsLabel_->setStyleSheet("QLabel { color:#8ac8ad; padding:6px 4px; }");
        }else {
            missingAssetsLabel_->setText(QString("%1 missing texture%2. The viewport displays a magenta checker for missing assets, not a substitute.\n%3\nLoad the Minecraft 1.20.1 client JAR or a texture pack to resolve them.")
                .arg(missing.size()).arg(missing.size()==1?"":"s")
                .arg(missing.mid(0,4).join("\n")+(missing.size()>4?"\n...":"")));
            missingAssetsLabel_->setStyleSheet("QLabel { color:#e3af79; padding:6px 4px; }");
        }
    }
    if(missingAssetsLabel_ && project_.notes.find("Static body only")!=std::string::npos){
        missingAssetsLabel_->setText(missingAssetsLabel_->text()+
            "\n\nStatic model only: the in-game renderer also draws the rotating drum, pulleys, belt, water and items.");
    }
    const bool enabled=selected_>=0&&selected_<int(project_.parts.size());
    const int index=std::clamp(uvFace_->currentIndex(),0,5);
    uvFace_->setEnabled(enabled);
    applyTexture_->setEnabled(enabled);
    const FaceUV* selectedFace=enabled?&project_.parts[std::size_t(selected_)].faces[std::size_t(index)]:nullptr;
    const QString faceTexture=selectedFace?q(selectedFace->texture):QString();
    {
        QSignalBlocker blocker(uvTexture_);
        uvTexture_->clear();
        uvTexture_->addItem("Inherit cuboid texture",QString());
        for(const auto& [id,png]:project_.textures){Q_UNUSED(png);uvTexture_->addItem(q(id),q(id));}
        if(!faceTexture.isEmpty()&&uvTexture_->findData(faceTexture)<0)
            uvTexture_->addItem("External: "+faceTexture,faceTexture);
        auto chosen=uvTexture_->findData(faceTexture);
        uvTexture_->setCurrentIndex(std::max(chosen,0));
    }
    uvTexture_->setEnabled(enabled);
    for(auto* spin:uvNumbers_)spin->setEnabled(enabled);
    if(!enabled){
        uvImageKey_.clear();cachedUvImage_=QImage();
        uvCanvas_->setEnabled(false);uvCanvas_->setImage({});uvCanvas_->setRect({0,0,1,1});
        uvCanvas_->setFaceOverlays({},-1);
        uvFaceOverview_->clear();
        textureInfo_->setText("Select a formed-model cuboid to view all six face textures.");
        return;
    }
    uvCanvas_->setEnabled(true);
    const Part& part=project_.parts[std::size_t(selected_)];
    const auto rectangle=effectiveUV(part,index);
    uvCanvas_->setRect(rectangle);
    for(int i=0;i<4;i++){
        QSignalBlocker blocker(uvNumbers_[i]);
        uvNumbers_[i]->setValue(rectangle[std::size_t(i)]);
    }
    const auto texture=selectedFace->texture.empty()?part.texture:selectedFace->texture;
    // Show six independent faces in the cube net, and overlay only mappings
    // that actually use the same PNG on the selected texture atlas.
    std::array<UvFaceOverview::Face,6> overview{};
    std::array<UvEditor::OverlayFace,6> outlines{};
    for(int i=0;i<6;i++){
        const auto& f=part.faces[std::size_t(i)];
        const std::string resource=f.texture.empty()?part.texture:f.texture;
        auto& tile=overview[std::size_t(i)];
        tile.texture=q(resource);
        tile.uv=effectiveUV(part,i);
        tile.automatic=f.automatic;
        tile.quarterTurns=f.quarterTurns;
        const auto resourceImage=project_.textures.find(resource);
        if(resourceImage!=project_.textures.end()){
            tile.image=QImage::fromData(QByteArray::fromBase64(
                QByteArray::fromStdString(resourceImage->second)),"PNG");
        }
        if(resource==texture && i!=index){
            outlines[std::size_t(i)].visible=true;
            outlines[std::size_t(i)].uv=tile.uv;
        }
    }
    uvFaceOverview_->setFaces(overview,index);
    uvCanvas_->setFaceOverlays(outlines,index);
    const auto found=project_.textures.find(texture);
    if(texture!=uvImageKey_){
        uvImageKey_=texture;cachedUvImage_=QImage();
        if(found!=project_.textures.end())
            cachedUvImage_=QImage::fromData(QByteArray::fromBase64(QByteArray::fromStdString(found->second)),"PNG");
        uvCanvas_->setImage(cachedUvImage_);
    }
    textureInfo_->setText(cachedUvImage_.isNull()?"Missing texture: no matching PNG loaded. Choose Minecraft asset JAR or folder above."
        :QString("PNG preview: %1 x %2 px  ·  %3").arg(cachedUvImage_.width()).arg(cachedUvImage_.height()).arg(q(texture)));
}
void MainWindow::importMinecraftAssetJar(){
    const QString file=QFileDialog::getOpenFileName(this,
        "Choose installed Minecraft client JAR or resource-pack ZIP",{},
        "Minecraft JAR or resource pack (*.jar *.zip);;All files (*)");
    if(!file.isEmpty())importTextureSource(file);
}
void MainWindow::importMinecraftAssetFolder(){
    const QString folder=QFileDialog::getExistingDirectory(this,
        "Choose unpacked assets directory (containing assets/minecraft/textures)");
    if(!folder.isEmpty())importTextureSource(folder);
}
void MainWindow::importTextureSource(const QString& source){
    Project updated=project_;
    const auto report=importTextureAssets(updated,source);
    if(!report.error.isEmpty()){
        QMessageBox::warning(this,"Could not read Minecraft textures",report.error);return;
    }
    if(!report.imported.isEmpty()){
        snapshot();project_=std::move(updated);
        uvImageKey_.clear();cachedUvImage_=QImage();
        viewport_->invalidateTextures();
        commitChanged();
    }
    QString details=QString("Imported %1 PNG%2 from the selected local asset source.\n")
        .arg(report.imported.size()).arg(report.imported.size()==1?"":"s");
    if(!report.missing.isEmpty()){
        details+=QString("\n%1 model texture%2 still missing:\n%3")
            .arg(report.missing.size()).arg(report.missing.size()==1?"":"s")
            .arg(report.missing.mid(0,12).join("\n"));
    }else if(!referencedTextureIds(project_).isEmpty())
        details+="\nAll referenced model PNGs are now available in this project.";
    if(!report.warnings.isEmpty())details+="\n\nWarnings:\n"+report.warnings.mid(0,5).join("\n");
    statusBar()->showMessage(QString("Minecraft assets: %1 imported, %2 missing")
        .arg(report.imported.size()).arg(report.missing.size()),9000);
    QMessageBox::information(this,"Texture resource report",details);
    refreshTexturePanel();
}
void MainWindow::importPngTexture(){
    const QString filename=QFileDialog::getOpenFileName(this,"Import texture PNG",{},"PNG images (*.png)");
    if(filename.isEmpty())return;
    QFile file(filename);
    if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(this,"Cannot open texture",file.errorString());return;}
    const auto bytes=file.readAll();
    if(bytes.size()>6*1024*1024){QMessageBox::warning(this,"PNG too large","Use a PNG smaller than 6 MiB.");return;}
    QImage image=QImage::fromData(bytes,"PNG");
    if(image.isNull()||image.width()>4096||image.height()>4096){
        QMessageBox::warning(this,"Unsupported PNG","Texture must be a readable PNG at most 4096x4096 pixels.");return;
    }
    if(project_.textures.size()>=256){QMessageBox::warning(this,"Too many textures","Maximum 256 embedded PNGs per project.");return;}
    QString id="textures/"+q(slugify(QFileInfo(filename).completeBaseName().toStdString()))+".png";
    const QString base=id;int suffix=2;
    while(project_.textures.count(id.toStdString()))id=base.left(base.size()-4)+"_"+QString::number(suffix++)+".png";
    std::size_t totalSize=std::size_t(bytes.size());
    for(const auto& [key,old]:project_.textures){Q_UNUSED(key);totalSize+=old.size()*3/4;}
    if(totalSize>36*1024*1024){QMessageBox::warning(this,"Texture budget exceeded", "Embedded PNGs are limited to 36 MiB per project.");return;}
    snapshot();project_.textures[id.toStdString()]=bytes.toBase64().toStdString();
    if(selected_>=0&&selected_<int(project_.parts.size()))
        project_.parts[std::size_t(selected_)].texture=id.toStdString();
    commitChanged();
    statusBar()->showMessage("Imported "+id+" — embedded inside project file",5500);
}
void MainWindow::applyTextureToCube(){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    const QString texture=uvTexture_->currentData().toString();
    if(texture.isEmpty())return;
    snapshot();auto& part=project_.parts[std::size_t(selected_)];
    part.texture=texture.toStdString();
    for(auto& face:part.faces)face.texture.clear();
    commitChanged();
}
void MainWindow::applyFaceTexture(){
    if(updating_||selected_<0||selected_>=int(project_.parts.size())||uvTexture_->currentIndex()<0)return;
    const auto value=uvTexture_->currentData().toString().toStdString();
    auto& face=project_.parts[std::size_t(selected_)].faces[std::size_t(uvFace_->currentIndex())];
    if(face.texture==value)return;
    snapshot();face.texture=value;commitChanged();
}
void MainWindow::setAutoUv(){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    snapshot();project_.parts[std::size_t(selected_)].faces[std::size_t(uvFace_->currentIndex())].automatic=true;
    commitChanged();
}
void MainWindow::rotateUv(){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    snapshot();auto& f=project_.parts[std::size_t(selected_)].faces[std::size_t(uvFace_->currentIndex())];
    f.quarterTurns=(f.quarterTurns+1)%4;commitChanged();
}
void MainWindow::flipUv(bool horizontal){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    snapshot();auto& p=project_.parts[std::size_t(selected_)];
    const int face=uvFace_->currentIndex();
    makeManualUV(p,face);
    auto& f=p.faces[std::size_t(face)];
    if(horizontal)std::swap(f.uv[0],f.uv[2]);else std::swap(f.uv[1],f.uv[3]);
    commitChanged();
}
void MainWindow::applyUvNumbers(){
    if(updating_||selected_<0||selected_>=int(project_.parts.size()))return;
    const std::array<double,4> uv{uvNumbers_[0]->value(),uvNumbers_[1]->value(),uvNumbers_[2]->value(),uvNumbers_[3]->value()};
    auto& f=project_.parts[std::size_t(selected_)].faces[std::size_t(uvFace_->currentIndex())];
    if(!f.automatic&&f.uv==uv)return;
    snapshot();f.automatic=false;f.uv=uv;commitChanged();
}

void MainWindow::createMenus(){
    auto* file=menuBar()->addMenu("&File");
    file->addAction("Home / Start",QKeySequence("Ctrl+Shift+H"),this,&MainWindow::openHome);
    file->addAction("New project...",QKeySequence::New,this,&MainWindow::createNew);
    file->addAction("New Java block model...",this,&MainWindow::createJavaBlock);
    file->addAction("New Magneticraft2 multiblock...",this,&MainWindow::createMultiblock);
    file->addAction("Open Studio project...",QKeySequence::Open,this,&MainWindow::openDialog);
    file->addAction("Save project",QKeySequence::Save,this,&MainWindow::save);
    file->addAction("Save project as...",QKeySequence::SaveAs,this,&MainWindow::saveAs);
    file->addSeparator();
    auto* examples=file->addMenu("Examples");
    examples->addAction("Mechanical Ore Washer (real model)",this,[this](){openExample("washer");});
    examples->addAction("Mechanical Sifter (construction)",this,[this](){openExample("sifter");});
    examples->addAction("Workshop demonstration",this,[this](){openExample("workshop");});
    examples->addAction("Copper casing (Java block)",this,[this](){openExample("casing");});
    file->addAction("Import Minecraft model JSON...",QKeySequence("Ctrl+I"),this,&MainWindow::importBlockModels);
    file->addAction("Import texture PNG...",this,&MainWindow::importPngTexture);
    file->addAction("Load Minecraft asset JAR / resource-pack ZIP...",this,&MainWindow::importMinecraftAssetJar);
    file->addAction("Load unpacked texture assets folder...",this,&MainWindow::importMinecraftAssetFolder);
    file->addSeparator();
    file->addAction("Export project...",this,&MainWindow::exportDialog);
    file->addSeparator();file->addAction("Quit",QKeySequence::Quit,this,&QWidget::close);

    auto* edit=menuBar()->addMenu("&Edit");
    undoAction_=edit->addAction("Undo",QKeySequence::Undo,this,&MainWindow::undo);
    redoAction_=edit->addAction("Redo",QKeySequence::Redo,this,&MainWindow::redo);
    edit->addSeparator();
    auto* add=edit->addMenu("Add primitive");
    for(const QString name:{"Cube", "Timber beam", "Frame post", "Metal plate", "Drive shaft"})
        add->addAction(name,this,[this,name](){addPrimitive(name);});
    edit->addAction("Duplicate selected",QKeySequence("Ctrl+D"),this,&MainWindow::duplicatePart);
    edit->addAction("Rename selected",QKeySequence("F2"),this,&MainWindow::renameSelected);
    edit->addAction("Delete selected",QKeySequence::Delete,this,&MainWindow::removePart);
    edit->addSeparator();
    edit->addAction("Resize construction layout...",this,&MainWindow::resizeDocument);

    auto* view=menuBar()->addMenu("&View");
    view->addAction("Frame entire structure",QKeySequence("F"),viewport_,&Viewport::resetCamera);
    view->addAction("Frame selected part",QKeySequence("Shift+F"),viewport_,&Viewport::focusSelected);
    view->addSeparator();
    view->addAction("Reset workspace layout",this,&MainWindow::resetWorkspaceLayout);
    auto* help=menuBar()->addMenu("&Help");
    help->addAction("About Magneticraft2 Studio",this,[this](){
        QMessageBox::about(this,"Magneticraft2 Studio",
            "<b>Magneticraft2 Studio — Linux Editor v0.5</b><br><br>"
            "Native Qt 6/C++ modeling with OpenGL 3.3 and editable Java block model imports.<br>"
            "Construction and model workspaces share a saved Studio document.<br><br>"
            "Still in development: animation rigging, texture painting, parent-model resolution and Forge formed rendering.");
    });

    auto* header=addToolBar("Studio header");
    header->setObjectName("StudioHeader");
    header->setMovable(false);header->setFloatable(false);
    auto* brand=new QLabel("◈  MAGNETICRAFT²",this);
    brand->setObjectName("Brand");header->addWidget(brand);
    auto* identity=new QWidget(this);
    auto* identityLayout=new QVBoxLayout(identity);
    identityLayout->setContentsMargins(6,0,10,0);identityLayout->setSpacing(0);
    projectHeading_=new QLabel(this);projectHeading_->setObjectName("ProjectHeading");
    projectContext_=new QLabel(this);projectContext_->setObjectName("ProjectContext");
    identityLayout->addWidget(projectHeading_);identityLayout->addWidget(projectContext_);
    header->addWidget(identity);
    auto* spacer=new QWidget(this);spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    header->addWidget(spacer);

    workspaceBuild_=new QToolButton(this);
    workspaceModel_=new QToolButton(this);
    workspaceUV_=new QToolButton(this);
    workspaceBuild_->setObjectName("WorkspaceBuild");
    workspaceModel_->setObjectName("WorkspaceModel");
    workspaceUV_->setObjectName("WorkspaceUV");
    for(auto* button:{workspaceBuild_,workspaceModel_,workspaceUV_}){
        button->setProperty("workspaceTab",true);button->setCheckable(true);
        header->addWidget(button);
    }
    workspaceBuild_->setText("BUILD");workspaceBuild_->setToolTip("Define the blocks players assemble");
    workspaceModel_->setText("MODEL");workspaceModel_->setToolTip("Edit the formed machine geometry");
    workspaceUV_->setText("UV / TEXTURE");workspaceUV_->setToolTip("Edit per-face texture mapping");
    workspaceBuild_->setAccessibleName("Construction workspace");
    workspaceModel_->setAccessibleName("Model workspace");
    workspaceUV_->setAccessibleName("UV and textures workspace");
    connect(workspaceBuild_,&QToolButton::clicked,this,[this](){
        uvWorkspace_=false;mode_->setCurrentIndex(0);switchWorkspace();
    });
    connect(workspaceModel_,&QToolButton::clicked,this,[this](){
        uvWorkspace_=false;mode_->setCurrentIndex(1);
        if(auto* p=findChild<QDockWidget*>("PropertiesDock"))p->raise();
        switchWorkspace();
        QTimer::singleShot(0,viewport_,&Viewport::resetCamera);
    });
    connect(workspaceUV_,&QToolButton::clicked,this,[this](){
        uvWorkspace_=true;mode_->setCurrentIndex(1);
        // UVs have no use for the transform gizmo; keep the preview clean.
        const int select=tool_->findData(int('v'));
        if(select>=0)tool_->setCurrentIndex(select);
        if(textureDock_)textureDock_->raise();
        switchWorkspace();
        QTimer::singleShot(0,viewport_,&Viewport::resetCamera);
    });
    header->addSeparator();
    auto* homeButton=new QPushButton("Home",this);
    homeButton->setObjectName("HomeButton");
    header->addWidget(homeButton);
    connect(homeButton,&QPushButton::clicked,this,&MainWindow::openHome);
    auto* newButton=new QPushButton("New",this);
    auto* openButton=new QPushButton("Open",this);
    auto* saveButton=new QPushButton("Save",this);
    auto* exportButton=new QPushButton("Export",this);exportButton->setObjectName("PrimaryAction");
    for(auto* b:{newButton,openButton,saveButton,exportButton})header->addWidget(b);
    connect(newButton,&QPushButton::clicked,this,&MainWindow::createNew);
    connect(openButton,&QPushButton::clicked,this,&MainWindow::openDialog);
    connect(saveButton,&QPushButton::clicked,this,&MainWindow::save);
    connect(exportButton,&QPushButton::clicked,this,&MainWindow::exportDialog);

    mode_=new QComboBox(this);mode_->addItems({"Construction","Model"});mode_->hide();
    axis_=new QComboBox(this);axis_->addItem("X",int('x'));axis_->addItem("Y",int('y'));axis_->addItem("Z",int('z'));axis_->hide();
    tool_=new QComboBox(this);
    tool_->setObjectName("ModelToolSelect");
    tool_->addItem("Select",int('v'));
    tool_->addItem("Draw cube",int('b'));
    tool_->addItem("Move",int('w'));
    tool_->addItem("Rotate",int('e'));
    tool_->addItem("Resize",int('r'));
    tool_->hide();

    const struct {char shortcut;const char* glyph;const char* label;} tools[] = {
        {'v',"◈","Select"}, {'b',"▣","Draw"}, {'w',"✥","Move"}, {'e',"⟳","Rotate"}, {'r',"⤢","Resize"}
    };
    for(const auto& spec:tools){
        auto* button=new QToolButton(toolRail_);
        button->setObjectName(QString("Tool%1").arg(spec.label));
        button->setProperty("editorTool",true);button->setCheckable(true);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setText(QString("%1\n%2").arg(QString::fromUtf8(spec.glyph),QString::fromLatin1(spec.label)));
        button->setToolTip(QString("%1  (%2)").arg(spec.label).arg(QChar(spec.shortcut).toUpper()));
        button->setAccessibleName(QString(spec.label)+" tool");
        button->setFixedHeight(65);
        railLayout_->addWidget(button);
        railButtons_[spec.shortcut]=button;
        connect(button,&QToolButton::clicked,this,[this,spec](){
            const int index=tool_->findData(int(spec.shortcut));
            if(index>=0)tool_->setCurrentIndex(index);
            else syncWorkspaceChrome();
            viewport_->setFocus();
        });
    }
    railLayout_->addStretch(1);
    auto* railHint=new QLabel("V B W\nE R",toolRail_);
    railHint->setAlignment(Qt::AlignCenter);railHint->setObjectName("MicroLabel");
    railLayout_->addWidget(railHint);

    // The contextual row must occupy its own line. On 1280px displays the
    // previous combined header squeezed view/snap controls off-screen.
    addToolBarBreak(Qt::TopToolBarArea);
    modelToolbar_=addToolBar("Modeling context");
    modelToolbar_->setObjectName("ModelAuthoringToolbar");
    modelToolbar_->setMovable(false);modelToolbar_->setFloatable(false);
    modelToolbar_->addWidget(new QLabel("SNAP",this));
    snap_=new QComboBox(this);snap_->setObjectName("SnapSelect");
    snap_->addItem("1 px   ·   1/16",1.0/16.0);
    snap_->addItem("2 px   ·   1/8",1.0/8.0);
    snap_->addItem("4 px   ·   1/4",1.0/4.0);
    snap_->addItem("8 px   ·   1/2",1.0/2.0);
    snap_->addItem("16 px   ·   1 block",1.0);
    modelToolbar_->addWidget(snap_);
    modelToolbar_->addSeparator();
    modelToolbar_->addWidget(new QLabel("PLANE Y",this));
    workPlane_=coordinateSpin(this);workPlane_->setValue(0);workPlane_->setMaximumWidth(93);
    modelToolbar_->addWidget(workPlane_);
    modelToolbar_->addWidget(new QLabel("EXTRUDE",this));
    drawHeight_=coordinateSpin(this);drawHeight_->setRange(1.0/16.0,128);drawHeight_->setValue(1);
    drawHeight_->setMaximumWidth(93);modelToolbar_->addWidget(drawHeight_);
    modelToolbar_->addSeparator();
    modelToolbar_->addWidget(new QLabel("VIEW",this));
    viewPreset_=new QComboBox(this);viewPreset_->setObjectName("CameraPreset");
    viewPreset_->addItems({"Perspective","Top","Front","Right"});
    modelToolbar_->addWidget(viewPreset_);
    ghost_=new QCheckBox("Show build guides",this);ghost_->setObjectName("ShowBuildGuides");
    modelToolbar_->addWidget(ghost_);
    connect(mode_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){switchWorkspace();});
    connect(ghost_,&QCheckBox::toggled,this,[this](bool){refreshScene();});
    connect(tool_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){
        const char t=char(tool_->currentData().toInt());
        viewport_->setTool(t);
        if(t=='b' && viewPreset_->currentIndex()>=2)viewPreset_->setCurrentIndex(1);
        drawHeight_->setEnabled(t=='b');
        syncWorkspaceChrome();
    });
    connect(axis_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){viewport_->setAxis(char(axis_->currentData().toInt()));});
    connect(snap_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int){viewport_->setGridStep(snap_->currentData().toDouble());});
    connect(workPlane_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double y){viewport_->setWorkPlane(y);});
    connect(drawHeight_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double h){viewport_->setDrawHeight(h);});
    connect(viewPreset_,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int view){viewport_->setCameraPreset(view);syncWorkspaceChrome();});
    viewport_->setTool('v');viewport_->setAxis('x');
    for(const auto key:{Qt::Key_V,Qt::Key_B,Qt::Key_W,Qt::Key_E,Qt::Key_R,Qt::Key_X,Qt::Key_Y,Qt::Key_Z}){
        auto* shortcut=new QShortcut(QKeySequence(key),viewport_);
        shortcut->setContext(Qt::WidgetShortcut);
        connect(shortcut,&QShortcut::activated,this,[this,key](){
            const char c=char(key);
            if(c=='V'||c=='B'||c=='W'||c=='E'||c=='R'){
                const char lower=char(c+('a'-'A'));
                const int index=tool_->findData(int(lower));
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
    // Keep manually collapsed/expanded groups stable across edits, renames,
    // selection changes and undo/redo; refreshing should not reopen everything.
    std::map<std::string,bool> expansion;
    for(int i=0;i<tree_->topLevelItemCount();++i){
        const auto* item=tree_->topLevelItem(i);
        expansion[item->text(0).toStdString()]=item->isExpanded();
    }
    tree_->clear();
    std::map<std::string,QTreeWidgetItem*> groups;
    for(std::size_t i=0;i<project_.parts.size();i++){
        const auto& part=project_.parts[i];
        const auto group=part.group.empty()?std::string("Ungrouped"):part.group;
        if(!groups.count(group)){
            auto* header=new QTreeWidgetItem(tree_);
            header->setText(0,q(group));
            header->setData(0,Qt::UserRole,-1);
            const auto old=expansion.find(group);
            const bool first=groups.empty();
            header->setExpanded(old!=expansion.end()?old->second:
                (project_.parts.size()<30 || first));
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
    filterOutliner();
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
    refreshTexturePanel();
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
    const auto detail=QString("%1%2 × %3 × %4   /   %5 construction cells   /   %6 model parts")
        .arg(project_.kind==ProjectKind::JavaBlock?"Java block  ·  ":"Multiblock  ·  ")
        .arg(project_.dimensions[0]).arg(project_.dimensions[1]).arg(project_.dimensions[2])
        .arg(project_.cells.size()).arg(project_.parts.size());
    if(statusMetrics_)statusMetrics_->setText(detail+"  ");
    if(sceneInfo_) {
        const bool staticReference=view.showFormed &&
            project_.notes.find("Static body only")!=std::string::npos;
        if(staticReference){
            const int unresolved=missingTextureIds(project_).size();
            sceneInfo_->setText(QString("STATIC BODY ONLY  ·  %1 missing PNG%2  ·  moving parts excluded")
                .arg(unresolved).arg(unresolved==1?"":"s"));
            sceneInfo_->setToolTip("Only the Ore Washer's 66 baked static cuboids are imported. "
                "The in-game renderer separately draws the drum, pulleys, leather belt, water and items.");
            sceneInfo_->setStyleSheet("color:#e2b17c; font-weight:600;");
        }else{
            sceneInfo_->setText(view.showFormed
                ? (selected_>=0 && selected_<int(project_.parts.size())
                    ? q(project_.parts[std::size_t(selected_)].name)+"  ·  "+QString::number(project_.parts.size())+" parts"
                    : QString::number(project_.parts.size())+" model parts")
                : "Layer "+QString::number(layer_+1)+" / "+QString::number(project_.dimensions[1]));
            sceneInfo_->setToolTip({});
            sceneInfo_->setStyleSheet({});
        }
    }
}
void MainWindow::updateTitle(){
    setWindowTitle(QString("%1%2  —  Magneticraft2 Studio")
        .arg(q(project_.name),dirty_?" *":""));
    if(projectHeading_)projectHeading_->setText(q(project_.name)+(dirty_?"  •":""));
    if(projectContext_)projectContext_->setText(filename_.isEmpty()
        ? (project_.kind==ProjectKind::JavaBlock?"Unsaved Java block model":"Unsaved multiblock")
        : "Project  ·  "+QFileInfo(filename_).fileName());
    undoAction_->setEnabled(!undo_.empty());redoAction_->setEnabled(!redo_.empty());
}
void MainWindow::refreshAll(){
    layerSpinner_->setRange(0,project_.dimensions[1]-1);
    layer_=std::min(layer_,project_.dimensions[1]-1);
    layerSpinner_->setValue(layer_);
    refreshRolePalette();refreshPartTree();refreshInspector();refreshScene();refreshTexturePanel();updateTitle();syncWorkspaceChrome();
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
    uvImageKey_.clear();viewport_->invalidateTextures();
    if(selected_>=int(project_.parts.size()))selected_=-1;
    commitChanged();
}
void MainWindow::redo(){
    if(redo_.empty())return;
    undo_.push_back(project_);project_=std::move(redo_.back());redo_.pop_back();
    uvImageKey_.clear();viewport_->invalidateTextures();
    if(selected_>=int(project_.parts.size()))selected_=-1;
    commitChanged();
}
void MainWindow::addPart(){addPrimitive("Cube");}
void MainWindow::addDrawnBox(double x0,double z0,double x1,double z1){
    if(mode_->currentIndex()!=1)return;
    auto bounds=drawnCuboid({x0,0,z0},{x1,0,z1},workPlane_->value(),
        drawHeight_->value(),snap_->currentData().toDouble());
    if(!partBoundsAllowed(project_,bounds)){
        statusBar()->showMessage("Java block model bounds are limited to 3 × 3 × 3 (−16..32 model pixels)",6500);
        return;
    }
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
    if(!mode_||!viewport_||!layerContainer_)return;
    const bool isModel=mode_->currentIndex()==1;
    if(!isModel)uvWorkspace_=false;
    layerContainer_->setVisible(!isModel);
    if(toolRail_)toolRail_->setVisible(isModel&&!uvWorkspace_);
    if(uvWorkspacePanel_)uvWorkspacePanel_->setVisible(isModel&&uvWorkspace_);
    if(textureDock_)textureDock_->setVisible(isModel);
    if(workspaceBuild_)workspaceBuild_->setEnabled(project_.kind!=ProjectKind::JavaBlock);
    if(ghost_)ghost_->setEnabled(project_.kind!=ProjectKind::JavaBlock);
    if(snap_)snap_->setEnabled(isModel);
    if(workPlane_) {
        workPlane_->setEnabled(isModel);
        workPlane_->setRange(project_.kind==ProjectKind::JavaBlock?0.0:-100000.0,
                             project_.kind==ProjectKind::JavaBlock?3.0:100000.0);
    }
    if(drawHeight_)drawHeight_->setEnabled(isModel&&tool_->currentData().toInt()==int('b'));
    if(ghost_)ghost_->setEnabled(isModel&&project_.kind!=ProjectKind::JavaBlock);
    if(modelToolbar_)modelToolbar_->setVisible(isModel&&!uvWorkspace_);
    syncWorkspaceChrome();
    refreshScene();
}
void MainWindow::syncWorkspaceChrome(){
    const bool isModel=mode_&&mode_->currentIndex()==1;
    if(workspaceBuild_)workspaceBuild_->setChecked(!isModel);
    if(workspaceModel_)workspaceModel_->setChecked(isModel&&!uvWorkspace_);
    if(workspaceUV_)workspaceUV_->setChecked(isModel&&uvWorkspace_);
    if(sceneTitle_)sceneTitle_->setText(isModel?(uvWorkspace_?"UV  /  3D PREVIEW":"FORMED MODEL  /  3D EDITOR"):
        "CONSTRUCTION  /  BUILD LAYOUT");
    if(tool_){
        const char current=char(tool_->currentData().toInt());
        for(const auto& [key,button]:railButtons_)if(button)button->setChecked(key==current);
    }
}
void MainWindow::filterOutliner(){
    if(!tree_ || !outlinerSearch_)return;
    const QString query=outlinerSearch_->text().trimmed();
    for(int i=0;i<tree_->topLevelItemCount();++i){
        auto* group=tree_->topLevelItem(i);
        const bool matchingGroup=group->text(0).contains(query,Qt::CaseInsensitive);
        bool visible=false;
        for(int j=0;j<group->childCount();++j){
            auto* item=group->child(j);
            const bool match=query.isEmpty()||matchingGroup||item->text(0).contains(query,Qt::CaseInsensitive);
            item->setHidden(!match);visible|=match;
        }
        group->setHidden(!visible);
        if(!query.isEmpty()&&visible)group->setExpanded(true);
    }
}
void MainWindow::renameSelected(){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    bool accepted=false;
    QString value=QInputDialog::getText(this,"Rename model part","Part name",QLineEdit::Normal,
                                      q(project_.parts[std::size_t(selected_)].name),&accepted).trimmed();
    if(!accepted||value.isEmpty()||value==q(project_.parts[std::size_t(selected_)].name))return;
    snapshot();project_.parts[std::size_t(selected_)].name=value.toStdString();commitChanged();
}
void MainWindow::addPrimitive(const QString& kind){
    const double cx=project_.dimensions[0]*0.5,cz=project_.dimensions[2]*0.5;
    const double y=project_.kind==ProjectKind::JavaBlock?1.0:(workPlane_?workPlane_->value():0);
    Vec3 size{1,1,1};QString material="wood",group="Structure";
    if(kind=="Timber beam")size={4,.25,.25};
    else if(kind=="Frame post")size={.25,3,.25};
    else if(kind=="Metal plate"){size={2,.125,2};material="copper";group="Hardware";}
    else if(kind=="Drive shaft"){size={3,.1875,.1875};material="iron";group="Motion";}
    else group="Geometry";
    Bounds bounds{{cx-size.x*.5,y,cz-size.z*.5},{cx+size.x*.5,y+size.y,cz+size.z*.5}};
    if(project_.kind==ProjectKind::JavaBlock){
        const Vec3 extent=bounds.to-bounds.from;
        bounds.from.x=std::max(0.0,std::min(3.0-std::min(3.0,extent.x),bounds.from.x));
        bounds.from.y=std::max(0.0,std::min(3.0-std::min(3.0,extent.y),bounds.from.y));
        bounds.from.z=std::max(0.0,std::min(3.0-std::min(3.0,extent.z),bounds.from.z));
        bounds.to={bounds.from.x+std::min(3.0,extent.x),
                   bounds.from.y+std::min(3.0,extent.y),
                   bounds.from.z+std::min(3.0,extent.z)};
    }
    auto part=makeBox(project_,bounds);
    part.name=kind.toStdString();part.group=group.toStdString();part.material=material.toStdString();
    snapshot();project_.parts.push_back(std::move(part));selected_=int(project_.parts.size())-1;
    uvWorkspace_=false;mode_->setCurrentIndex(1);commitChanged();viewport_->focusSelected();
}
void MainWindow::resetWorkspaceLayout(){
    if(auto* parts=findChild<QDockWidget*>("PartsDock")){
        addDockWidget(Qt::LeftDockWidgetArea,parts);parts->show();parts->setFloating(false);
    }
    auto* props=findChild<QDockWidget*>("PropertiesDock");
    if(props){addDockWidget(Qt::RightDockWidgetArea,props);props->show();props->setFloating(false);}
    if(textureDock_){
        if(props)tabifyDockWidget(props,textureDock_);
        textureDock_->setFloating(false);
        textureDock_->setVisible(mode_ && mode_->currentIndex()==1);
        if(props)props->raise();
    }
    resizeDocks({findChild<QDockWidget*>("PartsDock"),props},{255,320},Qt::Horizontal);
    QSettings pref;pref.remove("window/v05/state");
}
void MainWindow::importBlockModels(){
    const auto files=QFileDialog::getOpenFileNames(this,"Import Minecraft Java block models",{},
                     "Minecraft models (*.json);;All files (*)");
    if(files.isEmpty())return;
    // Import relative to the formation controller, not to the world origin.
    // This matches how the washer's baked model extends beyond (-16..32) pixels.
    Vec3 anchor=project_.kind==ProjectKind::JavaBlock?Vec3{1,1,1}:
        Vec3{double(project_.dimensions[0]/2),0,double(project_.dimensions[2]/2)};
    for(const auto& [cell,role]:project_.cells){
        if(role=="Controller"){anchor={double(cell.x),double(cell.y),double(cell.z)};break;}
    }
    Project draft=project_;
    QStringList warnings;
    std::size_t added=0;
    for(const QString& fileName:files){
        QFile file(fileName);
        if(!file.open(QIODevice::ReadOnly)){
            QMessageBox::warning(this,"Model import failed",fileName+": "+file.errorString());return;
        }
        const auto result=importBlockModel(file.readAll(),draft,QFileInfo(fileName).completeBaseName(),anchor);
        if(!result.error.isEmpty()){
            QMessageBox::warning(this,"Model import failed",QFileInfo(fileName).fileName()+": "+result.error);return;
        }
        if(draft.parts.size()+result.parts.size()>20000){
            QMessageBox::warning(this,"Too many parts","A Studio project supports up to 20,000 model elements in one import.");return;
        }
        added+=result.parts.size();
        for(const auto& warning:result.warnings)warnings << QFileInfo(fileName).fileName()+": "+warning;
        for(const auto& part:result.parts){
            if(!partBoundsAllowed(draft,part.bounds)){
                QMessageBox::warning(this,"Java block model limit",
                    "Imported element '"+q(part.name)+"' would exceed the normal Java block model "
                    "coordinates (−16..32 pixels). Use a multiblock project for larger geometry.");
                return;
            }
            draft.parts.push_back(part);
        }
    }
    if(added==0)return;
    snapshot();project_=std::move(draft);
    selected_=int(project_.parts.size())-1;
    uvWorkspace_=false;
    mode_->setCurrentIndex(1);
    commitChanged();viewport_->resetCamera();
    statusBar()->showMessage(QString("Imported %1 editable cuboids into the formed model").arg(added),8000);
    if(!warnings.isEmpty()){
        if(warnings.size()>8)warnings=warnings.mid(0,8)+QStringList{"Additional import warnings omitted."};
        QMessageBox::information(this,"Imported with limitations",warnings.join("\n"));
    }
}
void MainWindow::duplicatePart(){
    if(selected_<0||selected_>=int(project_.parts.size()))return;
    snapshot();Part part=project_.parts[std::size_t(selected_)];
    part.id=nextPartId(project_);part.name+=" Copy";
    Vec3 offset{0.25,0,0.25};
    auto moved=Bounds{part.bounds.from+offset,part.bounds.to+offset};
    if(!partBoundsAllowed(project_,moved)){
        offset={-0.25,0,-0.25};
        moved={part.bounds.from+offset,part.bounds.to+offset};
        if(!partBoundsAllowed(project_,moved))offset={0,0,0};
    }
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
    if(!partBoundsAllowed(project_,part.bounds)){
        statusBar()->showMessage(project_.kind==ProjectKind::JavaBlock
            ? "Java block model limit: each element must stay within the 3 × 3 × 3 modeling envelope"
            : "Invalid cuboid dimensions: each Min must be smaller than Max",6500);
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
    if(tool=='w'){
        if(project_.kind==ProjectKind::JavaBlock){
            amount=std::clamp(amount,0.0-*lo,3.0-*hi);
            if(amount==0)return;
        }
        *lo+=amount;*hi+=amount;*pivot+=amount;
    }
    else if(tool=='e')*angle+=amount;
    else if(tool=='r'){
        const double requested=std::max(*lo+1.0/16.0,*hi+amount);
        *hi=project_.kind==ProjectKind::JavaBlock ? std::min(3.0,requested):requested;
    }
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
static QString bundledSample(const QString& relative);
void MainWindow::setupStartPage(){
    editorSurface_=takeCentralWidget();
    centralStack_=new QStackedWidget(this);
    centralStack_->setObjectName("StudioPageStack");
    startPage_=new StartPage(centralStack_);
    centralStack_->addWidget(startPage_);
    centralStack_->addWidget(editorSurface_);
    setCentralWidget(centralStack_);
    connect(startPage_,&StartPage::createJavaBlockRequested,this,&MainWindow::createJavaBlock);
    connect(startPage_,&StartPage::createMultiblockRequested,this,&MainWindow::createMultiblock);
    connect(startPage_,&StartPage::openProjectRequested,this,&MainWindow::openDialog);
    connect(startPage_,&StartPage::exampleRequested,this,&MainWindow::openExample);
    connect(startPage_,&StartPage::recentProjectRequested,this,[this](const QString& path){
        if(!maybeSave())return;
        openPath(path);
    });
    loadRecentProjects();
}
void MainWindow::loadRecentProjects(){
    QSettings prefs;
    recentProjects_=prefs.value("files/recentProjects").toStringList();
    if(startPage_)startPage_->setRecentProjects(recentProjects_);
}
void MainWindow::rememberRecentProject(const QString& path){
    const QString absolute=QFileInfo(path).absoluteFilePath();
    recentProjects_.removeAll(absolute);
    recentProjects_.prepend(absolute);
    while(recentProjects_.size()>8)recentProjects_.removeLast();
    QSettings prefs;prefs.setValue("files/recentProjects",recentProjects_);
    if(startPage_)startPage_->setRecentProjects(recentProjects_);
}
void MainWindow::openHome(){
    if(atHome_)return;
    if(!maybeSave())return;
    hasDocument_=false;atHome_=true;dirty_=false;
    if(startPage_)startPage_->setRecentProjects(recentProjects_);
    if(centralStack_)centralStack_->setCurrentWidget(startPage_);
    for(auto* dock:findChildren<QDockWidget*>())dock->hide();
    for(auto* bar:findChildren<QToolBar*>())bar->hide();
    setWindowTitle("Magneticraft2 Studio — Home");
    if(statusMetrics_)statusMetrics_->hide();
    statusBar()->showMessage("Choose a project type, open an example, or continue a saved project");
}
void MainWindow::activateEditor(){
    if(!centralStack_)return;
    atHome_=false;
    if(statusMetrics_)statusMetrics_->show();
    centralStack_->setCurrentWidget(editorSurface_);
    if(auto* header=findChild<QToolBar*>("StudioHeader"))header->show();
    if(auto* left=findChild<QDockWidget*>("PartsDock"))left->show();
    if(auto* right=findChild<QDockWidget*>("PropertiesDock"))right->show();
    switchWorkspace();
    if(mode_->currentIndex()==1 && !uvWorkspace_){
        if(auto* p=findChild<QDockWidget*>("PropertiesDock"))p->raise();
    }
}
void MainWindow::openExample(const QString& key){
    if(key=="washer"){openOreWasherReference();return;}
    if(key=="sifter"){
        if(!maybeSave())return;
        const QString path=bundledSample("mechanical_sifter_north.json");
        LoadResult example;QString error;
        if(path.isEmpty()||!ProjectIO::read(path,example,error)){
            QMessageBox::warning(this,"Cannot open Sifter example",
                path.isEmpty()?"The Sifter construction example was not installed.":error);
            return;
        }
        project_=std::move(example.project);
        project_.name="Mechanical Sifter — Construction";
        project_.notes="This example contains the real Magneticraft2 Sifter construction layout. "
                       "Its formed model has not been imported yet.";
        loadedJson_=example.original;filename_.clear();dirty_=true;hasDocument_=true;
        undo_.clear();redo_.clear();selected_=-1;layer_=0;uvWorkspace_=false;
        mode_->setCurrentIndex(0);activateEditor();refreshAll();viewport_->resetCamera();return;
    }
    if(key=="workshop"||key=="casing"){
        if(!maybeSave())return;
        project_=key=="workshop"?makeDemo():makeJavaBlockProject("Copper Casing — Example");
        project_.notes=key=="workshop" ? "Simple tutorial machine, not the actual Ore Washer."
                                           : "Java block example in the 3x3x3 model envelope.";
        loadedJson_={};filename_.clear();dirty_=true;hasDocument_=true;
        undo_.clear();redo_.clear();layer_=0;uvWorkspace_=false;
        selected_=key=="casing"?0:-1;
        mode_->setCurrentIndex(1);activateEditor();refreshAll();viewport_->resetCamera();return;
    }
}
void MainWindow::createNew(){ openHome(); }
void MainWindow::createJavaBlock(){
    if(!maybeSave())return;
    uvImageKey_.clear();viewport_->invalidateTextures();
    project_=makeJavaBlockProject("New Java Block");
    dirty_=false;hasDocument_=true;filename_.clear();loadedJson_={};
    selected_=0;undo_.clear();redo_.clear();layer_=0;uvWorkspace_=false;
    mode_->setCurrentIndex(1);
    activateEditor();refreshAll();viewport_->resetCamera();
}
void MainWindow::createMultiblock(){
    if(!maybeSave())return;
    QString name="New Multiblock";Dimensions dims{7,5,9};
    if(!sizeDialog(this,"Create Multiblock",name,dims,true))return;
    uvImageKey_.clear();viewport_->invalidateTextures();
    project_=makeProject(name.toStdString(),dims);
    dirty_=false;hasDocument_=true;filename_.clear();loadedJson_={};selected_=-1;
    undo_.clear();redo_.clear();layer_=0;uvWorkspace_=false;
    mode_->setCurrentIndex(0);
    activateEditor();refreshAll();viewport_->resetCamera();
}
void MainWindow::resizeDocument(){
    if(project_.kind==ProjectKind::JavaBlock){
        QMessageBox::information(this,"Fixed Java model size",
            "Java block models use a fixed 3 × 3 × 3 modeling envelope (−16..32 pixels). "
            "Use a Multiblock project for larger machines.");return;
    }
    QString name=q(project_.name);Dimensions dims=project_.dimensions;
    if(!sizeDialog(this,"Resize Multiblock",name,dims,false))return;
    snapshot();studio::resize(project_,dims);selected_=-1;commitChanged();viewport_->resetCamera();
}
// Locate the sample both when running the source checkout and after an
// installed CMake build. No network service or external installation is used.
static QString bundledSample(const QString& relative){
    const QDir cwd=QDir::current();
    const QDir binary=QDir(QCoreApplication::applicationDirPath());
    for(const auto& root:{cwd.absolutePath(),binary.absoluteFilePath(".."),
                          binary.absoluteFilePath("../share/magneticraft2-studio")}){
        const QString candidate=QDir(root).filePath("samples/"+relative);
        if(QFileInfo(candidate).isFile())return candidate;
    }
    return {};
}
bool MainWindow::openOreWasherReference(){
    auto fail=[this](const QString& title,const QString& message){
        if(QCoreApplication::arguments().contains("--smoke"))qCritical().noquote()<<title+": "+message;
        else QMessageBox::warning(this,title,message);
        return false;
    };
    qInfo("Studio: opening authentic Ore Washer reference");
    if(!maybeSave())return false;
    const QString structure=bundledSample("mechanical_ore_washer_north.json");
    const QString model=bundledSample("ore_washer_models/mechanical_ore_washer_north.json");
    if(structure.isEmpty()||model.isEmpty()){
        return fail("Sample files unavailable",
            "The Ore Washer reference needs the samples/ folder shipped with Studio. "
            "Run from the repository root or install the CMake sample data.");
    }
    LoadResult structureResult;QString error;
    if(!ProjectIO::read(structure,structureResult,error)){
        return fail("Cannot open sample",error);
    }
    QFile modelFile(model);
    if(!modelFile.open(QIODevice::ReadOnly)){
        return fail("Cannot open model",modelFile.errorString());
    }
    auto example=std::move(structureResult.project);
    Vec3 anchor{double(example.dimensions[0]/2),0,double(example.dimensions[2]/2)};
    for(const auto& [cell,role]:example.cells){if(role=="Controller"){
        anchor={double(cell.x),double(cell.y),double(cell.z)};break;
    }}
    const auto result=importBlockModel(modelFile.readAll(),example,"Ore Washer / Static body",anchor);
    if(!result.error.isEmpty()){
        return fail("Cannot convert sample model",result.error);
    }
    // The source JSON has anonymous elements, not named components. Present
    // a useful inferred hierarchy by source texture and geometry aspect ratio
    // without pretending to reconstruct the original animation rig.
    std::map<std::string,int> counts;
    for(auto part:result.parts){
        const QString texture=q(part.texture).toLower();
        const QString category=texture.contains("copper")?"Copper":
                               texture.contains("spruce")?"Spruce frame":
                               texture.contains("oak")?"Oak timber":"Other";
        const auto d=part.bounds.to-part.bounds.from;
        std::array<double,3> sizes{{d.x,d.y,d.z}};
        std::sort(sizes.begin(),sizes.end());
        const QString shape=sizes[2]>3.0*std::max(0.125,sizes[1])?"Beam":
                            sizes[0]<0.25*sizes[1]?"Panel":"Cuboid";
        part.group=QString("Ore Washer / %1").arg(category).toStdString();
        const int n=++counts[part.group];
        part.name=QString("%1 %2").arg(shape).arg(n,3,10,QChar('0')).toStdString();
        example.parts.push_back(std::move(part));
    }
    example.name="Mechanical Ore Washer — Reference";
    example.notes="Imported from the actual Magneticraft2 model JSON (north). Static body only; "
                  "rotor, input pulley and animation are not automatically assembled.";
    uvImageKey_.clear();viewport_->invalidateTextures();
    project_=std::move(example);loadedJson_=structureResult.original;
    hasDocument_=true;
    filename_.clear();dirty_=true;undo_.clear();redo_.clear();selected_=-1;layer_=0;
    uvWorkspace_=false;mode_->setCurrentIndex(1);
    activateEditor();refreshAll();viewport_->resetCamera();
    qInfo("Studio: Ore Washer model imported with %lld editable cuboids",static_cast<long long>(result.parts.size()));
    const auto missing=missingTextureIds(project_);
    statusBar()->showMessage(QString("Ore Washer: %1 static parts, %2 missing PNG assets. The rotating drum, belt, pulleys and water are separate renderer layers.")
            .arg(result.parts.size()).arg(missing.size()),13000);
    return true;
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
    uvImageKey_.clear();viewport_->invalidateTextures();
    project_=std::move(result.project);loadedJson_=std::move(result.original);
    hasDocument_=true;
    mode_->setCurrentIndex(project_.parts.empty()?0:1);
    selected_=-1;layer_=0;filename_=result.fromLegacy ? QString() : path;dirty_=false;undo_.clear();redo_.clear();
    activateEditor();refreshAll();viewport_->resetCamera();
    if(!result.fromLegacy)rememberRecentProject(path);
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
    filename_=path;dirty_=false;hasDocument_=true;
    rememberRecentProject(path);updateTitle();
    statusBar()->showMessage("Saved "+path,4000);return true;
}
bool MainWindow::maybeSave(){
    if(!hasDocument_||!dirty_)return true;
    const auto answer=QMessageBox::question(this,"Unsaved changes",
        "Save changes to this project before continuing?",
        QMessageBox::Save|QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Save);
    if(answer==QMessageBox::Cancel)return false;
    if(answer==QMessageBox::Discard)return true;
    save();return !dirty_;
}
void MainWindow::exportDialog(){
    if(project_.kind==ProjectKind::JavaBlock){
        const QString filename=QFileDialog::getSaveFileName(this,"Export Java block model",
            q(project_.id)+".json","Minecraft Java model (*.json)");
        if(filename.isEmpty())return;
        QString error;
        if(!ProjectIO::exportJavaBlockModel(filename,project_,error))
            QMessageBox::warning(this,"Java block export failed",error);
        else statusBar()->showMessage("Java block model exported: "+filename,5000);
        return;
    }
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
    pref.setValue("window/v05/geometry",saveGeometry());pref.setValue("window/v05/state",saveState());
    event->accept();
}

} // namespace studio
