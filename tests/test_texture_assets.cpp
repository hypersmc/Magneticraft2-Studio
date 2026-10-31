#include "io/TextureAssets.hpp"
#include "core/Project.hpp"
#include "io/BlockModelImport.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QColor>
#include <utility>
#include <QTemporaryDir>

#include <cassert>
#include <iostream>
#include <zlib.h>
using namespace studio;

static QByteArray png(QColor base){
    QImage img(16,16,QImage::Format_RGBA8888);
    img.fill(base);
    for(int y=0;y<16;y++)for(int x=0;x<16;x+=3)img.setPixelColor(x,y,base.lighter(120));
    QByteArray bytes;QBuffer b(&bytes);assert(b.open(QIODevice::WriteOnly));assert(img.save(&b,"PNG"));return bytes;
}
static void put16(QByteArray& data,quint16 n){data.append(char(n&255));data.append(char((n>>8)&255));}
static void put32(QByteArray& data,quint32 n){put16(data,quint16(n&65535));put16(data,quint16(n>>16));}
static QByteArray zipFixture(const QByteArray& image){
    const QByteArray name="assets/minecraft/textures/block/stripped_spruce_log.png";
    QByteArray packed;
    packed.resize(compressBound(uLong(image.size())));
    z_stream stream{};
    assert(deflateInit2(&stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,8,Z_DEFAULT_STRATEGY)==Z_OK);
    stream.next_in=reinterpret_cast<Bytef*>(const_cast<char*>(image.constData()));
    stream.avail_in=uInt(image.size());
    stream.next_out=reinterpret_cast<Bytef*>(packed.data());
    stream.avail_out=uInt(packed.size());
    assert(deflate(&stream,Z_FINISH)==Z_STREAM_END);
    packed.resize(stream.total_out);
    deflateEnd(&stream);
    const quint32 crc=quint32(crc32(crc32(0L,Z_NULL,0),reinterpret_cast<const Bytef*>(image.constData()),uInt(image.size())));
    QByteArray out;
    // Local file header, raw-deflate content.
    put32(out,0x04034b50);put16(out,20);put16(out,0);put16(out,8);
    put16(out,0);put16(out,0);put32(out,crc);
    put32(out,quint32(packed.size()));put32(out,quint32(image.size()));
    put16(out,quint16(name.size()));put16(out,0);out+=name;out+=packed;
    const quint32 dirOffset=quint32(out.size());
    put32(out,0x02014b50);put16(out,20);put16(out,20);put16(out,0);put16(out,8);
    put16(out,0);put16(out,0);put32(out,crc);
    put32(out,quint32(packed.size()));put32(out,quint32(image.size()));
    put16(out,quint16(name.size()));put16(out,0);put16(out,0);
    put16(out,0);put16(out,0);put32(out,0);put32(out,0);out+=name;
    const quint32 dirSize=quint32(out.size())-dirOffset;
    put32(out,0x06054b50);put16(out,0);put16(out,0);put16(out,1);put16(out,1);
    put32(out,dirSize);put32(out,dirOffset);put16(out,0);
    return out;
}
int main(){
    auto project=makeProject("Texture importer",{3,2,3});
    Part part=makeBox(project);part.texture="minecraft:block/stripped_spruce_log";
    part.faces[0].texture="minecraft:block/copper_block";
    project.parts.push_back(part);
    const QStringList required=referencedTextureIds(project);
    assert(required.size()==2);
    assert(missingTextureIds(project).size()==2);
    QTemporaryDir temp;assert(temp.isValid());
    QDir root(temp.path());
    assert(root.mkpath("assets/minecraft/textures/block"));
    const QByteArray wood=png(QColor("#619a40"));
    const QByteArray copper=png(QColor("#c95e42"));
    for(auto [path,data]:{
        std::pair<QString,QByteArray>{"assets/minecraft/textures/block/stripped_spruce_log.png",wood},
        {"assets/minecraft/textures/block/copper_block.png",copper}}){
        QFile file(root.filePath(path));assert(file.open(QIODevice::WriteOnly));assert(file.write(data)==data.size());
    }
    auto fromDirectory=importTextureAssets(project,root.absolutePath());
    assert(fromDirectory.error.isEmpty());
    assert(fromDirectory.imported.size()==2 && fromDirectory.missing.empty());
    assert(QByteArray::fromBase64(QByteArray::fromStdString(project.textures.at("minecraft:block/copper_block")))==copper);
    // Resource ZIPs should be read with no external unzip utility and must
    // import only the resources the model actually references.
    auto zipProject=makeProject("Zip test",{3,2,3});
    Part zipPart=makeBox(zipProject);zipPart.texture="minecraft:block/stripped_spruce_log";
    zipProject.parts.push_back(zipPart);
    const auto archive=root.filePath("assets.jar");
    {QFile file(archive);assert(file.open(QIODevice::WriteOnly));const QByteArray zip=zipFixture(wood);assert(file.write(zip)==zip.size());}
    auto fromJar=importTextureAssets(zipProject,archive);
    assert(fromJar.error.isEmpty());
    assert(fromJar.imported.size()==1 && fromJar.missing.empty());
    assert(QImage::fromData(QByteArray::fromBase64(QByteArray::fromStdString(zipProject.textures.at("minecraft:block/stripped_spruce_log"))),"PNG").pixelColor(1,1)==QColor("#619a40"));
    auto repeat=importTextureAssets(zipProject,archive);
    assert(repeat.imported.empty() && repeat.alreadyPresent.size()==1);
    // Already customized resources are preserved unless the user explicitly requests replacement.
    const QByteArray custom=png(QColor("#d9c900"));
    zipProject.textures["minecraft:block/stripped_spruce_log"]=custom.toBase64().toStdString();
    assert(importTextureAssets(zipProject,archive).imported.empty());
    assert(QByteArray::fromBase64(QByteArray::fromStdString(zipProject.textures.at("minecraft:block/stripped_spruce_log")))==custom);
    assert(importTextureAssets(zipProject,archive,true).imported.size()==1);
    // Invalid/malformed ZIP cannot smuggle corrupted texture bytes into a project.
    const auto badArchive=root.filePath("broken.zip");
    {QFile file(badArchive);assert(file.open(QIODevice::WriteOnly));assert(file.write("not a zip",9)==9);}
    auto missingProject=makeProject("Broken",{3,3,3});missingProject.parts.push_back(zipPart);
    auto failure=importTextureAssets(missingProject,badArchive);
    assert(!failure.error.isEmpty() && missingProject.textures.empty());
    // Studio-local PNG identifiers are not mistakenly classified as Mojang assets.
    auto localProject=makeProject("Local",{3,3,3});
    Part localPart=makeBox(localProject);localPart.texture="textures/custom.png";
    localProject.parts.push_back(localPart);
    localProject.textures["textures/custom.png"]=wood.toBase64().toStdString();
    assert(missingTextureIds(localProject).empty());
#ifdef STUDIO_TEST_SOURCE_DIR
    QFile staticModel(QString(STUDIO_TEST_SOURCE_DIR)+"/samples/ore_washer_models/mechanical_ore_washer_north.json");
    assert(staticModel.open(QIODevice::ReadOnly));
    const auto model=importBlockModel(staticModel.readAll(),makeProject("Ore Washer",{3,2,3}),"Static",{1,0,1});
    assert(model.error.isEmpty());
    auto reference=makeProject("Reference",{3,2,3});reference.parts=model.parts;
    const QStringList refs=missingTextureIds(reference);
    assert(refs.size()==3);
    assert(refs.contains("minecraft:block/oak_planks"));
    assert(refs.contains("minecraft:block/copper_block"));
    assert(refs.contains("minecraft:block/stripped_spruce_log"));
#endif
    std::cout<<"Minecraft JAR/folder asset import and cache-safety tests passed\n";
}
