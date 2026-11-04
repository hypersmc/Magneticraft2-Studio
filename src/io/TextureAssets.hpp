#pragma once

#include "core/Project.hpp"

#include <QString>
#include <QStringList>
#include <QByteArray>
#include <memory>

namespace studio {

// Local, read-only Minecraft asset source (resource root or ZIP/JAR).
// Opens an archive just once; model JSON and PNG imports share its validated
// central directory, decompression limits and CRC checks. No file extraction
// to arbitrary paths, network access, or dependence on mod source checkouts.
class AssetSource final {
public:
    AssetSource();
    ~AssetSource();
    AssetSource(const AssetSource&)=delete;
    AssetSource& operator=(const AssetSource&)=delete;
    bool open(const QString& sourcePath,QString& error);
    // Missing entry returns false with an empty error.
    // Invalid or unsafe entry returns false with a diagnostic.
    bool read(const QString& resourcePath,QByteArray& contents,
              QString& error,qsizetype maxBytes=8*1024*1024) const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

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
