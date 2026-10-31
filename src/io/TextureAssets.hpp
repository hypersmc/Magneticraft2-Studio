#pragma once

#include "core/Project.hpp"

#include <QString>
#include <QStringList>

namespace studio {

struct TextureAssetReport {
    QStringList imported;
    QStringList missing;
    QStringList alreadyPresent;
    QStringList warnings;
    QString error;
};

// Returns the exact Minecraft resource IDs referenced by formed geometry.
// Unnamed cuboids use Studio's materials and have no external dependency.
QStringList referencedTextureIds(const Project& project);
QStringList missingTextureIds(const Project& project);

// Import only model-referenced PNGs from a locally owned Minecraft client JAR,
// resource-pack ZIP/JAR, or unpacked assets directory. No internet, global
// installation, external unzip binary, or search of arbitrary disk paths.
// Existing, user-edited PNGs are never overwritten unless requested.
TextureAssetReport importTextureAssets(Project& project,
                                       const QString& sourcePath,
                                       bool overwrite = false);

} // namespace studio
