#include "io/RigTransforms.hpp"
#include "io/RigAssetLoader.hpp"
#include "io/VisualContract.hpp"
#include <QCoreApplication>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QVector3D>
#include <QTemporaryDir>
#include <cassert>
#include <cmath>
#include <iostream>
using namespace studio;
static QByteArray read(const QString& p){QFile f(p);assert(f.open(QIODevice::ReadOnly));return f.readAll();}
static bool near(double x,double y,double eps=1e-4){return std::abs(x-y)<eps;}
int main(int argc,char** argv){
    QCoreApplication app(argc,argv);
    const QString root=QString(STUDIO_TEST_SOURCE_DIR)+"/tests/fixtures/animation-v1/";
    const QStringList machines={"bronze_mechanical_lathe","mechanical_sifter","primitive_grinder",
                                  "mechanical_ore_washer","bronze_stamp_mill","rotary_ore_classifier"};
    for(const auto& machine:machines){
        QString error;QJsonObject m,s;
        assert(VisualContract::importPair(machine,read(root+"machine_visuals/"+machine+".json"),
            read(root+"animations/"+machine+".json"),m,s,error));
        QMap<QString,double> angles;
        for(const QString& source:VisualContract::availableAngles(machine))
            angles[source]=source=="output_shaft"?45.0:90.0;
        RigPose pose;
        if(!RigTransforms::evaluate(machine,m,s,angles,true,false,false,2,pose,error)){
            std::cerr<<error.toStdString()<<"\n";return 8;
        }
        assert(pose.components.size()==std::size_t(s.value("components").toObject().size()));
        if(machine=="mechanical_sifter"){
            const auto& crank=pose.components.at("sieve_rod").world;
            const auto& follower=pose.components.at("sieve_follower").world;
            const QVector3D pin=crank.map(QVector3D(0,0,0));
            const QVector3D end=follower.map(QVector3D(0,0,0));
            assert(near(double((end-pin).length()),0.4375,2e-5));
            assert(near(pose.components.at("sieve_carriage").world.map(QVector3D(0,0,0)).z(),0.12));
            assert(pose.components.at("upper_tray").modelId.find("wear_2")!=std::string::npos);
            const QVector3D pivot(0.5f,0.78f,1.2f);
            const auto child=pose.components.at("upper_tray").world.map(pivot);
            const auto parent=pose.components.at("sieve_carriage").world.map(pivot);
            assert(near(double((child-parent).length()),0.0));
        }
        if(machine=="bronze_stamp_mill"){
            assert(pose.beltDistanceBlocks.count("drive_belt")==1);
            assert(near(pose.beltDistanceBlocks.at("drive_belt"),45.*3.141592653589793/180.*0.2978125));
            RigPose idle;
            assert(RigTransforms::evaluate(machine,m,s,angles,false,false,false,0,idle,error));
            const double headY=idle.components.at("hammer_middle").world.map(QVector3D()).y();
            const auto part=s.value("components").toObject().value("hammer_middle").toObject();
            assert(near(headY,part.value("model_offset").toArray()[1].toDouble()));
        }
        if(machine=="primitive_grinder"){
            auto changed=s;auto parts=changed.value("components").toObject();
            const QString first=parts.begin().key();
            auto part=parts.value(first).toObject();part["visible_when"]="stopped";
            parts[first]=part;changed["components"]=parts;
            RigPose modified;
            assert(RigTransforms::evaluate(machine,m,changed,angles,true,false,false,0,modified,error));
            assert(!modified.components.at(first.toStdString()).visible);
        }
    }
    // Parent-only inheritance must resolve the actual texture-slot override;
    // the importer must NOT fill in five sides missing from the Java model.
    QTemporaryDir temp;assert(temp.isValid());
    QDir rootDir(temp.path());assert(rootDir.mkpath("assets/magneticraft2/models/studio"));
    const QByteArray base=R"({"textures":{"surface":"minecraft:block/oak_planks"},
      "elements":[{"from":[0,0,0],"to":[16,16,16],
      "faces":{"south":{"texture":"#surface","uv":[0,0,16,16]}}}]})";
    const QByteArray child=R"({"parent":"magneticraft2:studio/base",
      "textures":{"surface":"minecraft:block/stripped_spruce_log"}})";
    for(const auto& name:QStringList{"base","child"}){
        QFile f(rootDir.filePath("assets/magneticraft2/models/studio/"+name+".json"));
        assert(f.open(QIODevice::WriteOnly));
        const QByteArray& bytes=name=="base"?base:child;
        assert(f.write(bytes)==bytes.size());
    }
    QJsonObject scene;scene["components"]=QJsonObject{{"preview",
        QJsonObject{{"model","magneticraft2:studio/child"},{"model_offset",QJsonArray{0,0,0}},
                    {"pivot",QJsonArray{0,0,0}}}}};
    RigAssets assets;
    assert(RigAssetLoader::load(temp.path(),scene,assets));
    assert(assets.models.count("magneticraft2:studio/child")==1);
    assert(assets.models.at("magneticraft2:studio/child").triangles.size()==6);
    assert(assets.missingTextures.contains("minecraft:block/stripped_spruce_log"));
    std::cout<<"Six machine rigs, rigid slider-crank and real model inheritance verified\n";
}
