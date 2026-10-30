#pragma once
#include "core/Geometry.hpp"
#include "io/TextureAssets.hpp"
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <map>
#include <string>

namespace studio {

// Models are loaded from a user-selected, local Minecraft resource root.
// Never silently draw a fake machine part in place of an unavailable model.
struct RigAssets {
    std::map<std::string,Geometry> models;
    std::map<std::string,double> beltRepeats; // physical blocks per PNG U repeat
    Project textureProject;
    QStringList loadedModels;
    QStringList missingModels;
    QStringList missingTextures;
    QStringList warnings;
    QString error;
};

class RigAssetLoader final {
public:
    static bool load(const QString& assetRoot,const QJsonObject& scene,RigAssets& result,
                     const QString& javaMachine={});
    static TextureAssetReport addTextures(RigAssets& assets,const QString& source);
};
} // namespace studio
