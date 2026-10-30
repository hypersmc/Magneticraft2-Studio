#include "io/ProjectIO.hpp"
#include "io/AnimationExport.hpp"
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
    // The native editor preserves animation metadata until the full motion
    // timeline exists. The export-only bridge must produce the EXACT typed
    // schema consumed by Forge without fabricating baked model geometry.
    Project rig=makeDemo();
    assert(rig.parts.size()>=2);
    rig.parts[0].motionId="shaft";
    rig.parts[1].motionId="shaft";
    QJsonObject rigProject=ProjectIO::encode(rig);
    QJsonArray rigParts=rigProject.value("parts").toArray();
    QJsonObject rootPart=rigParts[0].toObject();
    rootPart["animation"]=QJsonObject{
        {"model","magneticraft2:animated/input_shaft"},
        {"model_offset",QJsonArray{0.0,0.0,-2.0}},
        {"pivot",QJsonArray{0.5,0.5,0.5}}
    };
    rigParts[0]=rootPart;
    QJsonObject childPart=rigParts[1].toObject();
    childPart["animation"]=QJsonObject{
        {"model","magneticraft2:animated/shaft_marker"},
        {"model_offset",QJsonArray{0.0,0.0,0.0}},
        {"pivot",QJsonArray{0.0,0.0,0.0}},
        {"parent",QString::fromStdString(rig.parts[0].id)},
        {"visible_when","running"}
    };
    rigParts[1]=childPart;
    rigProject["parts"]=rigParts;
    rigProject["motions"]=QJsonArray{QJsonObject{
        {"id","shaft"},{"type","rotate"},{"axis","z"},
        {"driver_source","mechanical.visual_angle_degrees"},
        {"driver_module","mechanical_input"},{"scale",-1.0},
        {"speed",15.0} // Legacy Studio speed MUST NOT become Gear V2 RPM.
    }};
    LoadResult rigLoaded; assert(ProjectIO::decode(
        QJsonDocument(rigProject).toJson(),rigLoaded,err));
    QJsonObject animation;
    assert(AnimationExport::build(rigLoaded.project,rigLoaded.original,animation,err));
    assert(animation.value("format").toString()=="magneticraft2:visual_animation");
    assert(animation.value("format_version").toInt()==1);
    const QJsonObject rigComponents=animation.value("components").toObject();
    assert(rigComponents.size()==2);
    assert(rigComponents.value(QString::fromStdString(rig.parts[1].id))
        .toObject().value("parent").toString()==QString::fromStdString(rig.parts[0].id));
    const QJsonObject driven=animation.value("animations").toObject()
        .value("driven_shaft").toObject();
    assert(driven.value("tracks").toArray().size()==2);
    assert(driven.value("tracks").toArray().at(0).toObject().value("scale").toDouble()==-1.0);
    assert(AnimationExport::write(
        temp.filePath("bronze_mechanical_lathe.json"),
        rigLoaded.project,rigLoaded.original,err));
    QFile animationFile(temp.filePath("bronze_mechanical_lathe.json"));
    assert(animationFile.open(QIODevice::ReadOnly));
    assert(QJsonDocument::fromJson(animationFile.readAll()).object()==animation);
    // A typo in a parent chain must fail before writing a misleading asset.
    QJsonObject broken=rigLoaded.original;
    QJsonArray brokenParts=broken.value("parts").toArray();
    QJsonObject brokenChild=brokenParts[1].toObject();
    QJsonObject brokenBinding=brokenChild.value("animation").toObject();
    brokenBinding["parent"]="not_a_real_component";
    brokenChild["animation"]=brokenBinding;
    brokenParts[1]=brokenChild;broken["parts"]=brokenParts;
    assert(!AnimationExport::build(rigLoaded.project,broken,animation,err));
    assert(err.contains("Unknown parent"));
    // Runtime-v1 component binding is independent of the older motionId.
    // Saving and reloading a bound cuboid must preserve both fields.
    Project authored=makeDemo();
    authored.parts.front().motionId="legacy_gear";
    authored.parts.front().rigComponentId="drum";
    const auto authoredJson=ProjectIO::encode(authored);
    LoadResult authoredLoaded;
    assert(ProjectIO::decode(QJsonDocument(authoredJson).toJson(),authoredLoaded,err));
    assert(authoredLoaded.project.parts.front().motionId=="legacy_gear");
    assert(authoredLoaded.project.parts.front().rigComponentId=="drum");
    Project plain=makeDemo();
    const auto oldParts=ProjectIO::encode(plain).value("parts").toArray();
    assert(!oldParts.front().toObject().contains("rigComponent"));

    // Optional v1 animation metadata must survive old Studio document edits,
    // including unknown schema fields that the native project does not own.
    QJsonObject oldDocument=ProjectIO::encode(makeDemo());
    oldDocument.remove("visualRuntime");
    LoadResult oldLoaded;
    assert(ProjectIO::decode(QJsonDocument(oldDocument).toJson(),oldLoaded,err));
    assert(!ProjectIO::encode(oldLoaded.project,oldLoaded.original).contains("visualRuntime"));
    QJsonObject visual{
        {"machine","primitive_grinder"},
        {"manifest",QJsonObject{{"format","magneticraft2:machine_visual"},
                                {"unknown_pack_note","preserve exactly"}}},
        {"scene",QJsonObject{{"format","magneticraft2:visual_animation"},
                             {"components",QJsonObject{{"axle",QJsonObject{{"future_property",42}}}}}}}
    };
    oldDocument["visualRuntime"]=visual;
    assert(ProjectIO::decode(QJsonDocument(oldDocument).toJson(),oldLoaded,err));
    oldLoaded.project.name="Renamed without touching animation data";
    const auto saved=ProjectIO::encode(oldLoaded.project,oldLoaded.original);
    assert(saved.value("visualRuntime").toObject()==visual);
    assert(ProjectIO::write(temp.filePath("visual-roundtrip.mcraftstudio.json"),
                             oldLoaded.project,oldLoaded.original,err));
    LoadResult loadedVisual;
    assert(ProjectIO::read(temp.filePath("visual-roundtrip.mcraftstudio.json"),loadedVisual,err));
    assert(loadedVisual.original.value("visualRuntime").toObject()==visual);
    std::cout<<"Native Qt JSON IO tests passed\n";
    return 0;
}
