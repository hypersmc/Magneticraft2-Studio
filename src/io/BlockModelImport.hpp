#pragma once
#include "core/Project.hpp"
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <vector>

namespace studio {

struct BlockModelImportResult {
    std::vector<Part> parts;
    QStringList warnings;
    QString error;
};

// Converts vanilla/Forge Java block model `elements` (pixel coordinates) into
// editable Studio cuboids. Doesn't claim to import unresolved parent models,
// custom mesh loaders, or Minecraft-specific face culling semantics.
BlockModelImportResult importBlockModel(const QByteArray& json, const Project& project,
                                      const QString& group, Vec3 anchor);
}
