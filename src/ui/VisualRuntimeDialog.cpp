#include "VisualRuntimeDialog.hpp"
#include "io/VisualContract.hpp"

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
         {"input_axle",QJsonObject{{"model","magneticraft2:multiblock/primitive_grinder_input_axle"},
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
    layout->addWidget(editors,4);
    auto* previewPanel=new QGroupBox("Deterministic runtime pose inspection — physical shaft angle",this);
    auto* pl=new QVBoxLayout(previewPanel);
    auto* controls=new QHBoxLayout;
    shaftAngle_=new QDoubleSpinBox(previewPanel);
    shaftAngle_->setObjectName("VisualPhysicalAngle");
    shaftAngle_->setRange(-720,720);shaftAngle_->setSingleStep(15);
    shaftAngle_->setSuffix("°");
    controls->addWidget(new QLabel("Angle",previewPanel));controls->addWidget(shaftAngle_);
    running_=new QCheckBox("Running",previewPanel);running_->setChecked(true);
    overloaded_=new QCheckBox("Overloaded",previewPanel);
    worn_=new QCheckBox("Worn",previewPanel);
    controls->addWidget(running_);controls->addWidget(overloaded_);controls->addWidget(worn_);
    controls->addWidget(new QLabel("Wear stage",previewPanel));
    wearStage_=new QSpinBox(previewPanel);wearStage_->setRange(0,5);
    controls->addWidget(wearStage_);controls->addStretch();
    pl->addLayout(controls);
    preview_=new QPlainTextEdit(previewPanel);
    preview_->setObjectName("VisualPoseSample");
    preview_->setReadOnly(true);preview_->setMinimumHeight(160);
    pl->addWidget(preview_);
    layout->addWidget(previewPanel,2);
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
        sources->setText("Java angles: "+VisualContract::availableAngles(machine).join(", ")+
          "    |    Java integers: "+(VisualContract::availableIntegers(machine).isEmpty()
             ?QString("none"):VisualContract::availableIntegers(machine).join(", ")));
        sample();
    });
    connect(shaftAngle_,QOverload<double>::of(&QDoubleSpinBox::valueChanged),this,[this](double){sample();});
    connect(wearStage_,QOverload<int>::of(&QSpinBox::valueChanged),this,[this](int){sample();});
    for(auto* c:{running_,overloaded_,worn_})
        connect(c,&QCheckBox::toggled,this,[this](bool){sample();});
    sources->setText("Java angles: "+VisualContract::availableAngles(machine_->currentText()).join(", ")+
      "    |    Java integers: "+(VisualContract::availableIntegers(machine_->currentText()).isEmpty()
          ?QString("none"):VisualContract::availableIntegers(machine_->currentText()).join(", ")));
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
        preview_->setPlainText("Cannot inspect invalid scene:\n"+error);return;
    }
    m["_studio_machine"]=machine_->currentText();
    QMap<QString,double> inputs;
    for(const auto& source:VisualContract::availableAngles(machine_->currentText()))
        inputs[source]=shaftAngle_->value();
    const auto pose=VisualContract::sample(m,s,inputs,running_->isChecked(),overloaded_->isChecked(),
                                           worn_->isChecked(),wearStage_->value(),error);
    if(!error.isEmpty())preview_->setPlainText(error);
    else preview_->setPlainText(pretty(pose));
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
