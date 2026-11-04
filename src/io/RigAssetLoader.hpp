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
    // Import an installed mod's own binding + scene as a validated pair.
    // The selected .jar can differ from Studio's bundled v1 examples; never
    // silently combine a jar manifest with another machine's scene.
    static bool loadMachineDefinition(const QString& modJarOrAssetsRoot,
                                      const QString& machine,
                                      QJsonObject& manifest,QJsonObject& scene,
                                      QString& error);
    static bool load(const QString& assetRoot,const QJsonObject& scene,RigAssets& result,
                     const QString& javaMachine={});
    static TextureAssetReport addTextures(RigAssets& assets,const QString& source);
};
} // namespace studio
