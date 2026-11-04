#include "RigAssetLoader.hpp"
#include "io/BlockModelImport.hpp"
#include "io/RigBeltMesh.hpp"
#include "io/VisualContract.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QImage>
#include <QSet>
#include <QHash>
#include <algorithm>
#include <utility>

namespace studio {
namespace {

bool resourceId(const QString& id){
    static const QRegularExpression pattern("^[a-z0-9_.-]+:[a-z0-9_./-]+$");
    return id.size()<256 && !id.contains("..") && pattern.match(id).hasMatch();
}
QString resourcePath(const QString& resource){
    const auto ns=resource.section(':',0,0),path=resource.section(':',1);
    return "assets/"+ns+"/models/"+path+".json";
}
class Resolver {
public:
    explicit Resolver(const AssetSource& source):source_(source){}
    bool get(const QString& id,QJsonObject& resolved,QString& reason,int depth=0){
        if(!resourceId(id)){reason="Invalid model resource: "+id;return false;}
        if(depth>32){reason="Model inheritance exceeds 32 levels: "+id;return false;}
        if(cache_.contains(id)){resolved=cache_.value(id);return true;}
        if(visiting_.contains(id)){reason="Cyclic model inheritance at "+id;return false;}
        visiting_.insert(id);
        QByteArray bytes;QString readError;
        if(!source_.read(resourcePath(id),bytes,readError,4*1024*1024)){
            reason=readError.isEmpty()?"Missing baked model "+id:
                "Cannot read model "+id+": "+readError;
            visiting_.remove(id);return false;
        }
        QJsonParseError parse;
        const auto doc=QJsonDocument::fromJson(bytes,&parse);
        if(parse.error!=QJsonParseError::NoError || !doc.isObject()){
            reason="Invalid model JSON "+id+": "+parse.errorString();
            visiting_.remove(id);return false;
        }
        const auto child=doc.object();
        QJsonObject base;
        if(child.value("parent").isString()){
            QString parent=child.value("parent").toString();
            if(!parent.contains(':'))parent.prepend("minecraft:");
            if(!get(parent,base,reason,depth+1)){
                reason="Model "+id+" inherits unresolved "+parent+": "+reason;
                visiting_.remove(id);return false;
            }
        }
        // Minecraft's model elements inherit from the parent when a child has
        // no elements; a child texture slot overrides a parent texture slot.
        // Preserve inherited aliases so BlockModelImport can resolve all
        // per-face #texture chains using the final material map.
        QJsonObject textures=base.value("textures").toObject();
        const auto overrides=child.value("textures").toObject();
        for(auto it=overrides.begin();it!=overrides.end();++it)textures[it.key()]=it.value();
        if(child.contains("elements"))base["elements"]=child.value("elements");
        base["textures"]=textures;
        if(child.contains("ambientocclusion"))base["ambientocclusion"]=child.value("ambientocclusion");
        visiting_.remove(id);
        cache_[id]=base;
        resolved=base;return true;
    }
private:
    const AssetSource& source_;
    QHash<QString,QJsonObject> cache_;
    QSet<QString> visiting_;
};

QStringList modelsIn(const QJsonObject& scene){
    QSet<QString> required;
    const auto components=scene.value("components").toObject();
    for(auto it=components.begin();it!=components.end();++it){
        const auto c=it.value().toObject();
        if(c.value("model").isString())required.insert(c.value("model").toString());
        const auto variants=c.value("model_variants").toObject().value("models").toObject();
        for(auto v=variants.begin();v!=variants.end();++v)
            if(v.value().isString())required.insert(v.value().toString());
    }
    auto ids=required.values();ids.sort();return ids;
}
}
bool RigAssetLoader::loadMachineDefinition(const QString& sourcePath,
                                          const QString& machine,
                                          QJsonObject& manifest,QJsonObject& scene,
                                          QString& error){
    AssetSource asset;
    if(!asset.open(sourcePath,error))return false;
    const QString manifestPath="assets/magneticraft2/machine_visuals/"+machine+".json";
    QByteArray manifestBytes;
    if(!asset.read(manifestPath,manifestBytes,error,2*1024*1024)){
        if(error.isEmpty())error="The selected mod JAR has no runtime-v1 machine manifest: "+manifestPath;
        return false;
    }
    QJsonObject binding;
    if(!VisualContract::parseObject(manifestBytes,binding,error))return false;
    if(!VisualContract::validateManifest(binding,machine,error))return false;
    const QString sceneId=binding.value("animation").toString();
    const QString scenePath="assets/"+sceneId.section(':',0,0)+
                            "/animations/"+sceneId.section(':',1)+".json";
    QByteArray sceneBytes;
    if(!asset.read(scenePath,sceneBytes,error,2*1024*1024)){
        if(error.isEmpty())error="The selected mod JAR has no animation scene: "+scenePath;
        return false;
    }
    return VisualContract::importPair(machine,manifestBytes,sceneBytes,
                                      manifest,scene,error);
}
bool RigAssetLoader::load(const QString& selected,const QJsonObject& scene,RigAssets& output,
                          const QString& javaMachine){
    RigAssets assets;
    AssetSource source;QString readError;
    if(!source.open(selected,readError)){
        output.error=readError;return false;
    }
    // Load all models from the distributed mod JAR or an unpacked assets root.
    // The user never needs to download the Forge source checkout.
    Resolver resolver(source);
    assets.textureProject=makeProject("Animated model assets",{5,5,5});
    assets.textureProject.cells.clear();
    assets.textureProject.parts.clear();
    for(const auto& id:modelsIn(scene)){
        QJsonObject source;QString error;
        if(!resolver.get(id,source,error)){
            assets.missingModels.append(id);
            assets.warnings.append(error);continue;
        }
        Project isolated=makeProject("Model "+id.toStdString(),{5,5,5});
        isolated.cells.clear();isolated.parts.clear();
        const auto imported=importBlockModel(QJsonDocument(source).toJson(QJsonDocument::Compact),
                                              isolated,id,Vec3{0,0,0});
        if(!imported.error.isEmpty()){
            assets.missingModels.append(id);
            assets.warnings.append(id+": "+imported.error);
            continue;
        }
        isolated.parts=imported.parts;
        for(const auto& p:imported.parts)
            assets.textureProject.parts.push_back(p);
        BuildOptions options;
        options.showGrid=false;options.showConstruction=false;
        options.showFormed=true;options.selectedPart=-1;
        assets.models[id.toStdString()]=buildGeometry(isolated,options);
        assets.loadedModels.append(id);
        for(const auto& warning:imported.warnings)
            assets.warnings.append(id+": "+warning);
    }
    auto textures=importTextureAssets(assets.textureProject,selected);
    if(!textures.error.isEmpty())assets.warnings<<textures.error;
    assets.missingTextures=textures.missing;
    assets.warnings.append(textures.warnings);
    // Belt PNG is a *full direct file ID*, unlike Minecraft atlas aliases.
    // Only the Stamp Mill Java renderer opts in to generic belt loops today.
    const auto belts=scene.value("belt_loops").toObject();
    for(auto it=belts.begin();it!=belts.end();++it){
        const auto data=it.value().toObject();
        const QString full=data.value("texture").toString();
        if(javaMachine!="bronze_stamp_mill"){
            assets.warnings<<"Java "+javaMachine+" does not draw generic belt loops: "+it.key();
            continue;
        }
        const QString relative="assets/"+full.section(':',0,0)+"/"+full.section(':',1);
        QByteArray png;QString pngError;
        if(!source.read(relative,png,pngError,8*1024*1024)){
            assets.warnings<<"Direct belt PNG unavailable "+full+
                (pngError.isEmpty()?QString():(": "+pngError));
            continue; // Never generate fake textures for missing assets.
        }
        const QImage decoded=QImage::fromData(png,"PNG");
        if(decoded.isNull()||decoded.width()>4096||decoded.height()>4096){
            assets.warnings<<"Invalid direct belt PNG "+full;
            continue;
        }
        Geometry beltMesh;QString beltError;
        if(!RigBeltMesh::build(data,beltMesh,beltError)){
            assets.warnings<<"Unable to build "+it.key()+" belt: "+beltError;
            continue;
        }
        const std::string key="__belt/"+it.key().toStdString();
        assets.models[key]=std::move(beltMesh);
        assets.beltRepeats[it.key().toStdString()]=data.value("texture_repeat").toDouble();
        qsizetype allocated=0;
        for(const auto& [id,bytes]:assets.textureProject.textures){
            Q_UNUSED(id);
            allocated+=qsizetype(bytes.size()*3/4);
        }
        if(assets.textureProject.textures.size()>=256 ||
           allocated+png.size()>36LL*1024*1024){
            assets.models.erase(key);
            assets.beltRepeats.erase(it.key().toStdString());
            assets.warnings<<"Direct belt PNG would exceed the project texture budget: "+full;
            continue;
        }
        assets.textureProject.textures[full.toStdString()]=png.toBase64().toStdString();
    }
    output=std::move(assets);
    output.error.clear();
    return true;
}
TextureAssetReport RigAssetLoader::addTextures(RigAssets& assets,const QString& source){
    auto report=importTextureAssets(assets.textureProject,source);
    assets.missingTextures=report.missing;
    assets.warnings.append(report.warnings);
    return report;
}
} // namespace studio
