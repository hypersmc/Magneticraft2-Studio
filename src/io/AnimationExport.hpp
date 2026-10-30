#pragma once

#include "core/Project.hpp"

#include <QJsonObject>
#include <QString>

namespace studio {

/**
 * Export-only bridge to the Magneticraft2 Forge 1.20.1 animation runtime.
 *
 * Studio v0.5 preserves unknown part metadata and motions in a native project.
 * This exporter consumes explicit per-part animation bindings and validated
 * typed motion definitions. It does NOT generate geometry or claim the editor
 * already simulates animation in the viewport.
 */
class AnimationExport {
public:
    static bool build(const Project& project, const QJsonObject& original,
                      QJsonObject& animation, QString& error);
    static bool write(const QString& filename, const Project& project,
                      const QJsonObject& original, QString& error);
};

} // namespace studio
