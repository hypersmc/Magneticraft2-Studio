#include "MainWindow.hpp"
#include "LayerCanvas.hpp"
#include "UvEditor.hpp"
#include "Viewport.hpp"
#include "io/ProjectIO.hpp"
#include "core/Modeling.hpp"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QImage>
#include <QImageReader>
#include <QFileInfo>
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
    // Show the real manipulation handles in the bundled demo from startup.
    tool_->setCurrentIndex(tool_->findData(int('w')));
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

    auto* objects=new QDockWidget("MODEL PARTS",this);
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
    createTextureDock();
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
    auto* help=new QLabel("MODEL TOOLS\nV Select / B Draw cube\nW Move / E Rotate / R Resize\nDrag red, green, blue handles for axes.\nMove: drag plane squares or white center for free motion.\nResize: drag white center for uniform resize.\nTexture & UV panel: import PNG, assign faces and drag UVs.\n1 unit = 16 Minecraft pixels.",contents);
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
void MainWindow::createTextureDock(){
    textureDock_=new QDockWidget("TEXTURES  /  UV",this);
    textureDock_->setObjectName("TextureDock");
    auto* props=findChild<QDockWidget*>("PropertiesDock");
    auto* panel=new QWidget(textureDock_);
    auto* layout=new QVBoxLayout(panel);
    layout->setContentsMargins(10,10,10,10);
    auto* import=new QPushButton("+ Import PNG texture",panel);
    import->setObjectName("ImportPngButton");
    layout->addWidget(import);
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
    uvCanvas_=new UvEditor(panel);
    uvCanvas_->setObjectName("UvCanvas");
    layout->addWidget(uvCanvas_);
    auto* controls=new QHBoxLayout;
    auto* autoUV=new QPushButton("Auto UV",panel);
    auto* rotateUV=new QPushButton("Rotate 90°",panel);
    auto* flipU=new QPushButton("Flip U",panel);
    auto* flipV=new QPushButton("Flip V",panel);
    for(auto* b:{autoUV,rotateUV,flipU,flipV})controls->addWidget(b);
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
        textureInfo_->setText("Select a formed-model cuboid to edit its face textures.");
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
    const auto found=project_.textures.find(texture);
    if(texture!=uvImageKey_){
        uvImageKey_=texture;cachedUvImage_=QImage();
        if(found!=project_.textures.end())
            cachedUvImage_=QImage::fromData(QByteArray::fromBase64(QByteArray::fromStdString(found->second)),"PNG");
        uvCanvas_->setImage(cachedUvImage_);
    }
    textureInfo_->setText(cachedUvImage_.isNull()?"No embedded PNG for this face. Viewport shows procedural material texture."
        :QString("PNG preview: %1 x %2 px  ·  %3").arg(cachedUvImage_.width()).arg(cachedUvImage_.height()).arg(q(texture)));
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
    auto* textures=menuBar()->addMenu("&Textures");
    textures->addAction("Import PNG...",this,&MainWindow::importPngTexture);
    textures->addAction("Apply selected texture to whole cube",this,&MainWindow::applyTextureToCube);
    auto* help=menuBar()->addMenu("&Help");
    help->addAction("About Magneticraft2 Studio",this,[this](){
        QMessageBox::about(this,"Magneticraft2 Studio", "<b>Magneticraft2 Studio — Native Linux Model Workspace v0.4</b><br>"
            "C++17 + Qt 6 + OpenGL 3.3 Core<br><br>"
            "Dedicated 3D model workspace with direct cube drawing, grid snapping, views and Studio v1 save/load.<br>"
            "<b>Not yet:</b> animated mechanical rigs, full Blockbench editing parity or mod-side tiled rendering.");
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
    if(textureDock_)textureDock_->setVisible(isModel);
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
    uvImageKey_.clear();viewport_->invalidateTextures();
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
    uvImageKey_.clear();viewport_->invalidateTextures();
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
