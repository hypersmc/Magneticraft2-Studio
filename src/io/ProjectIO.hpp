#pragma once
#include "core/Project.hpp"
#include <QJsonObject>
#include <QString>

namespace studio {

struct LoadResult {
    Project project;
    // Keep unknown JSON fields intact when reading Studio v1 projects.
    QJsonObject original;
    bool fromLegacy = false;
};

class ProjectIO {
public:
    static bool read(const QString& filename, LoadResult& out, QString& error);
    static bool decode(const QByteArray& bytes, LoadResult& out, QString& error);
    static QJsonObject encode(const Project& p, const QJsonObject& original = {});
    static bool write(const QString& filename, const Project& p, const QJsonObject& original, QString& error);
    // Exports construction JSON and a preview-only sliced model manifest. Forge
    // tile rendering must be implemented separately in Magneticraft2.
    static bool exportForgeDirectory(const QString& root, const Project& p, const QJsonObject& original, QString& error);
};

} // namespace studio
