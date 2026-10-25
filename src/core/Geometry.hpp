#pragma once
#include "Project.hpp"

namespace studio {

struct Vertex {
    float x, y, z;
    float r, g, b;
    float nx, ny, nz;
};
struct Rgb { float r,g,b; };
struct Geometry {
    std::vector<Vertex> triangles;
    std::vector<Vertex> lines;
    std::size_t visibleObjects=0;
};
struct BuildOptions {
    bool showConstruction=true;
    bool showFormed=true;
    bool ghostStructure=false;
    bool showGrid=true;
    int layer=-1; // -1 = all construction layers
    int selectedPart=-1;
};

Rgb materialColor(const std::string& material);
Vec3 rotateEuler(Vec3 point, Vec3 pivot, Vec3 rotationDegrees);
Bounds transformedBounds(const Part& part);
Geometry buildGeometry(const Project& document, BuildOptions options);

} // namespace studio
