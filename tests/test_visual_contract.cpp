#include "io/VisualContract.hpp"
#include "io/VisualTemplates.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryDir>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace studio;
static QByteArray read(const QString& path){
    QFile file(path);assert(file.open(QIODevice::ReadOnly));return file.readAll();
}
static bool close(double a,double b){return std::abs(a-b)<1e-5;}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const QString root=QString(STUDIO_TEST_SOURCE_DIR)+"/tests/fixtures/animation-v1/";
    const QStringList machines={"bronze_mechanical_lathe","mechanical_sifter",
      "primitive_grinder","mechanical_ore_washer","bronze_stamp_mill","rotary_ore_classifier"};
    for(const QString& machine:machines){
        QJsonObject manifest,scene;QString error;QStringList warnings;
        const auto mb=read(root+"machine_visuals/"+machine+".json");
        const auto sb=read(root+"animations/"+machine+".json");
        if(!VisualContract::importPair(machine,mb,sb,manifest,scene,error,&warnings)){
           std::cerr<<machine.toStdString()<<": "<<error.toStdString()<<"\n";return 9;
        }
        QTemporaryDir temp;assert(temp.isValid());
        QJsonObject bundledManifest,bundledScene;
        assert(VisualTemplates::bundled(machine,bundledManifest,bundledScene,error));
        assert(bundledManifest==manifest && bundledScene==scene);
        assert(VisualContract::exportPair(temp.path(),machine,manifest,scene,error,&warnings));
        const QString sceneId=manifest.value("animation").toString();
        const QString path=temp.path()+"/assets/"+sceneId.section(':',0,0)+
            "/animations/"+sceneId.section(':',1)+".json";
        QJsonObject manifest2,scene2;
        assert(VisualContract::importPair(machine,
            read(temp.path()+"/assets/magneticraft2/machine_visuals/"+machine+".json"),
            read(path),manifest2,scene2,error));
        assert(scene==scene2 && manifest==manifest2);
        QJsonObject invalid=manifest;
        invalid["angle_channels"]=QJsonObject{{"invalid","arbitrary_nonexistent_port"}};
        assert(!VisualContract::validatePair(machine,invalid,scene,error));
        const auto comps=scene.value("components").toObject();
        if(comps.size()>1){
            QJsonObject broken=scene,parts=comps;
            auto first=parts.begin();
            QJsonObject node=first.value().toObject();node["parent"]=first.key();
            parts[first.key()]=node;broken["components"]=parts;
            assert(!VisualContract::validateScene(broken,error));
        }
        QJsonObject physical=manifest;
        physical["_studio_machine"]=machine;
        QMap<QString,double> angleValues;
        for(const auto& source:VisualContract::availableAngles(machine))angleValues[source]=90.0;
        QJsonObject pose=VisualContract::sample(physical,scene,angleValues,true,false,false,0,error);
        assert(error.isEmpty() && pose.value("components").toObject().size()==comps.size());
        if(machine=="mechanical_sifter"){
            const auto k=pose.value("mechanisms").toObject().value("sieve_drive").toObject();
            assert(!k.isEmpty());
            assert(close(k.value("stroke_z").toDouble(),0.12));
        }
    }
    QJsonObject rootScene{
      {"format","magneticraft2:visual_animation"},{"format_version",1},
      {"coordinate_system",QJsonObject{{"units","blocks"},{"origin","controller"},{"reference_facing","south"}}},
      {"components",QJsonObject{{"joint",QJsonObject{{"model_offset",QJsonArray{0,0,0}},
         {"pivot",QJsonArray{0,0,0}}}}}},
      {"inputs",QJsonObject{}},{"animations",QJsonObject{}}
    };
    QString error;
    assert(VisualContract::validateScene(rootScene,error));
    std::cout<<"Validated and losslessly round-tripped 6 real Java manifests and scenes\n";
}
