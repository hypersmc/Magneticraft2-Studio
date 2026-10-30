#include "RigAssetCache.hpp"
#include <QCryptographicHash>
#include <QFileInfo>
#include <QJsonDocument>
#include <algorithm>
#include <utility>

namespace studio {
RigAssetCache& RigAssetCache::shared(){
    static RigAssetCache cache;return cache;
}
QString RigAssetCache::cacheKey(const QString& source,const QString& machine,
                               const QJsonObject& scene,const QString& vanilla) const{
    const auto fingerprint=[](const QString& path){
        QFileInfo info(path);
        return info.absoluteFilePath()+":"+QString::number(info.size())+":"+
            QString::number(info.lastModified().toMSecsSinceEpoch());
    };
    // Meshes depend on model IDs and belt geometry, not animation tracks,
    // pivot values, or live shaft angles. Authoring a rotation must not reread
    // or rebuild every baked model from the JAR.
    QJsonObject models;
    const auto components=scene.value("components").toObject();
    for(auto it=components.begin();it!=components.end();++it){
        const auto part=it.value().toObject();
        models[it.key()]=QJsonObject{{"model",part.value("model")},
                                    {"model_variants",part.value("model_variants")}};
    }
    const QJsonObject geometryKey{{"models",models},
                                  {"belt_loops",scene.value("belt_loops")}};
    const auto hash=QCryptographicHash::hash(
        QJsonDocument(geometryKey).toJson(QJsonDocument::Compact),
        QCryptographicHash::Sha256).toHex();
    return fingerprint(source)+"|"+machine+"|"+QString::fromLatin1(hash)+"|"+
           (vanilla.isEmpty()?QString():fingerprint(vanilla));
}
std::shared_ptr<const RigAssets> RigAssetCache::get(const QString& source,
                                                     const QString& machine,
                                                     const QJsonObject& scene,
                                                     const QString& vanillaSource,
                                                     QString& error){
    error.clear();
    if(!QFileInfo::exists(source)){
        error="Previously selected mod asset source is missing: "+source;
        return {};
    }
    const QString key=cacheKey(source,machine,scene,vanillaSource);
    const auto found=cached_.find(key);
    if(found!=cached_.end()){
        order_.erase(std::remove(order_.begin(),order_.end(),key),order_.end());
        order_.push_back(key);
        return found->second;
    }
    RigAssets assets;
    if(!RigAssetLoader::load(source,scene,assets,machine)){
        error=assets.error;return {};
    }
    if(!vanillaSource.isEmpty()){
        if(QFileInfo::exists(vanillaSource)){
            const auto extra=RigAssetLoader::addTextures(assets,vanillaSource);
            if(!extra.error.isEmpty())assets.warnings.append("Vanilla PNG source: "+extra.error);
        }else{
            assets.warnings.append("Saved Minecraft texture source is missing: "+vanillaSource);
        }
    }
    auto stored=std::make_shared<const RigAssets>(std::move(assets));
    cached_[key]=stored;
    order_.push_back(key);
    while(order_.size()>kMaxCachedMachines){
        cached_.erase(order_.front());
        order_.pop_front();
    }
    return stored;
}
void RigAssetCache::invalidate(const QString& source){
    const QString prefix=QFileInfo(source).absoluteFilePath()+":";
    for(auto it=cached_.begin();it!=cached_.end();){
        if(it->first.startsWith(prefix)){
            const QString key=it->first;
            it=cached_.erase(it);
            order_.erase(std::remove(order_.begin(),order_.end(),key),order_.end());
        }else ++it;
    }
}
} // namespace studio
