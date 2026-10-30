#include "AnimationWorkspace.hpp"
#include "Viewport.hpp"
#include "io/RigAssetCache.hpp"
#include "io/RigTransforms.hpp"
#include "io/VisualContract.hpp"
#include "io/VisualTemplates.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QMatrix4x4>
#include <QVector3D>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

namespace studio {
namespace {
QDoubleSpinBox* spin(QWidget* parent,double low,double high,double step,int decimals=3){
    auto* s=new QDoubleSpinBox(parent);
    s->setRange(low,high);s->setSingleStep(step);s->setDecimals(decimals);
    s->setKeyboardTracking(false);
    return s;
}
QJsonArray point3(QDoubleSpinBox* const v[3]){
    return {v[0]->value(),v[1]->value(),v[2]->value()};
}
QString selectedTrack(const QComboBox* box){return box->currentData().toString();}
}

AnimationWorkspace::AnimationWorkspace(Viewport* viewport,QWidget* parent)
    :QWidget(parent),viewport_(viewport){
    setObjectName("AnimationWorkspacePanel");
    setMinimumHeight(205);
    auto* main=new QVBoxLayout(this);
    main->setContentsMargins(12,8,12,8);
    main->setSpacing(6);

    auto* heading=new QHBoxLayout;
    auto* title=new QLabel("ANIMATE  /  SHARED MACHINE",this);
    title->setObjectName("ContextTitle");
    heading->addWidget(title);
    heading->addWidget(new QLabel("Machine",this));
    machine_=new QComboBox(this);
    machine_->setObjectName("AnimationMachine");
    machine_->addItems(VisualTemplates::machines());
    heading->addWidget(machine_,2);
    heading->addWidget(new QLabel("Component",this));
    component_=new QComboBox(this);
    component_->setObjectName("AnimationComponent");
    component_->setMinimumWidth(140);
    heading->addWidget(component_,2);
    heading->addStretch();
    main->addLayout(heading);
    auto* binding=new QHBoxLayout;
    selectedPartStatus_=new QLabel("Select a model cuboid in the Outliner to assign it to a joint.",this);
    selectedPartStatus_->setObjectName("AnimationSelectedModelPart");
    binding->addWidget(selectedPartStatus_,1);
    bindPartButton_=new QPushButton("Bind selected cuboid to joint",this);
    bindPartButton_->setObjectName("AnimationBindPart");
    binding->addWidget(bindPartButton_);
    unbindPartButton_=new QPushButton("Unbind",this);
    unbindPartButton_->setObjectName("AnimationUnbindPart");
    binding->addWidget(unbindPartButton_);
    bindPartButton_->setEnabled(false);
    unbindPartButton_->setEnabled(false);
    main->addLayout(binding);

    auto* columns=new QHBoxLayout;
    columns->setSpacing(12);

    auto* preview=new QGroupBox("MECHANICAL PREVIEW  /  EDITOR ONLY",this);
    auto* pl=new QVBoxLayout(preview);
    auto* angles=new QHBoxLayout;
    previewSource_=new QComboBox(preview);
    previewSource_->setObjectName("AnimationPhysicalSource");
    angles->addWidget(previewSource_,2);
    previewAngle_=spin(preview,-10000000,10000000,15,2);
    previewAngle_->setObjectName("AnimationPhysicalAngle");
    previewAngle_->setSuffix(" deg");
    angles->addWidget(previewAngle_,1);
    play_=new QPushButton("Play",preview);
    play_->setObjectName("AnimationPlay");
    angles->addWidget(play_);
    pl->addLayout(angles);
    auto* playback=new QHBoxLayout;
    playback->addWidget(new QLabel("Preview speed",preview));
    previewSpeed_=spin(preview,-3600,3600,15,1);
    previewSpeed_->setObjectName("AnimationPreviewSpeed");
    previewSpeed_->setValue(90);
    previewSpeed_->setSuffix(" deg/s");
    previewSpeed_->setToolTip("Preview speed only; actual Gear V2 RPM is supplied by Minecraft.");
    playback->addWidget(previewSpeed_);
    playback->addStretch();
    pl->addLayout(playback);
    auto* states=new QHBoxLayout;
    running_=new QCheckBox("Running",preview);running_->setChecked(true);
    overloaded_=new QCheckBox("Overloaded",preview);
    worn_=new QCheckBox("Worn",preview);
    states->addWidget(running_);states->addWidget(overloaded_);states->addWidget(worn_);
    states->addWidget(new QLabel("Wear",preview));
    wearStage_=new QSpinBox(preview);wearStage_->setRange(0,5);
    wearStage_->setObjectName("AnimationWearStage");
    states->addWidget(wearStage_);
    states->addStretch();
    pl->addLayout(states);
    columns->addWidget(preview,1);

    auto* motion=new QGroupBox("ROTATION  /  LOCAL JOINT",this);
    auto* ml=new QVBoxLayout(motion);
    auto* movement=new QHBoxLayout;
    track_=new QComboBox(motion);track_->setObjectName("AnimationRotationTrack");
    track_->setMinimumWidth(105);movement->addWidget(track_,2);
    driver_=new QComboBox(motion);driver_->setObjectName("AnimationRotationInput");
    driver_->setToolTip("Scene input alias bound to an actual Java mechanical source");
    movement->addWidget(driver_,1);
    axis_=new QComboBox(motion);axis_->setObjectName("AnimationRotationAxis");
    axis_->addItems({"x","-x","y","-y","z","-z"});
    movement->addWidget(axis_);
    ml->addLayout(movement);
    auto* amounts=new QHBoxLayout;
    amounts->addWidget(new QLabel("Ratio",motion));
    scale_=spin(motion,-128,128,.25,3);
    scale_->setObjectName("AnimationRotationScale");
    scale_->setValue(1);amounts->addWidget(scale_);
    amounts->addWidget(new QLabel("Phase",motion));
    phase_=spin(motion,-3600,3600,15,2);
    phase_->setObjectName("AnimationRotationPhase");
    phase_->setSuffix(" deg");
    amounts->addWidget(phase_);
    when_=new QComboBox(motion);
    when_->setObjectName("AnimationRotationCondition");
    when_->addItems({"always","running","stopped","overloaded","worn"});
    amounts->addWidget(when_);
    ml->addLayout(amounts);
    auto* applyRotation=new QPushButton("Add / update rotation",motion);
    applyRotation->setObjectName("AnimationApplyRotation");
    ml->addWidget(applyRotation);
    columns->addWidget(motion,1);

    auto* pivotBox=new QGroupBox("PIVOT  /  CONTROLLER BLOCK UNITS",this);
    auto* pivotLayout=new QVBoxLayout(pivotBox);
    auto* pivots=new QHBoxLayout;
    for(int i=0;i<3;i++){
        pivots->addWidget(new QLabel(QString("XYZ").mid(i,1),pivotBox));
        pivot_[i]=spin(pivotBox,-256,256,1.0/16.0,4);
        pivot_[i]->setObjectName(QString("AnimationPivot%1").arg(QString("XYZ").mid(i,1)));
        pivots->addWidget(pivot_[i]);
    }
    pivotLayout->addLayout(pivots);
    pivotFromPart_=new QPushButton("Use selected cuboid's pivot",pivotBox);
    pivotFromPart_->setObjectName("AnimationUsePartPivot");
    pivotFromPart_->setEnabled(false);
    pivotLayout->addWidget(pivotFromPart_);
    auto* savePivot=new QPushButton("Apply pivot",pivotBox);
    savePivot->setObjectName("AnimationApplyPivot");
    pivotLayout->addWidget(savePivot);
    auto* frame=new QPushButton("Frame animated rig",pivotBox);
    frame->setObjectName("AnimationFrameRig");
    pivotLayout->addWidget(frame);
    columns->addWidget(pivotBox,1);
    main->addLayout(columns,1);

    auto* assetsRow=new QHBoxLayout;
    auto* selectMod=new QPushButton("Choose mod JAR...",this);
    selectMod->setObjectName("AnimationChooseJar");
    assetsRow->addWidget(selectMod);
    auto* selectFolder=new QPushButton("Assets folder...",this);
    assetsRow->addWidget(selectFolder);
    auto* selectVanilla=new QPushButton("Minecraft textures...",this);
    assetsRow->addWidget(selectVanilla);
    auto* reload=new QPushButton("Reload assets",this);
    reload->setObjectName("AnimationReloadAssets");
    assetsRow->addWidget(reload);
    assetStatus_=new QLabel("Choose an installed Magneticraft2 JAR once to resolve machine geometry.",this);
    assetStatus_->setObjectName("AnimationAssetStatus");
    assetStatus_->setWordWrap(true);
    assetsRow->addWidget(assetStatus_,1);
    main->addLayout(assetsRow);
    status_=new QLabel("Select a machine and component to configure a rotation.",this);
    status_->setObjectName("AnimationValidationStatus");
    status_->setWordWrap(true);
    main->addWidget(status_);

    timer_=new QTimer(this);timer_->setInterval(33);
    connect(timer_,&QTimer::timeout,this,&AnimationWorkspace::tick);
    connect(play_,&QPushButton::clicked,this,[this](){
        if(timer_->isActive())stopPlayback();
        else{clock_.restart();timer_->start();play_->setText("Pause");}
    });
    connect(machine_,&QComboBox::currentTextChanged,this,[this](const QString& id){
        if(!settingUi_)changeMachine(id);
    });
    connect(component_,&QComboBox::currentTextChanged,this,[this](const QString&){
        if(!settingUi_)refreshTrackControls();
    });
    connect(track_,&QComboBox::currentIndexChanged,this,[this](int){
        if(!settingUi_)refreshTrackControls();
    });
    connect(previewSource_,&QComboBox::currentTextChanged,this,[this](const QString& source){
        if(settingUi_)return;
        QSignalBlocker blocker(previewAngle_);
        previewAngle_->setValue(previewAngles_.value(source));
        inspectPose();
    });
    connect(previewAngle_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this,[this](double degrees){
        if(settingUi_)return;
        previewAngles_[previewSource_->currentText()]=degrees;
        inspectPose();
    });
    connect(bindPartButton_,&QPushButton::clicked,this,[this](){
        if(!selectedPartId_.isEmpty()&&!component_->currentText().isEmpty())
            emit bindPartRequested(selectedPartId_,component_->currentText());
    });
    connect(unbindPartButton_,&QPushButton::clicked,this,[this](){
        if(!selectedPartId_.isEmpty())emit bindPartRequested(selectedPartId_,{});
    });
    connect(applyRotation,&QPushButton::clicked,this,&AnimationWorkspace::changeRotation);
    connect(pivotFromPart_,&QPushButton::clicked,this,[this](){
        if(selectedPartId_.isEmpty())return;
        const double coords[]{selectedPartPivot_.x,selectedPartPivot_.y,selectedPartPivot_.z};
        for(int i=0;i<3;i++)pivot_[i]->setValue(coords[i]);
        changePivot(); // Explicit user choice, never overwrite Java pivots silently.
    });
    connect(savePivot,&QPushButton::clicked,this,&AnimationWorkspace::changePivot);
    connect(frame,&QPushButton::clicked,viewport_,&Viewport::frameRig);
    for(auto* state:{running_,overloaded_,worn_})
        connect(state,&QCheckBox::toggled,this,[this](bool){inspectPose();});
    connect(wearStage_,QOverload<int>::of(&QSpinBox::valueChanged),
            this,[this](int){inspectPose();});
    connect(selectMod,&QPushButton::clicked,this,&AnimationWorkspace::chooseModJar);
    connect(selectFolder,&QPushButton::clicked,this,&AnimationWorkspace::chooseAssetFolder);
    connect(selectVanilla,&QPushButton::clicked,this,&AnimationWorkspace::chooseVanillaJar);
    connect(reload,&QPushButton::clicked,this,[this](){refreshAssets(true);});
    setEnabled(false);
}

QString AnimationWorkspace::detectMachine(const QString& title) const{
    const QString n=title.toLower();
    if(n.contains("ore washer"))return "mechanical_ore_washer";
    if(n.contains("sifter"))return "mechanical_sifter";
    if(n.contains("stamp mill"))return "bronze_stamp_mill";
    if(n.contains("lathe"))return "bronze_mechanical_lathe";
    if(n.contains("classifier"))return "rotary_ore_classifier";
    if(n.contains("grinder"))return "primitive_grinder";
    return "primitive_grinder";
}
void AnimationWorkspace::setProject(const Project& p,const QJsonObject& saved){
    stopPlayback();
    kind_=p.kind;
    authoredProject_=p;
    authoredMeshes_.clear();
    selectedPartId_.clear();
    selectedPartStatus_->setText("Select a model cuboid in the Outliner to assign it to a joint.");
    bindPartButton_->setEnabled(false);
    unbindPartButton_->setEnabled(false);
    pivotFromPart_->setEnabled(false);
    viewport_->clearRig();
    assets_.reset();
    previewAngles_.clear();
    hasSavedRuntime_=saved.value("manifest").isObject() && saved.value("scene").isObject();
    QString machine=saved.value("machine").toString();
    if(!VisualTemplates::machines().contains(machine))
        machine=detectMachine(QString::fromStdString(p.name));
    {
        QSignalBlocker blocker(machine_);
        machine_->setCurrentText(machine);
    }
    if(hasSavedRuntime_){
        manifest_=saved.value("manifest").toObject();
        scene_=saved.value("scene").toObject();
    }else{
        QString error;
        if(!VisualTemplates::bundled(machine,manifest_,scene_,error))
            status_->setText(error);
    }
    QSettings settings;
    acceptedMachine_=machine;
    const QString projectSource=saved.value("assetSource").toString();
    const QString globalJar=settings.value("animation/mod-jar").toString();
    const QString globalFolder=settings.value("animation/asset-root").toString();
    sourcePath_=QFileInfo::exists(projectSource)?projectSource:
        QFileInfo::exists(globalJar)?globalJar:
        QFileInfo::exists(globalFolder)?globalFolder:QString();
    vanillaPath_=settings.value("animation/minecraft-jar").toString();
    setEnabled(kind_==ProjectKind::Multiblock);
    refreshComponents();refreshInputs();
    if(kind_!=ProjectKind::Multiblock)
        status_->setText("Only Magneticraft2 multiblock projects can use the Forge visual animation runtime.");
    if(active_)refreshAssets();
}
void AnimationWorkspace::activate(bool visible){
    active_=visible && kind_==ProjectKind::Multiblock;
    viewport_->setRigVisible(active_);
    if(!active_){
        stopPlayback();
        return;
    }
    rebuildAuthoredGeometry();
    if(!assets_)refreshAssets();
    else publishRigModels();
    inspectPose();
}
QStringList AnimationWorkspace::boundComponentIds() const{
    QStringList ids;
    for(const auto& entry:authoredMeshes_)ids.append(QString::fromStdString(entry.first));
    return ids;
}
void AnimationWorkspace::setAuthoredProject(const Project& project){
    authoredProject_=project;
    if(active_)rebuildAuthoredGeometry();
}
void AnimationWorkspace::setSelectedModelPart(const QString& partId,const QString& partName,
                                             const QString& currentRig,Vec3 partPivot){
    selectedPartId_=partId;
    selectedPartPivot_=partPivot;
    pivotFromPart_->setEnabled(!partId.isEmpty());
    selectedPartStatus_->setText(partId.isEmpty()
        ?QString("Select a model cuboid in the Outliner to assign it to a joint.")
        :partName+(currentRig.isEmpty()?QString("  ·  not bound")
                                         :QString("  ·  bound to ")+currentRig));
    bindPartButton_->setEnabled(!partId.isEmpty());
    unbindPartButton_->setEnabled(!partId.isEmpty()&&!currentRig.isEmpty());
}
QStringList AnimationWorkspace::componentIds() const{
    return scene_.value("components").toObject().keys();
}
QString AnimationWorkspace::selectedComponentId() const{
    return component_?component_->currentText():QString();
}
void AnimationWorkspace::selectComponent(const QString& id){
    const int i=component_->findText(id);
    if(i>=0)component_->setCurrentIndex(i);
}
QJsonObject AnimationWorkspace::metadata() const{
    QJsonObject out{{"machine",machine_->currentText()},
                    {"manifest",manifest_},{"scene",scene_}};
    if(!sourcePath_.isEmpty())out["assetSource"]=sourcePath_;
    return out;
}
void AnimationWorkspace::refreshComponents(){
    const QString prior=component_->currentText();
    {
        QSignalBlocker blocker(component_);
        component_->clear();
        for(const QString& id:componentIds())component_->addItem(id);
        if(component_->findText(prior)>=0)component_->setCurrentText(prior);
    }
    refreshTrackControls();
    emit componentsChanged();
}
void AnimationWorkspace::refreshInputs(){
    const QString physical=previewSource_->currentText();
    const QString driver=driver_->currentText();
    {
        QSignalBlocker blockA(previewSource_),blockD(driver_);
        previewSource_->clear();
        previewSource_->addItems(VisualContract::availableAngles(machine_->currentText()));
        if(previewSource_->findText(physical)>=0)previewSource_->setCurrentText(physical);
        driver_->clear();
        const auto inputs=scene_.value("inputs").toObject();
        for(auto it=inputs.begin();it!=inputs.end();++it)
            driver_->addItem(it.key(),it.key());
        if(driver_->findText(driver)>=0)driver_->setCurrentText(driver);
    }
    {
        QSignalBlocker blocker(previewAngle_);
        previewAngle_->setValue(previewAngles_.value(previewSource_->currentText()));
    }
    refreshTrackControls();
}
void AnimationWorkspace::refreshTrackControls(){
    if(settingUi_)return;
    settingUi_=true;
    const QString target=component_->currentText();
    const QString previous=selectedTrack(track_);
    const auto tracks=scene_.value("animations").toObject();
    {
        QSignalBlocker blocker(track_);
        track_->clear();
        track_->addItem("New rotation",QString());
        for(auto it=tracks.begin();it!=tracks.end();++it){
            const auto array=it.value().toObject().value("tracks").toArray();
            for(int i=0;i<array.size();i++){
                const auto track=array[i].toObject();
                if(track.value("target")!=target || track.value("property")!="rotation")continue;
                const QString reference=it.key()+"#"+QString::number(i);
                track_->addItem(it.key()+" / "+track.value("when").toString("always"),reference);
            }
        }
        const int desired=track_->findData(previous);
        track_->setCurrentIndex(desired>=0?desired:track_->count()>1?1:0);
    }
    QJsonObject current;
    const QString selected=selectedTrack(track_);
    if(!selected.isEmpty()){
        const int split=selected.lastIndexOf('#');
        const QString animation=selected.left(split);
        const int index=selected.mid(split+1).toInt();
        const auto arr=tracks.value(animation).toObject().value("tracks").toArray();
        if(index>=0 && index<arr.size())current=arr[index].toObject();
    }
    const auto component=scene_.value("components").toObject().value(target).toObject();
    const auto pivot=component.value("pivot").toArray();
    for(int i=0;i<3;i++){
        QSignalBlocker blocker(pivot_[i]);
        pivot_[i]->setValue(pivot.size()==3?pivot[i].toDouble():0);
    }
    {
        QSignalBlocker a(axis_),b(scale_),c(phase_),d(when_),e(driver_);
        axis_->setCurrentText(current.value("axis").toString("x"));
        scale_->setValue(current.value("scale").toDouble(1));
        phase_->setValue(current.value("phase_degrees").toDouble());
        when_->setCurrentText(current.value("when").toString("always"));
        if(driver_->findText(current.value("input").toString())>=0)
            driver_->setCurrentText(current.value("input").toString());
    }
    settingUi_=false;
}
void AnimationWorkspace::inspectPose(){
    if(!active_ || kind_!=ProjectKind::Multiblock)return;
    QString error;
    RigPose pose;
    for(const auto& channel:VisualContract::availableAngles(machine_->currentText()))
        if(!previewAngles_.contains(channel))previewAngles_[channel]=0;
    if(!RigTransforms::evaluate(machine_->currentText(),manifest_,scene_,previewAngles_,
          running_->isChecked(),overloaded_->isChecked(),worn_->isChecked(),
          wearStage_->value(),pose,error)){
        status_->setText("INVALID ANIMATION: "+error);
        viewport_->setRigPose(RigPose{});
        return;
    }
    status_->setText(QString("%1 animated components  /  physical motion drives preview  /  editing project JSON v1")
                     .arg(pose.components.size()));
    for(auto& [id,component]:pose.components)
        if(authoredMeshes_.count(id))
            component.modelId="__studio_part/"+id;
    viewport_->setRigPose(pose);
}
void AnimationWorkspace::rebuildAuthoredGeometry(){
    authoredMeshes_.clear();
    if(kind_!=ProjectKind::Multiblock)return;
    QMap<QString,double> restAngles;
    for(const auto& name:VisualContract::availableAngles(machine_->currentText()))
        restAngles[name]=0.0;
    RigPose rest;
    QString error;
    if(!RigTransforms::evaluate(machine_->currentText(),manifest_,scene_,restAngles,
          true,false,false,0,rest,error)){
        emit message("Cannot map authored parts onto invalid runtime rig: "+error);
        return;
    }
    std::map<std::string,std::vector<Part>> groups;
    for(const auto& part:authoredProject_.parts){
        if(part.rigComponentId.empty())continue;
        if(rest.components.find(part.rigComponentId)==rest.components.end())continue;
        if(part.visible && validBounds(part.bounds))
            groups[part.rigComponentId].push_back(part);
    }
    BuildOptions options;
    options.showFormed=true;
    options.showConstruction=false;
    options.showGrid=false;
    options.selectedPart=-1;
    for(auto& [name,parts]:groups){
        Project subset=makeProject("Authored animation component",authoredProject_.dimensions);
        subset.parts=std::move(parts);
        subset.cells.clear();
        Geometry geometry=buildGeometry(subset,options);
        if(geometry.triangles.empty())continue;
        bool invertible=false;
        const auto inverse=rest.components.at(name).world.inverted(&invertible);
        if(!invertible){
            emit message("Cannot invert the rest transform for "+QString::fromStdString(name));
            continue;
        }
        // Java runtime joints use only rigid rotations/translations, so
        // inverse.mapVector gives the correct normal transform (no scaling).
        for(auto& vertex:geometry.triangles){
            const auto position=inverse.map(QVector3D(vertex.x,vertex.y,vertex.z));
            vertex.x=position.x();vertex.y=position.y();vertex.z=position.z();
            auto normal=inverse.mapVector(QVector3D(vertex.nx,vertex.ny,vertex.nz));
            if(normal.lengthSquared()>1e-6f)normal.normalize();
            vertex.nx=normal.x();vertex.ny=normal.y();vertex.nz=normal.z();
        }
        authoredMeshes_[name]=std::move(geometry);
    }
    publishRigModels();
    inspectPose();
}
void AnimationWorkspace::publishRigModels(){
    if(!active_)return;
    std::map<std::string,Geometry> models;
    std::map<std::string,std::string> textures;
    std::map<std::string,double> belts;
    if(assets_){
        models=assets_->models;
        textures=assets_->textureProject.textures;
        belts=assets_->beltRepeats;
    }
    for(const auto& [name,geometry]:authoredMeshes_)
        models["__studio_part/"+name]=geometry;
    viewport_->setRigModels(models,textures,belts);
}
void AnimationWorkspace::refreshAssets(bool force){
    if(!active_)return;
    if(sourcePath_.isEmpty() || !QFileInfo::exists(sourcePath_)){
        assets_.reset();
        publishRigModels(); // Authored Studio components still animate without a mod JAR.
        assetStatus_->setText("No mod JAR selected. Studio-authored bound cuboids can still animate.");
        inspectPose();
        return;
    }
    if(force)RigAssetCache::shared().invalidate(sourcePath_);
    QString error;
    auto loaded=RigAssetCache::shared().get(sourcePath_,machine_->currentText(),
                                          scene_,vanillaPath_,error);
    if(!loaded){
        assetStatus_->setText("Asset source unavailable: "+error);
        emit message(error);
        return;
    }
    const bool changed=loaded!=assets_;
    assets_=std::move(loaded);
    if(changed)publishRigModels();
    inspectPose();
    if(changed)viewport_->frameRig();
    assetStatus_->setText(QString("%1 models  /  %2 missing models  /  %3 missing PNGs  /  %4")
        .arg(assets_->loadedModels.size()).arg(assets_->missingModels.size())
        .arg(assets_->missingTextures.size()).arg(QFileInfo(sourcePath_).fileName()));
    assetStatus_->setToolTip((assets_->missingModels+assets_->missingTextures+
                             assets_->warnings).mid(0,50).join("\n"));
}
bool AnimationWorkspace::loadAssetSource(const QString& path,QString& error,bool remember){
    error.clear();
    if(!QFileInfo::exists(path)){
        error="The selected mod asset source does not exist: "+path;return false;
    }
    const QString previous=sourcePath_;
    sourcePath_=path;
    if(active_){
        refreshAssets();
        if(!assets_){
            error="Could not read any machine assets from "+path;
            sourcePath_=previous;
            refreshAssets();
            return false;
        }
    }
    if(remember){
        QSettings prefs;
        prefs.setValue(QFileInfo(path).isDir()?"animation/asset-root":"animation/mod-jar",path);
        emit animationChanged(metadata());
    }
    return true;
}
void AnimationWorkspace::chooseModJar(){
    const QString chosen=QFileDialog::getOpenFileName(this,"Select installed Magneticraft2 JAR",
        sourcePath_,"Minecraft Forge mod (*.jar *.zip)");
    if(chosen.isEmpty())return;
    QString error;
    // Import both documents together only if the user explicitly accepts it.
    QJsonObject jarManifest,jarScene;
    if(RigAssetLoader::loadMachineDefinition(chosen,machine_->currentText(),
          jarManifest,jarScene,error)){
        if((jarManifest!=manifest_||jarScene!=scene_) &&
           QMessageBox::question(this,"Use this mod version's animation JSON?",
             "The installed mod contains a different machine visual definition.\n\n"
             "Replace the current animation document with the matching pair from this JAR?\n"
             "Choose No to load only its models and textures, preserving your edits.",
             QMessageBox::Yes|QMessageBox::No,QMessageBox::No)==QMessageBox::Yes){
            manifest_=jarManifest;scene_=jarScene;
            hasSavedRuntime_=true;
            refreshComponents();refreshInputs();
        }
    }else{
        emit message("JAR definition not imported: "+error+
                     " (models can still be loaded separately)");
    }
    if(!loadAssetSource(chosen,error))QMessageBox::warning(this,"Asset import failed",error);
}
void AnimationWorkspace::chooseAssetFolder(){
    const QString folder=QFileDialog::getExistingDirectory(this,"Select unpacked Minecraft assets folder",
        QSettings().value("animation/asset-root").toString());
    if(folder.isEmpty())return;
    QString error;
    if(!loadAssetSource(folder,error))QMessageBox::warning(this,"Asset import failed",error);
}
void AnimationWorkspace::chooseVanillaJar(){
    const QString chosen=QFileDialog::getOpenFileName(this,"Select local Minecraft client JAR or resource pack",
        vanillaPath_,"Minecraft client / resource pack (*.jar *.zip)");
    if(chosen.isEmpty())return;
    if(!QFileInfo::exists(chosen))return;
    vanillaPath_=chosen;
    QSettings().setValue("animation/minecraft-jar",chosen);
    refreshAssets(true);
}
void AnimationWorkspace::changeMachine(const QString& machine){
    if(settingUi_)return;
    if(hasSavedRuntime_){
        const auto answer=QMessageBox::warning(this,"Replace this project's animation?",
            "Changing machines replaces this project's current animation bindings and tracks. "
            "You can cancel without losing any edits.",
            QMessageBox::Discard|QMessageBox::Cancel,QMessageBox::Cancel);
        if(answer!=QMessageBox::Discard){
            QSignalBlocker blocker(machine_);
            machine_->setCurrentText(acceptedMachine_);
            return;
        }
    }
    QJsonObject nextManifest,nextScene;QString error;
    if(!VisualTemplates::bundled(machine,nextManifest,nextScene,error)){
        emit message(error);return;
    }
    manifest_=nextManifest;scene_=nextScene;acceptedMachine_=machine;
    hasSavedRuntime_=true;
    assets_.reset();previewAngles_.clear();
    rebuildAuthoredGeometry();
    refreshComponents();refreshInputs();
    refreshAssets();
    emit animationChanged(metadata());
}
bool AnimationWorkspace::saveChanges(QJsonObject m,QJsonObject s,const QString& action){
    QString error;
    if(!VisualContract::validatePair(machine_->currentText(),m,s,error)){
        QMessageBox::warning(this,"Invalid mechanical animation",error+
             "\n\nThe edit was not applied. The existing animation remains unchanged.");
        return false;
    }
    manifest_=std::move(m);scene_=std::move(s);
    hasSavedRuntime_=true;
    rebuildAuthoredGeometry();
    refreshComponents();refreshInputs();refreshAssets();
    emit animationChanged(metadata());
    emit message(action+" saved in Studio project");
    return true;
}
bool AnimationWorkspace::ensureInput(QJsonObject& manifest,QJsonObject& scene,QString& id){
    if(!id.isEmpty())return true;
    const QString physical=previewSource_->currentText();
    if(physical.isEmpty())return false;
    QJsonObject channels=manifest.value("angle_channels").toObject();
    QString alias;
    for(auto it=channels.begin();it!=channels.end();++it)
        if(it.value()==physical){alias=it.key();break;}
    if(alias.isEmpty()){
        alias="studio_"+physical;
        channels[alias]=physical;
        manifest["angle_channels"]=channels;
    }
    QJsonObject inputs=scene.value("inputs").toObject();
    for(auto it=inputs.begin();it!=inputs.end();++it)
        if(it.value().toObject().value("module")==alias){id=it.key();return true;}
    QString base="studio_drive",key=base;
    for(int i=2;inputs.contains(key);i++)key=base+"_"+QString::number(i);
    inputs[key]=QJsonObject{{"source","mechanical.visual_angle_degrees"},
                            {"module",alias}};
    scene["inputs"]=inputs;
    id=key;
    return true;
}
void AnimationWorkspace::changePivot(){
    const QString target=component_->currentText();
    if(target.isEmpty())return;
    QJsonObject s=scene_,components=s.value("components").toObject(),
                part=components.value(target).toObject();
    part["pivot"]=point3(pivot_);
    components[target]=part;s["components"]=components;
    saveChanges(manifest_,s,"Pivot "+target);
}
void AnimationWorkspace::changeRotation(){
    const QString target=component_->currentText();
    if(target.isEmpty())return;
    QJsonObject m=manifest_,s=scene_,animations=s.value("animations").toObject();
    QString input=driver_->currentText();
    if(!ensureInput(m,s,input)){
        QMessageBox::warning(this,"No mechanical input","Choose a valid Java shaft source first.");
        return;
    }
    const QString chosen=selectedTrack(track_);
    QString name;int index=-1;
    if(!chosen.isEmpty()){
        const int split=chosen.lastIndexOf('#');
        if(split>=0){name=chosen.left(split);index=chosen.mid(split+1).toInt();}
    }
    if(name.isEmpty()){
        name="studio_rotation";
        if(!animations.contains(name)){
            animations[name]=QJsonObject{{"type","driven"},{"tracks",QJsonArray{}}};
        }else if(animations.value(name).toObject().value("type")!="driven"){
            int i=2;
            while(animations.contains("studio_rotation_"+QString::number(i)))i++;
            name="studio_rotation_"+QString::number(i);
            animations[name]=QJsonObject{{"type","driven"},{"tracks",QJsonArray{}}};
        }
    }
    QJsonObject animation=animations.value(name).toObject();
    QJsonArray tracks=animation.value("tracks").toArray();
    QJsonObject track;
    if(index>=0&&index<tracks.size())track=tracks[index].toObject();
    track["target"]=target;
    track["property"]="rotation";
    track["axis"]=axis_->currentText();
    track["input"]=input;
    track["scale"]=scale_->value();
    track["phase_degrees"]=phase_->value();
    track["when"]=when_->currentText();
    if(index>=0&&index<tracks.size())tracks[index]=track;
    else tracks.append(track);
    animation["type"]="driven";animation["tracks"]=tracks;
    animations[name]=animation;s["animations"]=animations;
    if(saveChanges(m,s,"Rotation "+target)){
        const QString reference=name+"#"+QString::number(index>=0?index:tracks.size()-1);
        const int ix=track_->findData(reference);
        if(ix>=0)track_->setCurrentIndex(ix);
    }
}
void AnimationWorkspace::tick(){
    const QString source=previewSource_->currentText();
    if(!active_||source.isEmpty()){stopPlayback();return;}
    const double elapsed=clock_.isValid()?std::clamp(clock_.restart()/1000.0,0.0,0.15):0.0;
    const double angle=previewAngles_.value(source)+previewSpeed_->value()*elapsed;
    // Wrap only at a large value, not at 360: driven belts need the signed,
    // cumulative shaft angle and would visually jump if reset each turn.
    previewAngles_[source]=angle>9999999?-9999999:angle<-9999999?9999999:angle;
    QSignalBlocker blocker(previewAngle_);
    previewAngle_->setValue(previewAngles_.value(source));
    inspectPose();
}
void AnimationWorkspace::stopPlayback(){
    if(timer_)timer_->stop();
    if(play_)play_->setText("Play");
}
} // namespace studio
