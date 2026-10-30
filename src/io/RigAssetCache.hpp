#pragma once
#include "RigAssetLoader.hpp"
#include <QJsonObject>
#include <QString>
#include <deque>
#include <map>
#include <memory>

namespace studio {

// Read-only per-session cache of actual installed mod resources. Cached rigs
// are keyed by source identity, scene content and optional vanilla PNG source.
// A project can reopen the same machine without reading its JAR again.
// Never store copyrighted mod/client PNG data in application settings.
class RigAssetCache final {
public:
    static RigAssetCache& shared();
    std::shared_ptr<const RigAssets> get(const QString& source,
                                         const QString& machine,
                                         const QJsonObject& scene,
                                         const QString& vanillaSource,
                                         QString& error);
    void invalidate(const QString& source);
private:
    QString cacheKey(const QString& source,const QString& machine,
                     const QJsonObject& scene,const QString& vanilla) const;
    std::map<QString,std::shared_ptr<const RigAssets>> cached_;
    std::deque<QString> order_;
    // Keep all six current runtime-v1 machine examples warm in one session.
    // The cache is bounded, and serialized PNG/GL resources are not copied.
    static constexpr int kMaxCachedMachines=6;
};
} // namespace studio
