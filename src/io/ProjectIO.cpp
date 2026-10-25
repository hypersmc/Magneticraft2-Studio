#include "ProjectIO.hpp"
#include "core/Geometry.hpp"

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>
#include <QStringList>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <stdexcept>

namespace studio {

static QString q(const std::string& s) { return QString::fromStdString(s); }
static std::string s(const QJsonValue& v) { return v.toString().toStdString(); }
static QJsonArray array(Vec3 v) { return QJsonArray{v.x,v.y,v.z}; }
static QJsonArray array(Dimensions d) {return QJsonArray{d[0],d[1],d[2]};}
static Vec3 vec(const QJsonValue& value, Vec3 fallback={}) {
    if(!value.isArray() || value.toArray().size()!=3) return fallback;
    auto a=value.toArray();return {a[0].toDouble(fallback.x),a[1].toDouble(fallback.y),a[2].toDouble(fallback.z)};
}
static Dimensions dimsFrom(const QJsonValue& v) {
    if(!v.isArray() || v.toArray().size()!=3)throw std::invalid_argument("Project is missing [width,height,depth] dimensions");
    auto a=v.toArray();Dimensions dim{};
    for(int i=0;i<3;i++){
        if(!a[i].isDouble()||a[i].toDouble()!=std::floor(a[i].toDouble())) throw std::invalid_argument("Dimensions must be integers");
        dim[i]=a[i].toInt();
    }
    validateDimensions(dim);return dim;
}
static QJsonObject partObject(const Part& part,QJsonObject original) {
    original["id"]=q(part.id);original["name"]=q(part.name);
    original["group"]=q(part.group);original["material"]=q(part.material);
    original["visible"]=part.visible;
    original["from"]=array(part.bounds.from);original["to"]=array(part.bounds.to);
    original["pivot"]=array(part.pivot);original["rotation"]=array(part.rotationDegrees);
    if(!part.texture.empty())original["texture"]=q(part.texture);else original.remove("texture");
    if(!part.motionId.empty())original["motionId"]=q(part.motionId);else original.remove("motionId");
    return original;
}
static Part readPart(const QJsonObject& obj) {
    Part part;
    part.id=s(obj.value("id"));part.name=s(obj.value("name"));part.group=s(obj.value("group"));
    part.material=s(obj.value("material"));part.texture=s(obj.value("texture"));
    part.motionId=s(obj.value("motionId"));part.visible=obj.value("visible").toBool(true);
    part.bounds={vec(obj.value("from")),vec(obj.value("to"))};
    if(!validBounds(part.bounds)) throw std::invalid_argument("A model part has non-finite or inverted bounds");
    if(part.group.empty())part.group="Static";
    if(part.material.empty())part.material="wood";
    if(part.name.empty())part.name="Cube";
    part.pivot=vec(obj.value("pivot"),(part.bounds.from+part.bounds.to)*0.5);
    part.rotationDegrees=vec(obj.value("rotation"));
    return part;
}
static QJsonObject defaultSettings() {
    return {{"inventory",true},{"power",false},{"fuel",false},{"fluid",false},
        {"inputs",QJsonArray{}},{"outputs",QJsonArray{}}};
}

QJsonObject ProjectIO::encode(const Project& p,const QJsonObject& original) {
    QJsonObject result=original;
    result["studioVersion"]=1;result["name"]=q(p.name);result["id"]=q(p.id);
    result["dimensions"]=array(p.dimensions);
    result["notes"]=q(p.notes);
    result["omittedAirInSource"]=p.omittedAirInSource;
    QJsonObject roles;
    for(const auto& [key,value]:p.palette)roles[q(key)]=q(value);
    result["palette"]=roles;
    QJsonObject cells;
    for(const auto& [coord,role]:p.cells)cells[q(cellString(coord))]=q(role);
    result["cells"]=cells;
    QJsonArray existing=result.value("parts").toArray();
    std::map<std::string,QJsonObject> metadata;
    for(const QJsonValue& value:existing) {
        auto obj=value.toObject();metadata[s(obj.value("id"))]=obj;
    }
    QJsonArray parts;
    for(const auto& part:p.parts)parts.append(partObject(part,metadata[part.id]));
    result["parts"]=parts;
    if(!result.value("motions").isArray())result["motions"]=QJsonArray{};
    if(!result.value("settings").isObject())result["settings"]=defaultSettings();
    return result;
}
static Project importLegacy(const QJsonObject& json) {
    const auto structure=json.value("structure").toObject();
    if(structure.isEmpty()) throw std::invalid_argument("Missing structure in legacy multiblock");
    const auto sourceDim=dimsFrom(structure.value("dimensions"));
    const auto rawName=s(json.value("name"));
    const std::string directions[]={"north","east","south","west"};
    Direction srcDir=Direction::North;
    std::string id=rawName;
    for(int i=0;i<4;i++) {
        const std::string suffix="_"+directions[i];
        if(id.size()>suffix.size() && id.compare(id.size()-suffix.size(),suffix.size(),suffix)==0) {
            id.resize(id.size()-suffix.size());srcDir=Direction(i);break;
        }
    }
    auto inverse = srcDir==Direction::East ? Direction::West : srcDir==Direction::West ? Direction::East : srcDir;
    Project p=makeProject(id.empty()?"Imported Machine":id,rotatedDimensions(sourceDim,inverse));
    p.id=slugify(id);p.cells.clear();p.palette.clear();
    const auto roles=structure.value("blocks").toObject();
    for(auto it=roles.begin();it!=roles.end();++it) p.palette[it.key().toStdString()]=s(it.value());
    p.omittedAirInSource=p.palette.find("Air")==p.palette.end();
    if(p.omittedAirInSource)p.palette["Air"]="minecraft:air";
    const auto layout=structure.value("layout").toObject();
    for(int y=0;y<sourceDim[1];y++) {
        auto rowSet=layout.value("layer"+QString::number(y+1)).toArray();
        if(rowSet.size()!=sourceDim[2])throw std::invalid_argument("Legacy structure has missing/incomplete layer");
        for(int z=0;z<sourceDim[2];z++) {
            const auto row=rowSet[z].toArray();
            if(row.size()!=sourceDim[0])throw std::invalid_argument("Legacy structure has an incomplete row");
            for(int x=0;x<sourceDim[0];x++) {
                std::string role=s(row[x]);
                if(role=="Air"||role.empty()) continue;
                if(p.palette.count(role)==0)throw std::invalid_argument("Unknown block role in legacy structure");
                p.cells[rotateCell({x,y,z},sourceDim,inverse)]=role;
            }
        }
    }
    return p;
}

bool ProjectIO::decode(const QByteArray& bytes,LoadResult& out,QString& error) {
    QJsonParseError parse;
    const auto doc=QJsonDocument::fromJson(bytes,&parse);
    if(doc.isNull() || !doc.isObject()) {
        error="Invalid JSON: "+parse.errorString();return false;
    }
    try {
        const QJsonObject root=doc.object();
        if(root.contains("structure")) {
            out.project=importLegacy(root);
            QJsonObject settings=root.value("settings").toObject();
            settings.remove("replacewhenformed");
            out.original=QJsonObject{{"settings",settings}};
            out.fromLegacy=true;return true;
        }
        Project p;
        p.dimensions=dimsFrom(root.value("dimensions"));
        p.name=s(root.value("name"));if(p.name.empty())p.name="Untitled Multiblock";
        p.id=slugify(s(root.value("id")).empty()?p.name:s(root.value("id")));
        p.notes=s(root.value("notes"));p.omittedAirInSource=root.value("omittedAirInSource").toBool(false);
        p.palette.clear();
        auto roles=root.value("palette").toObject();
        for(auto it=roles.begin();it!=roles.end();++it)p.palette[it.key().toStdString()]=s(it.value());
        if(p.palette.find("Air")==p.palette.end())p.palette["Air"]="minecraft:air";
        const auto cells=root.value("cells").toObject();
        for(auto it=cells.begin();it!=cells.end();++it) {
            Cell c;
            if(!parseCell(it.key().toStdString(),c)||!contains(p.dimensions,c))
                throw std::invalid_argument("A construction cell has invalid coordinates");
            if(!it.value().isString())throw std::invalid_argument("Construction role must be text");
            if(s(it.value())!="Air")p.cells[c]=s(it.value());
        }
        const auto parts=root.value("parts").toArray();
        for(const auto& element:parts) {
            if(!element.isObject())throw std::invalid_argument("Model part is not an object");
            auto b=readPart(element.toObject());
            if(b.id.empty())b.id=nextPartId(p);
            p.parts.push_back(b);
        }
        out.project=std::move(p);out.original=root;out.fromLegacy=false;
        error.clear();return true;
    } catch (const std::exception& e) {error=QString::fromUtf8(e.what());return false;}
}
bool ProjectIO::read(const QString& filename,LoadResult& out,QString& error) {
    QFile f(filename);
    if(!f.open(QIODevice::ReadOnly)){error=f.errorString();return false;}
    return decode(f.readAll(),out,error);
}
static bool writeJson(const QString& filename,const QJsonObject& data,QString& error) {
    QSaveFile file(filename);
    if(!file.open(QIODevice::WriteOnly)){error=file.errorString();return false;}
    const QByteArray bytes=QJsonDocument(data).toJson(QJsonDocument::Indented);
    if(file.write(bytes)!=bytes.size()) {error=file.errorString();file.cancelWriting();return false;}
    if(!file.commit()) {error=file.errorString();return false;}
    return true;
}
bool ProjectIO::write(const QString& filename,const Project& p,const QJsonObject& original,QString& error) {
    return writeJson(filename,encode(p,original),error);
}

bool ProjectIO::exportForgeDirectory(const QString& root,const Project& p,const QJsonObject& original,QString& error) {
    const auto [w,h,d]=p.dimensions;
    if(std::size_t(w)*std::size_t(h)*std::size_t(d)>150000) {
        error="Dense legacy construction JSON export is limited to 150,000 block cells";return false;
    }
    QDir dir(root);
    if(!dir.mkpath("data/magneticraft2/multiblocks")||
       !dir.mkpath("assets/magneticraft2/models/multiblock")) {
        error="Could not create export folders";return false;
    }
    QJsonObject manifest{{"format",2},{"editor","Magneticraft2 Studio Native"},
        {"id",q(p.id)},{"dimensions",array(p.dimensions)},
        {"rendererRequired",true}};
    QJsonArray files, warnings;
    const char* names[]={"north","east","south","west"};
    for(int i=0;i<4;i++){
        auto orientation=Direction(i);auto dims=rotatedDimensions(p.dimensions,orientation);
        // Retain legacy layer layout and key ordering expected by Forge.
        // Build a dense role buffer once, then serialize each row once. Qt's
        // implicitly-shared QJsonArray would otherwise copy rows repeatedly.
        const std::size_t cellCount=std::size_t(dims[0])*dims[1]*dims[2];
        std::vector<QString> roles(cellCount,QStringLiteral("Air"));
        for(const auto& [pos,role]:p.cells) {
            const Cell r=rotateCell(pos,p.dimensions,orientation);
            const auto offset=(std::size_t(r.y)*dims[2]+r.z)*dims[0]+r.x;
            roles[offset]=q(role);
        }
        QJsonObject layout;
        for(int y=0;y<dims[1];y++) {
            QJsonArray rows;
            for(int z=0;z<dims[2];z++){
                QJsonArray row;
                for(int x=0;x<dims[0];x++)row.append(roles[(std::size_t(y)*dims[2]+z)*dims[0]+x]);
                rows.append(row);
            }
            layout["layer"+QString::number(y+1)]=rows;
        }
        QJsonObject palette;
        for(const auto& [role,id]:p.palette)palette[q(role)]=q(id);
        // The source format can omit Air only if no empty cells are present.
        if(p.omittedAirInSource && std::size_t(p.cells.size())==std::size_t(w)*h*d)palette.remove("Air");
        QJsonObject settings=original.value("settings").toObject();
        if(settings.isEmpty())settings=defaultSettings();
        settings["replacewhenformed"]="multiblock/"+q(p.id)+"_"+names[i];
        QJsonObject output{{"name",q(p.id)+"_"+names[i]},
            {"structure",QJsonObject{{"dimensions",array(dims)},{"layout",layout},{"blocks",palette}}},
            {"settings",settings}};
        const QString filename=dir.filePath("data/magneticraft2/multiblocks/"+q(p.id)+"_"+names[i]+".json");
        if(!writeJson(filename,output,error))return false;
        files.append(dir.relativeFilePath(filename));
        // TODO: split large meshes into multiple baked model files and use a
        // formed-filler-cell renderer in the mod. We deliberately don't create
        // replacement models that would incorrectly display a large machine.
    }
    for(const auto& part:p.parts) {
        if(!part.visible)continue;
        try {
            // Verify the exact same geometry-slicing constraints as the current
            // Minecraft exporter. Model baking is a separate integration phase.
            auto chunks=sliceStaticPart(part,p.dimensions,Direction::North);
            if(chunks.empty() && part.motionId.empty())warnings.append(q(part.name)+": no geometry within the structure");
        }catch(const std::exception& e){warnings.append(q(part.name)+": "+QString::fromUtf8(e.what()));}
    }
    manifest["constructionFiles"]=files;
    manifest["warnings"]=warnings;
    manifest["staticModelExportStatus"]="not yet integrated with Forge filler renderer";
    return writeJson(dir.filePath("manifest.json"),manifest,error);
}

} // namespace studio
