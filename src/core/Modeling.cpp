#include "Modeling.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace studio {

double snapGrid(double value, double step) {
    if(!std::isfinite(value) || !std::isfinite(step) || step<=0.0)
        throw std::invalid_argument("Grid position/step must be finite and step positive");
    return std::round(value/step)*step;
}

Bounds drawnCuboid(Vec3 first, Vec3 last, double planeY, double height, double step) {
    if(!std::isfinite(height) || height<=0 || !std::isfinite(planeY))
        throw std::invalid_argument("Drawn cuboid height and plane must be valid");
    const auto x1=snapGrid(first.x,step), x2=snapGrid(last.x,step);
    const auto z1=snapGrid(first.z,step), z2=snapGrid(last.z,step);
    const auto y=snapGrid(planeY,step);
    const auto h=std::max(step,snapGrid(height,step));
    const auto minX=std::min(x1,x2),minZ=std::min(z1,z2);
    const auto maxX=std::max(x1,x2),maxZ=std::max(z1,z2);
    // Clicking without a drag still creates a single grid unit cube.
    return {{minX,y,minZ},{maxX>minX?maxX:minX+step,y+h,maxZ>minZ?maxZ:minZ+step}};
}

} // namespace studio
