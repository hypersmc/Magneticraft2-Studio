#include "BlockModelImport.hpp"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <algorithm>
#include <cmath>

namespace studio {
namespace {

constexpr const char* names[] = {"south", "north", "west", "east", "up", "down"};

bool triple(const QJsonValue& value, Vec3& xyz) {
    const QJsonArray values=value.toArray();
    if(values.size()!=3)return false;
    for(const QJsonValue& v:values)if(!v.isDouble()||!std::isfinite(v.toDouble()))return false;
    xyz={values[0].toDouble(),values[1].toDouble(),values[2].toDouble()};
    return true;
}
QString textureOf(const QString& raw,const QJsonObject& aliases) {
    QString value=raw;
    // Resolve texture indirections; reject loops and broken aliases gracefully.
    for(int i=0;i<16 && value.startsWith('#');++i) {
        const QString next=aliases.value(value.mid(1)).toString();
        if(next.isEmpty()||next==value)return {};
        value=next;
    }
    if(value.startsWith('#'))return {};
    if(!value.contains(':'))value.prepend("minecraft:");
    return value;
}
QString materialFor(const QString& texture) {
    if(texture.contains("copper",Qt::CaseInsensitive))return "copper";
    if(texture.contains("iron",Qt::CaseInsensitive)||texture.contains("steel",Qt::CaseInsensitive))return "iron";
    if(texture.contains("water",Qt::CaseInsensitive))return "water";
    if(texture.contains("glass",Qt::CaseInsensitive))return "glass";
    if(texture.contains("stone",Qt::CaseInsensitive))return "stone";
    return "wood";
}
}
BlockModelImportResult importBlockModel(const QByteArray& json,const Project& project,
                                       const QString& group,Vec3 anchor) {
    BlockModelImportResult result;
    QJsonParseError parse;
    auto doc=QJsonDocument::fromJson(json,&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject()){
        result.error="Invalid Minecraft model JSON: "+parse.errorString();return result;
    }
    const QJsonObject root=doc.object();
    const QJsonValue elementsValue=root.value("elements");
    if(!elementsValue.isArray()){
        result.error="No 'elements' array. Parent-only models and mesh loaders are not supported yet.";return result;
    }
    const auto elements=elementsValue.toArray();
    if(elements.size()>20000){result.error="Too many elements (20,000 maximum per import).";return result;}
    if(elements.isEmpty()){result.error="The model contains no cuboids.";return result;}
    const QJsonObject textures=root.value("textures").toObject();
    const int firstNumber=int(project.parts.size());
    QStringList namesUsed;
    for(int i=0;i<elements.size();++i){
        const QJsonObject element=elements[i].toObject();
        Vec3 from{},to{};
        if(!triple(element.value("from"),from)||!triple(element.value("to"),to)){
            result.error=QString("Element %1 has invalid from/to coordinates.").arg(i+1);result.parts.clear();return result;
        }
        Bounds bounds{{from.x/16.0+anchor.x,from.y/16.0+anchor.y,from.z/16.0+anchor.z},
                      {to.x/16.0+anchor.x,to.y/16.0+anchor.y,to.z/16.0+anchor.z}};
        if(!validBounds(bounds)){
            result.error=QString("Element %1 has inverted or zero-sized bounds.").arg(i+1);result.parts.clear();return result;
        }
        Part part=makeBox(project,bounds);
        part.id="import_"+std::to_string(firstNumber+i+1);
        // Model imports can be repeated. IDs must remain stable and unique.
        while(std::any_of(project.parts.begin(),project.parts.end(),[&](const Part& p){return p.id==part.id;}) ||
              std::any_of(result.parts.begin(),result.parts.end(),[&](const Part& p){return p.id==part.id;}))part.id+="_";
        part.name=QString("Element %1").arg(i+1,3,10,QChar('0')).toStdString();
        part.group=group.toStdString();
        part.pivot=(bounds.from+bounds.to)*0.5;
        const QJsonObject rotation=element.value("rotation").toObject();
        if(!rotation.isEmpty()){
            Vec3 pivot{};
            if(!triple(rotation.value("origin"),pivot)){
                result.error=QString("Element %1 contains an unsupported rotation origin.").arg(i+1);result.parts.clear();return result;
            }
            part.pivot={pivot.x/16.+anchor.x,pivot.y/16.+anchor.y,pivot.z/16.+anchor.z};
            const QString axis=rotation.value("axis").toString();
            const double angle=rotation.value("angle").toDouble();
            if(axis=="x")part.rotationDegrees.x=angle;
            else if(axis=="y")part.rotationDegrees.y=angle;
            else if(axis=="z")part.rotationDegrees.z=angle;
            else {result.error=QString("Element %1 contains an unsupported rotation axis.").arg(i+1);result.parts.clear();return result;}
            if(rotation.value("rescale").toBool(false))result.warnings << QString("Element %1 uses rescale; Minecraft rescale is not yet represented in Studio.").arg(i+1);
        }
        const QJsonObject faces=element.value("faces").toObject();
        QString defaultTexture;
        for(int face=0;face<6;face++){
            const QJsonObject data=faces.value(names[face]).toObject();
            if(data.isEmpty()){part.faces[face].enabled=false;continue;}
            const QString id=textureOf(data.value("texture").toString(),textures);
            if(!id.isEmpty()){
                part.faces[face].texture=id.toStdString();
                if(defaultTexture.isEmpty())defaultTexture=id;
            }
            const auto uv=data.value("uv").toArray();
            if(uv.size()==4 && std::all_of(uv.begin(),uv.end(),[](const QJsonValue& v){return v.isDouble();})){
                auto& u=part.faces[face];u.automatic=false;
                u.uv={uv[0].toDouble()/16.,uv[1].toDouble()/16.,uv[2].toDouble()/16.,uv[3].toDouble()/16.};
            }
            part.faces[face].quarterTurns=((data.value("rotation").toInt()/90)%4+4)%4;
        }
        if(!defaultTexture.isEmpty()){
            part.texture=defaultTexture.toStdString();
            part.material=materialFor(defaultTexture).toStdString();
        }
        result.parts.push_back(std::move(part));
    }
    if(root.contains("parent")&&!root.value("parent").toString().isEmpty())
        result.warnings << "Parent model references are not resolved; imported local elements only.";
    return result;
}
}
