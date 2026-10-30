#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

namespace studio {

struct Vec3 {
    double x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(double s) const { return {x*s,y*s,z*s}; }
};
struct Bounds { Vec3 from, to; };
struct Cell {
    int x = 0, y = 0, z = 0;
    bool operator==(const Cell& b) const { return x==b.x && y==b.y && z==b.z; }
};
struct CellHash {
    std::size_t operator()(Cell c) const noexcept {
        // Works for signed positions too; no lossy packing or 10-bit limit.
        auto mix=[](std::size_t h, std::uint32_t v) {
            return h ^ (std::size_t(v) + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
        };
        std::size_t h = 0;
        h = mix(h, std::uint32_t(c.x)); h = mix(h, std::uint32_t(c.y));
        return mix(h, std::uint32_t(c.z));
    }
};
// Face order: South (+Z), North (-Z), West (-X), East (+X), Up (+Y), Down (-Y).
// Coordinates are in texture tiles: 1.0 = the full imported PNG. When automatic,
// each world block maps to one texture tile (no accidental stretching).
struct FaceUV {
    bool enabled=true; // Java 'faces' may intentionally omit a side; legacy projects default visible.
    std::string texture; // Empty means inherit the cuboid texture.
    std::array<double,4> uv{0.0,0.0,1.0,1.0};
    bool automatic=true;
    int quarterTurns=0;
};
struct Part {
    std::string id;
    std::string name = "Cube";
    std::string group = "Static";
    std::string material = "wood";
    std::string texture = "";
    std::string motionId = "";
    std::array<FaceUV,6> faces{};
    Bounds bounds{{0,0,0},{1,1,1}};
    Vec3 pivot{0.5,0.5,0.5};
    Vec3 rotationDegrees{};
    bool visible = true;
};
struct Motion {
    std::string id;
    std::string type = "rotate";
    std::string axis = "y";
    Vec3 pivot{};
    double speed = 15.0;
    double amplitude = 0.1;
};
using Dimensions = std::array<int,3>;
using Cells = std::unordered_map<Cell, std::string, CellHash>;
enum class ProjectKind { Multiblock, JavaBlock };

struct Project {
    ProjectKind kind = ProjectKind::Multiblock;
    std::string name = "Untitled Multiblock";
    std::string id = "untitled_multiblock";
    Dimensions dimensions{5,3,5};
    std::map<std::string, std::string> palette;
    Cells cells;
    std::vector<Part> parts;
    // Embedded PNG bytes as base64: portable, offline and self-contained.
    // Texture keys can be referenced by Part.texture or FaceUV.texture.
    std::map<std::string,std::string> textures;
    std::vector<Motion> motions;
    std::string notes;
    bool omittedAirInSource = false;
};
enum class Direction { North, East, South, West };

std::string slugify(const std::string& name);
std::string cellString(Cell cell);
bool parseCell(const std::string& key, Cell& result);
bool contains(Dimensions size, Cell c);
void validateDimensions(Dimensions size);
void setCell(Project& p, Cell cell, const std::string& role);
void resize(Project& p, Dimensions size);
std::string nextPartId(const Project& p);
Part makeBox(const Project& p, const Bounds& bounds = {{0,0,0},{1,1,1}});
bool validBounds(Bounds box);
// Java block models use coordinates -16..32 model pixels relative to the
// block origin. Studio shows a positive 3x3x3 envelope, with the origin at (1,1,1).
bool javaBlockBoundsAllowed(Bounds box);
bool partBoundsAllowed(const Project& p, Bounds box);
double snapSixteenth(double x);
Dimensions rotatedDimensions(Dimensions dims, Direction direction);
Cell rotateCell(Cell cell, Dimensions original, Direction direction);
Bounds rotateBox(Bounds bounds, Dimensions original, Direction direction);
std::array<double,4> effectiveUV(const Part& part, int face);
void makeManualUV(Part& part,int face);
Project makeProject(const std::string& name, Dimensions dims);
Project makeJavaBlockProject(const std::string& name);
Project makeDemo();

struct SlicedCuboid { Cell cell; Bounds local; std::string partId; std::string texture; std::string material; };
std::vector<SlicedCuboid> sliceStaticPart(const Part& part, Dimensions dim, Direction direction, std::size_t maxCells = 150000);

} // namespace studio
