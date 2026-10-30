#include "VisualRuntimeDialog.hpp"
#include "io/VisualContract.hpp"
#include "io/RigTransforms.hpp"
#include "ui/Viewport.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonArray>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTimer>
#include <QElapsedTimer>
#include <QSignalBlocker>
#include <QSettings>
#include <QDir>
#include <QVBoxLayout>

namespace studio {
namespace {
QString pretty(const QJsonObject& object){
    return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Indented));
}
QJsonObject starterManifest(){
    return {{"format","magneticraft2:machine_visual"},{"format_version",1},
        {"animation","magneticraft2:studio/primitive_grinder_custom"},
        {"angle_channels",QJsonObject{{"mechanical_input","mechanical_input"}}},
        {"integer_channels",QJsonObject{}}};
}
QJsonObject starterScene(){
    return {{"format","magneticraft2:visual_animation"},{"format_version",1},
      {"coordinate_system",QJsonObject{{"units","blocks"},{"origin","controller"},{"reference_facing","south"}}},
      {"components",QJsonObject{
         {"input_axle",QJsonObject{{"model","magneticraft2:multiblock/primitive_grinder_axle"},
            {"model_offset",QJsonArray{0,0,0}},{"pivot",QJsonArray{0,0.5,0.5}}}},
         {"grinding_rotor",QJsonObject{{"model","magneticraft2:multiblock/primitive_grinder_rotor"},
            {"model_offset",QJsonArray{0,0,0}},{"pivot",QJsonArray{0.5,0,0.5}}}}}},
      {"inputs",QJsonObject{{"drive_angle",QJsonObject{{"source","mechanical.visual_angle_degrees"},
                           {"module","mechanical_input"}}}}},
      {"animations",QJsonObject{{"gear_drive",QJsonObject{
          {"type","driven"},{"tracks",QJsonArray{
            QJsonObject{{"target","input_axle"},{"property","rotation"},{"axis","x"},{"input","drive_angle"},{"scale",1}},
            QJsonObject{{"target","grinding_rotor"},{"property","rotation"},{"axis","y"},{"input","drive_angle"},{"scale",-1}}
          }}}}}}};
}
bool loadObject(QWidget* parent,QPlainTextEdit* destination,const QString& caption){
    const QString path=QFileDialog::getOpenFileName(parent,caption,{},"JSON (*.json)");
    if(path.isEmpty())return false;
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly)){QMessageBox::warning(parent,"Cannot import",file.errorString());return false;}
    QJsonObject obj;QString error;
    if(!VisualContract::parseObject(file.readAll(),obj,error)){
        QMessageBox::warning(parent,"Invalid JSON",error);return false;
    }
    destination->setPlainText(pretty(obj));return true;
}
}
VisualRuntimeDialog::VisualRuntimeDialog(const QJsonObject& existing,QWidget* parent):QDialog(parent){
    setWindowTitle("MGC2 Studio — JSON Machine Visual Editor (runtime v1)");
    resize(1280,850);
    auto* layout=new QVBoxLayout(this);
    auto* top=new QHBoxLayout;
    top->addWidget(new QLabel("Java machine",this));
    machine_=new QComboBox(this);machine_->setObjectName("VisualMachine");
    machine_->addItems({"bronze_mechanical_lathe","mechanical_sifter","primitive_grinder",
       "mechanical_ore_washer","bronze_stamp_mill","rotary_ore_classifier"});
    machine_->setCurrentText(existing.value("machine").toString("primitive_grinder"));
    top->addWidget(machine_);
    auto* sources=new QLabel(this);
    sources->setObjectName("VisualSources");sources->setWordWrap(true);
    top->addWidget(sources,1);
    layout->addLayout(top);
    auto* editors=new QSplitter(Qt::Horizontal,this);
    auto* manifestBox=new QWidget(editors);auto* ml=new QVBoxLayout(manifestBox);
    ml->addWidget(new QLabel("machine_visuals/<machine>.json — physical input aliases"));
    manifest_=new QPlainTextEdit(manifestBox);manifest_->setObjectName("VisualManifestJSON");
    manifest_->setTabStopDistance(22);
    manifest_->setPlainText(pretty(existing.value("manifest").isObject()
      ?existing.value("manifest").toObject():starterManifest()));
    ml->addWidget(manifest_,1);
    auto* importManifestButton=new QPushButton("Import machine manifest JSON...",manifestBox);
    ml->addWidget(importManifestButton);
    editors->addWidget(manifestBox);
    auto* sceneBox=new QWidget(editors);auto* sl=new QVBoxLayout(sceneBox);
    sl->addWidget(new QLabel("animations/<scene-id>.json — complete scene graph"));
    scene_=new QPlainTextEdit(sceneBox);scene_->setObjectName("VisualSceneJSON");
    scene_->setTabStopDistance(22);
    scene_->setPlainText(pretty(existing.value("scene").isObject()
      ?existing.value("scene").toObject():starterScene()));
    sl->addWidget(scene_,1);
    auto* importSceneButton=new QPushButton("Import animation scene JSON...",sceneBox);
    sl->addWidget(importSceneButton);
    editors->addWidget(sceneBox);
    editors->setStretchFactor(0,1);editors->setStretchFactor(1,2);
    layout->addWidget(editors,3);
    auto* previewPanel=new QGroupBox("Deterministic runtime pose inspection — physical shaft angle",this);
    auto* pl=new QVBoxLayout(previewPanel);
    auto* controls=new QHBoxLayout;
    angleSource_=new QComboBox(previewPanel);
    angleSource_->setObjectName("VisualPhysicalSource");
    controls->addWidget(new QLabel("Java shaft",previewPanel));
    controls->addWidget(angleSource_);
    shaftAngle_=new QDoubleSpinBox(previewPanel);
    shaftAngle_->setObjectName("VisualPhysicalAngle");
    shaftAngle_->setDecimals(2);shaftAngle_->setRange(-10000000,10000000);
    shaftAngle_->setSingleStep(15);shaftAngle_->setSuffix(" deg");
    controls->addWidget(shaftAngle_);
    playPause_=new QPushButton("Play",previewPanel);
    playPause_->setObjectName("VisualPlayPause");
    controls->addWidget(playPause_);
    playbackSpeed_=new QDoubleSpinBox(previewPanel);
    playbackSpeed_->setObjectName("VisualPlaybackSpeed");
    playbackSpeed_->setDecimals(1);
    playbackSpeed_->setRange(-3600,3600);playbackSpeed_->setValue(90);
    playbackSpeed_->setSuffix(" deg/s");
    playbackSpeed_->setToolTip("Preview-only playback rate. Never exported as Gear V2 speed or RPM.");
    controls->addWidget(playbackSpeed_);
    running_=new QCheckBox("Running",previewPanel);running_->setChecked(true);
    overloaded_=new QCheckBox("Overloaded",previewPanel);
    worn_=new QCheckBox("Worn",previewPanel);
    controls->addWidget(running_);controls->addWidget(overloaded_);controls->addWidget(worn_);
    controls->addWidget(new QLabel("Wear stage",previewPanel));
    wearStage_=new QSpinBox(previewPanel);wearStage_->setRange(0,5);
    controls->addWidget(wearStage_);controls->addStretch();
    pl->addLayout(controls);

    auto* assetRow=new QHBoxLayout;
    auto* modelsButton=new QPushButton("Load actual mod model assets...",previewPanel);
    modelsButton->setObjectName("VisualLoadAssets");
    modelsButton->setToolTip("Choose the local mod's src/main/resources directory, or an unpacked resource pack with assets/");
    assetRow->addWidget(modelsButton);
    auto* texturesButton=new QPushButton("Add missing PNGs from Minecraft JAR...",previewPanel);
    texturesButton->setObjectName("VisualAddTextures");
    assetRow->addWidget(texturesButton);
    rigAssetStatus_=new QLabel("3D geometry is not loaded. Choose a local Minecraft assets folder; no placeholder meshes are generated.",previewPanel);
    rigAssetStatus_->setObjectName("VisualAssetStatus");
    rigAssetStatus_->setWordWrap(true);
    assetRow->addWidget(rigAssetStatus_,1);
    pl->addLayout(assetRow);

    auto* rigScope=new QLabel("ANIMATED COMPONENTS ONLY  /  Java-drawn static machine body, water, items and other effects are not imported.",previewPanel);
    rigScope->setObjectName("ContextInfo");
    rigScope->setWordWrap(true);
    pl->addWidget(rigScope);
    auto* views=new QSplitter(Qt::Horizontal,previewPanel);
    rigViewport_=new Viewport(views);
    rigViewport_->setObjectName("VisualRigViewport");
    rigViewport_->setMinimumSize(420,240);
    {
        Project empty=makeProject("Visual rig preview",{5,5,5});
        empty.parts.clear();empty.cells.clear();empty.textures.clear();
        BuildOptions opts;opts.showFormed=false;opts.showConstruction=false;opts.showGrid=true;
        rigViewport_->setScene(empty,opts);
        rigViewport_->resetCamera();
    }
    views->addWidget(rigViewport_);
    preview_=new QPlainTextEdit(views);
    preview_->setObjectName("VisualPoseSample");
    preview_->setReadOnly(true);
    preview_->setMinimumWidth(220);
    preview_->setToolTip("Numeric pose data for diagnosing pivots, states, slider-crank closure and belt surface travel");
    views->addWidget(preview_);
    views->setStretchFactor(0,3);views->setStretchFactor(1,2);
    pl->addWidget(views,1);
    layout->addWidget(previewPanel,5);
    playbackTimer_=new QTimer(this);
    playbackTimer_->setInterval(33);
    connect(modelsButton,&QPushButton::clicked,this,&VisualRuntimeDialog::loadRigAssets);
    connect(texturesButton,&QPushButton::clicked,this,&VisualRuntimeDialog::loadExtraTextures);
    connect(playPause_,&QPushButton::clicked,this,[this](){
        if(playbackTimer_->isActive()){playbackTimer_->stop();playPause_->setText("Play");}
        else{playbackTimer_->start();playPause_->setText("Pause");}
    });
    connect(playbackTimer_,&QTimer::timeout,this,&VisualRuntimeDialog::advancePlayback);
    status_=new QLabel(this);status_->setWordWrap(true);layout->addWidget(status_);
    auto* buttons=new QHBoxLayout;
    auto* validateButton=new QPushButton("Validate against Java v1",this);
    auto* exportButton=new QPushButton("Export two JSON resources...",this);
    auto* closeButton=new QPushButton("Close",this);
    auto* acceptButton=new QPushButton("Save into Studio project",this);
    acceptButton->setObjectName("VisualSaveProject");
    buttons->addWidget(validateButton);buttons->addWidget(exportButton);
    buttons->addStretch();buttons->addWidget(closeButton);buttons->addWidget(acceptButton);
    layout->addLayout(buttons);

    connect(importManifestButton,&QPushButton::clicked,this,&VisualRuntimeDialog::importManifest);
    connect(importSceneButton,&QPushButton::clicked,this,&VisualRuntimeDialog::importScene);
    connect(validateButton,&QPushButton::clicked,this,&VisualRuntimeDialog::validate);
    connect(exportButton,&QPushButton::clicked,this,&VisualRuntimeDialog::exportFiles);
    connect(acceptButton,&QPushButton::clicked,this,[this](){
        QJsonObject m,s;QString error;QStringList warnings;
        if(!parsePair(m,s,error)){QMessageBox::warning(this,"Cannot save invalid scene",error);return;}
        accept();
    });
    connect(closeButton,&QPushButton::clicked,this,&VisualRuntimeDialog::reject);
    auto changed=[this](){valid_=false;sample();};
    connect(manifest_,&QPlainTextEdit::textChanged,this,changed);
    connect(scene_,&QPlainTextEdit::textChanged,this,changed);
    connect(machine_,&QComboBox::currentTextChanged,this,[this,sources](const QString& machine){
        syncAngleSource();
        sources->setText("Java angles: "+VisualContract::availableAngles(machine).join(", ")+
          "    |    Java integers: "+(VisualContract::availableIntegers(machine).isEmpty()
             ?QString("none"):VisualContract::availableIntegers(machine).join(", ")));
        sample();
    });
    connect(angleSource_,&QComboBox::currentTextChanged,this,[this](const QString& source){
        QSignalBlocker block(shaftAngle_);
        shaftAngle_->setValue(previewAngles_.value(source,0));
        sample();
    });
    connect(shaftAngle_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double value){
        previewAngles_[angleSource_->currentText()]=value;sample();
    });
    connect(wearStage_,QOverload<int>::of(&QSpinBox::valueChanged),this,[this](int){sample();});
    for(auto* c:{running_,overloaded_,worn_})
        connect(c,&QCheckBox::toggled,this,[this](bool){sample();});
    sources->setText("Java angles: "+VisualContract::availableAngles(machine_->currentText()).join(", ")+
      "    |    Java integers: "+(VisualContract::availableIntegers(machine_->currentText()).isEmpty()
          ?QString("none"):VisualContract::availableIntegers(machine_->currentText()).join(", ")));
    syncAngleSource();
    validate();
}
QJsonObject VisualRuntimeDialog::metadata() const{
    QJsonObject manifest,scene;QString error;
    if(!VisualContract::parseObject(manifest_->toPlainText().toUtf8(),manifest,error)||
       !VisualContract::parseObject(scene_->toPlainText().toUtf8(),scene,error))
        return {};
    return {{"machine",machine_->currentText()},{"manifest",manifest},{"scene",scene}};
}
bool VisualRuntimeDialog::parsePair(QJsonObject& m,QJsonObject& s,QString& error){
    QStringList warnings;
    return VisualContract::importPair(machine_->currentText(),
        manifest_->toPlainText().toUtf8(),scene_->toPlainText().toUtf8(),m,s,error,&warnings);
}
void VisualRuntimeDialog::importManifest(){if(loadObject(this,manifest_,"Import machine_visuals JSON"))validate();}
void VisualRuntimeDialog::importScene(){if(loadObject(this,scene_,"Import animation scene JSON"))validate();}
void VisualRuntimeDialog::validate(){
    QJsonObject m,s;QString error;QStringList warnings;
    valid_=VisualContract::importPair(machine_->currentText(),manifest_->toPlainText().toUtf8(),
        scene_->toPlainText().toUtf8(),m,s,error,&warnings);
    status_->setText(valid_?"Runtime v1 schema accepted. "+warnings.join(" • "):"INVALID: "+error);
    status_->setStyleSheet(valid_?"color:#8cd6a1;":"color:#ffae86;");
    sample();
}
void VisualRuntimeDialog::sample(){
    QJsonObject m,s;QString error;
    if(!parsePair(m,s,error)){
        preview_->setPlainText("Cannot inspect invalid scene:\n"+error);
        rigViewport_->setRigPose(RigPose{});return;
    }
    m["_studio_machine"]=machine_->currentText();
    QMap<QString,double> inputs;
    for(const auto& source:VisualContract::availableAngles(machine_->currentText()))
        inputs[source]=previewAngles_.value(source,0.0);
    const auto pose=VisualContract::sample(m,s,inputs,running_->isChecked(),
        overloaded_->isChecked(),worn_->isChecked(),wearStage_->value(),error);
    if(!error.isEmpty()){preview_->setPlainText(error);rigViewport_->clearRig();return;}
    preview_->setPlainText(pretty(pose));
    RigPose transforms;
    if(!RigTransforms::evaluate(machine_->currentText(),m,s,inputs,running_->isChecked(),
           overloaded_->isChecked(),worn_->isChecked(),wearStage_->value(),transforms,error)){
        rigAssetStatus_->setText("Animation pose cannot be evaluated: "+error);return;
    }
    rigViewport_->setRigPose(transforms);
}

void VisualRuntimeDialog::syncAngleSource(){
    const QString previous=angleSource_->currentText();
    const auto sources=VisualContract::availableAngles(machine_->currentText());
    QSignalBlocker blocker(angleSource_);
    angleSource_->clear();angleSource_->addItems(sources);
    if(sources.contains(previous))angleSource_->setCurrentText(previous);
    if(!angleSource_->currentText().isEmpty()){
        QSignalBlocker angleBlocker(shaftAngle_);
        shaftAngle_->setValue(previewAngles_.value(angleSource_->currentText(),0));
    }
}
void VisualRuntimeDialog::advancePlayback(){
    const QString source=angleSource_->currentText();
    if(source.isEmpty())return;
    // Explicit preview-only signed speed; neither synchronized nor written to
    // JSON as physical machine RPM. A bounded fixed timer avoids extra GL data.
    const double degrees=previewAngles_.value(source,0)+playbackSpeed_->value()/30.0;
    const double wrapped=degrees>9999999?-9999999:degrees<-9999999?9999999:degrees;
    previewAngles_[source]=wrapped;
    {
        QSignalBlocker blocker(shaftAngle_);
        shaftAngle_->setValue(wrapped);
    }
    sample();
}
void VisualRuntimeDialog::loadRigAssets(){
    QSettings settings;
    const QString folder=QFileDialog::getExistingDirectory(this,
       "Choose mod resources root containing assets/ (e.g. src/main/resources)",
       settings.value("animation/asset-root").toString());
    if(folder.isEmpty())return;
    QString error;
    if(!loadAssetsFromDirectory(folder,error)){
        QMessageBox::warning(this,"Cannot load scene models",error);return;
    }
    settings.setValue("animation/asset-root",folder);
    const QString message=rigAssetStatus_->text();
    if(!rigAssets_.warnings.isEmpty()||!rigAssets_.missingModels.isEmpty()){
        QMessageBox::information(this,"Rig preview dependencies",
            message+"\n\n"+(rigAssets_.missingModels+rigAssets_.warnings).mid(0,12).join("\n"));
    }
}

bool VisualRuntimeDialog::loadAssetsFromDirectory(const QString& folder,QString& error){
    QJsonObject manifest,scene;
    if(!parsePair(manifest,scene,error))return false;
    RigAssets loaded;
    if(!RigAssetLoader::load(folder,scene,loaded,machine_->currentText())){
        error=loaded.error;return false;
    }
    rigAssets_=std::move(loaded);
    rigAssetsLoaded_=true;
    rigViewport_->setRigModels(rigAssets_.models,rigAssets_.textureProject.textures,
                              rigAssets_.beltRepeats);
    sample();
    rigViewport_->frameRig();
    rigAssetStatus_->setText(QString("Real models: %1 loaded, %2 unresolved; PNGs: %3 missing.")
        .arg(rigAssets_.loadedModels.size()).arg(rigAssets_.missingModels.size())
        .arg(rigAssets_.missingTextures.size()));
    rigAssetStatus_->setToolTip(
        (rigAssets_.missingModels+rigAssets_.missingTextures+rigAssets_.warnings).join("\n"));
    error.clear();return true;
}
void VisualRuntimeDialog::loadExtraTextures(){
    if(!rigAssetsLoaded_){QMessageBox::information(this,"Load models first",
        "Load the actual local model assets so Studio knows which PNGs are referenced.");return;}
    const QString source=QFileDialog::getOpenFileName(this,
       "Select local Minecraft client JAR or resource pack",
       {}, "Minecraft client / resource pack (*.jar *.zip)");
    if(source.isEmpty())return;
    const auto report=RigAssetLoader::addTextures(rigAssets_,source);
    if(!report.error.isEmpty()){
        QMessageBox::warning(this,"Cannot import PNGs",report.error);return;
    }
    rigViewport_->setRigModels(rigAssets_.models,rigAssets_.textureProject.textures);
    sample();
    rigAssetStatus_->setText(QString("Real models: %1 loaded, %2 unresolved; PNGs: %3 missing.")
        .arg(rigAssets_.loadedModels.size()).arg(rigAssets_.missingModels.size())
        .arg(rigAssets_.missingTextures.size()));
}
void VisualRuntimeDialog::exportFiles(){
    QJsonObject m,s;QString error;QStringList warnings;
    if(!VisualContract::importPair(machine_->currentText(),manifest_->toPlainText().toUtf8(),
           scene_->toPlainText().toUtf8(),m,s,error,&warnings)){
        QMessageBox::warning(this,"Export blocked",error);return;
    }
    const QString dir=QFileDialog::getExistingDirectory(this,"Choose resource-pack root directory");
    if(dir.isEmpty())return;
    if(!VisualContract::exportPair(dir,machine_->currentText(),m,s,error,&warnings)){
        QMessageBox::warning(this,"Export failed",error);return;
    }
    QMessageBox::information(this,"Two runtime JSON resources exported",
      "Machine manifest and animation scene exported using the exact runtime directory layout.\n\n"
      "This is NOT yet a complete Minecraft resource pack. Referenced baked models and PNGs "
      "are not automatically exported. Please resolve those dependencies before testing in Minecraft.\n\n"+
      warnings.join("\n"));
}
} // namespace studio
