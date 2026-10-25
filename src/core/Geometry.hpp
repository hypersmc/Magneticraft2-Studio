#pragma once
#include "Project.hpp"

namespace studio {

struct Vertex {
    float x, y, z;
    float r, g, b;
    float nx, ny, nz;
    float u, v;
};
struct Rgb { float r,g,b; };
struct TextureDraw {
    std::size_t first=0, count=0;
    std::string texture; // Project texture ID (or Minecraft resource reference)
    std::string material; // Fallback pixel texture when no embedded PNG is available
    bool useTexture=false;
};
struct Geometry {
    std::vector<Vertex> triangles;
    std::vector<Vertex> lines;
    std::vector<TextureDraw> draws;
    std::size_t visibleObjects=0;
};
struct BuildOptions {
    bool showConstruction=true;
    bool showFormed=true;
    bool ghostStructure=false;
    bool showGrid=true;
    int layer=-1; // -1 = all construction layers
    int selectedPart=-1;
    double gridY=0.0; // Modeling plane; independent from construction layer
};

Rgb materialColor(const std::string& material);
Vec3 rotateEuler(Vec3 point, Vec3 pivot, Vec3 rotationDegrees);
Bounds transformedBounds(const Part& part);
Geometry buildGeometry(const Project& document, BuildOptions options);

} // namespace studio
