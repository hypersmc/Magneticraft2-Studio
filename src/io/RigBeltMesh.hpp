#pragma once
#include "core/Geometry.hpp"
#include <QJsonObject>
#include <QString>

namespace studio {

// CPU-side, once-per-scene build of the same open-belt tangent/arc path used
// by Java BeltPath.createVisualSegments. Texture U is measured in physical
// blocks/repeat. Animation changes the sampler phase, never stretches quads.
class RigBeltMesh {
public:
    static bool build(const QJsonObject& belt,Geometry& out,QString& error);
};
} // namespace studio
