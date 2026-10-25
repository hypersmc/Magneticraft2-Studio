#pragma once
#include "Project.hpp"

namespace studio {

// Construct a positive-volume cuboid from two corners drawn on a horizontal
// modelling plane. Drag direction is irrelevant and every edge snaps to the
// requested editor grid. Units are Minecraft blocks, NOT model pixels.
Bounds drawnCuboid(Vec3 first, Vec3 last, double planeY, double height, double gridStep);

// Snap a scene coordinate to one of the supported editor steps. Invalid or
// zero step values are intentionally rejected rather than silently accepted.
double snapGrid(double value, double gridStep);

} // namespace studio
