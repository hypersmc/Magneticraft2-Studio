#include "io/ProjectIO.hpp"
#include <QCoreApplication>
#include <QImage>
#include <QBuffer>
#include <QColor>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <cassert>
#include <iostream>

using namespace studio;
int main(int argc,char**argv) {
    QCoreApplication app(argc,argv);
    QString err;LoadResult result;
    Project original=makeDemo();
    original.parts.front().rotationDegrees={0,45,0};
    original.parts.front().faces[0].automatic=false;
    original.parts.front().faces[0].uv={0.125,0.25,0.75,0.875};
    original.parts.front().faces[0].texture="textures/demo.png";
    original.parts.front().faces[0].quarterTurns=2;
    QImage demo(16,16,QImage::Format_RGBA8888);demo.fill(QColor("#a86a43"));
    QByteArray png;QBuffer stream(&png);stream.open(QIODevice::WriteOnly);
    assert(demo.save(&stream,"PNG"));
    original.textures["textures/demo.png"]=png.toBase64().toStdString();

    QJsonObject payload=ProjectIO::encode(original);
    payload["pluginMetadata"]=QJsonObject{{"test",17}};
    bool ok=ProjectIO::decode(QJsonDocument(payload).toJson(),result,err);
    assert(ok && err.isEmpty());
    assert(result.project.parts.size()==original.parts.size());
    assert(result.project.parts.front().rotationDegrees.y==45);
    assert(!result.project.parts.front().faces[0].automatic);
    assert(result.project.parts.front().faces[0].uv[0]==0.125);
    assert(result.project.parts.front().faces[0].texture=="textures/demo.png");
    assert(result.project.parts.front().faces[0].quarterTurns==2);
    assert(result.project.textures.at("textures/demo.png")==original.textures.at("textures/demo.png"));
    // Reject malformed embedded PNG data before it reaches the GPU uploader.
    QJsonObject malformed=payload;
    malformed["textures"]=QJsonObject{{"bad","not-an-image"}};
    LoadResult badResult;QString badMessage;
    assert(!ProjectIO::decode(QJsonDocument(malformed).toJson(),badResult,badMessage));
    QJsonObject roundTrip=ProjectIO::encode(result.project,result.original);
    assert(roundTrip.value("pluginMetadata").toObject().value("test").toInt()==17);
    assert(roundTrip.value("parts").toArray().size()==payload.value("parts").toArray().size());
    const auto source=QString(STUDIO_TEST_SOURCE_DIR)+"/samples/mechanical_ore_washer_north.json";
    ok=ProjectIO::read(source,result,err);assert(ok && result.project.dimensions[0]==3 && result.project.cells.size()>5);
    QTemporaryDir temp;assert(temp.isValid());
    ok=ProjectIO::write(temp.filePath("saved.mcraftstudio.json"),result.project,result.original,err);assert(ok);
    ok=ProjectIO::read(temp.filePath("saved.mcraftstudio.json"),result,err);assert(ok);
    ok=ProjectIO::exportForgeDirectory(temp.filePath("forge"),result.project,result.original,err);assert(ok);
    QFile generated(temp.filePath("forge/data/magneticraft2/multiblocks/mechanical_ore_washer_north.json"));
    assert(generated.open(QIODevice::ReadOnly));
    QJsonDocument json=QJsonDocument::fromJson(generated.readAll());
    assert(json.object().value("structure").toObject().value("dimensions").toArray().size()==3);
    // A Java block is a distinct document type with its own export path.
    Project block=makeJavaBlockProject("Copper Casing");
    const auto doc=ProjectIO::encode(block);
    assert(doc.value("projectType").toString()=="java_block");
    assert(ProjectIO::decode(QJsonDocument(doc).toJson(),result,err));
    assert(result.project.kind==ProjectKind::JavaBlock);
    assert((result.project.dimensions==Dimensions{3,3,3}));
    const QString modelFile=temp.filePath("copper_casing.json");
    assert(ProjectIO::exportJavaBlockModel(modelFile,block,err));
    QFile blockExport(modelFile);assert(blockExport.open(QIODevice::ReadOnly));
    const auto exported=QJsonDocument::fromJson(blockExport.readAll()).object();
    assert(exported.value("elements").toArray().size()==1);
    const auto box=exported.value("elements").toArray().at(0).toObject();
    assert(box.value("from").toArray().at(0).toInt()==0);
    assert(box.value("to").toArray().at(0).toInt()==16);
    assert(box.value("faces").toObject().size()==6);
    assert(!ProjectIO::exportForgeDirectory(temp.filePath("wrong"),block,{},err));
    block.parts.front().bounds.to.x=3.0625;
    assert(!ProjectIO::exportJavaBlockModel(modelFile,block,err));
    assert(!ProjectIO::write(temp.filePath("invalid.json"),block,{},err));
    std::cout<<"Native Qt JSON IO tests passed\n";
    return 0;
}
