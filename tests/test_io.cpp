#include "io/ProjectIO.hpp"
#include <QCoreApplication>
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
    QJsonObject payload=ProjectIO::encode(original);
    payload["pluginMetadata"]=QJsonObject{{"test",17}};
    bool ok=ProjectIO::decode(QJsonDocument(payload).toJson(),result,err);
    assert(ok && err.isEmpty());
    assert(result.project.parts.size()==original.parts.size());
    assert(result.project.parts.front().rotationDegrees.y==45);
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
    std::cout<<"Native Qt JSON IO tests passed\n";
    return 0;
}
