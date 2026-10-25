#include "Project.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <sstream>

namespace studio {

std::string slugify(const std::string& name) {
    std::string result;
    for(unsigned char c : name) {
        const bool valid = (c>='a' && c<='z') || (c>='0' && c<='9');
        if(valid) result.push_back(char(c));
        else if(c>='A' && c<='Z') result.push_back(char(c+('a'-'A')));
        else if(result.empty() || result.back()!='_') result.push_back('_');
    }
    while(!result.empty() && result.back()=='_') result.pop_back();
    return result.empty() ? "machine" : result;
}
std::string cellString(Cell c) { return std::to_string(c.x)+","+std::to_string(c.y)+","+std::to_string(c.z); }
bool parseCell(const std::string& key, Cell& c) {
    std::istringstream in(key); char a=0,b=0,extra=0;
    int x,y,z;
    if(!(in>>x>>a>>y>>b>>z) || a!=',' || b!=',' || (in>>extra)) return false;
    c={x,y,z}; return true;
}
bool contains(Dimensions dim, Cell c) { return c.x>=0 && c.y>=0 && c.z>=0 && c.x<dim[0] && c.y<dim[1] && c.z<dim[2]; }
void validateDimensions(Dimensions size) {
    for(int axis : size) if(axis<1 || axis>128) throw std::invalid_argument("Dimensions must be between 1 and 128 blocks");
}
void setCell(Project& p, Cell cell, const std::string& role) {
    if(!contains(p.dimensions, cell)) throw std::out_of_range("Cell outside of multiblock");
    if(role.empty() || role=="Air") p.cells.erase(cell);
    else p.cells[cell] = role;
}
void resize(Project& p, Dimensions size) {
    validateDimensions(size);
    p.dimensions = size;
    for(auto it=p.cells.begin(); it!=p.cells.end();) {
        if(!contains(size,it->first)) it=p.cells.erase(it);
        else ++it;
    }
    // A resized structure must still have a valid formation controller.
    const bool hasController = std::any_of(p.cells.begin(), p.cells.end(),
        [](const auto& entry){return entry.second=="Controller";});
    if(!hasController) p.cells[{size[0]/2,0,size[2]/2}] = "Controller";
    // Geometry is kept even if it extends beyond the construction bounds. Export
    // will warn/clip; a resize never silently destroys the artist's work.
}
std::string nextPartId(const Project& p) {
    for(std::size_t n=1;;++n) {
        std::string id="part_"+std::to_string(n);
        if(std::none_of(p.parts.begin(),p.parts.end(),[&](const Part& b){return b.id==id;})) return id;
    }
}
Part makeBox(const Project& p, const Bounds& bounds) {
    if(!validBounds(bounds)) throw std::invalid_argument("Invalid cuboid bounds");
    Part b; b.id=nextPartId(p); b.bounds=bounds;
    b.pivot=(bounds.from+bounds.to)*0.5;return b;
}
bool validBounds(Bounds box) {
    const auto finite=[](Vec3 v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);};
    return finite(box.from)&&finite(box.to) && box.from.x<box.to.x && box.from.y<box.to.y && box.from.z<box.to.z;
}
double snapSixteenth(double x) { return std::round(x*16.0)/16.0; }
Dimensions rotatedDimensions(Dimensions dims, Direction dir) {
    if(dir==Direction::East || dir==Direction::West) std::swap(dims[0],dims[2]);
    return dims;
}
Cell rotateCell(Cell c, Dimensions dim, Direction dir) {
    switch(dir) {
        case Direction::East: return {dim[2]-1-c.z,c.y,c.x};
        case Direction::South: return {dim[0]-1-c.x,c.y,dim[2]-1-c.z};
        case Direction::West: return {c.z,c.y,dim[0]-1-c.x};
        default: return c;
    }
}
static Vec3 rotatePoint(Vec3 v, Dimensions d, Direction dir) {
    switch(dir) {
        case Direction::East: return {double(d[2])-v.z,v.y,v.x};
        case Direction::South: return {double(d[0])-v.x,v.y,double(d[2])-v.z};
        case Direction::West: return {v.z,v.y,double(d[0])-v.x};
        default: return v;
    }
}
Bounds rotateBox(Bounds box, Dimensions d, Direction dir) {
    const auto a=rotatePoint(box.from,d,dir),b=rotatePoint(box.to,d,dir);
    return {{std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)},
            {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}};
}
Project makeProject(const std::string& name, Dimensions dims) {
    validateDimensions(dims);
    Project p; p.name=name; p.id=slugify(name); p.dimensions=dims;
    p.palette={{"Air","minecraft:air"},{"Frame","minecraft:oak_planks"},
        {"Controller","magneticraft2:mechanical_ore_washer"},
        {"MechanicalInput","magneticraft2:mechanical_input_module"},
        {"ItemInput","magneticraft2:multiblock_item_input"},
        {"ItemOutput","magneticraft2:multiblock_item_output"},
        {"FluidInput","magneticraft2:multiblock_fluid_input"},
        {"FluidOutput","magneticraft2:multiblock_fluid_output"}};
    p.cells[{dims[0]/2,0,dims[2]/2}]="Controller";
    return p;
}
Project makeDemo() {
    Project p=makeProject("Workshop Ore Washer",{5,3,5});
    for(int x=0;x<5;x++) for(int z=0;z<5;z++) {
        if(x==0||x==4||z==0||z==4) setCell(p,{x,0,z},"Frame");
    }
    for(int x : {0,4}) for(int z : {0,4}) for(int y : {1,2}) setCell(p,{x,y,z},"Frame");
    setCell(p,{2,0,2},"Controller"); setCell(p,{2,0,0},"ItemInput");
    setCell(p,{2,0,4},"ItemOutput"); setCell(p,{0,0,2},"MechanicalInput");
    setCell(p,{4,0,2},"FluidInput");
    auto add=[&](const char* name, Bounds b,const char* material,const char* group="Frame") {
        auto part=makeBox(p,b);part.name=name;part.material=material;part.group=group;p.parts.push_back(part);
    };
    for(double x : {0.2,4.48}) for(double z : {0.2,4.48})
        add("Timber post",{{x,0,z},{x+0.32,2.85,z+0.32}},"wood");
    for(double y : {0.28,2.6}) for(double z : {0.2,4.48})
        add("Cross beam",{{0.2,y,z},{4.8,y+0.24,z+0.32}},"wood");
    add("Lower trough",{{0.65,0.25,0.75},{4.35,0.45,4.25}},"copper");
    add("Water surface",{{0.75,0.46,0.85},{4.25,0.48,4.15}},"water","Water");
    add("Drum",{{1.25,1.05,1.15},{3.75,2.15,3.85}},"copper","Rotor");
    add("Main shaft",{{0.4,1.57,2.38},{4.6,1.72,2.62}},"iron","Rotor");
    add("Output chute",{{1.7,0.46,4.15},{3.3,0.67,5.0}},"wood");
    return p;
}
std::vector<SlicedCuboid> sliceStaticPart(const Part& part, Dimensions original, Direction dir, std::size_t maxCells) {
    validateDimensions(original);
    if(!validBounds(part.bounds)) throw std::invalid_argument("Part has invalid bounds");
    if(part.motionId!="") return {}; // Animated geometry belongs in a separate renderer.
    if(part.rotationDegrees.x!=0 || part.rotationDegrees.y!=0 || part.rotationDegrees.z!=0)
        throw std::invalid_argument("Rotated cuboids need a dedicated mesh exporter; cannot silently flatten rotation");
    const auto bound=rotateBox(part.bounds,original,dir);
    const auto dim=rotatedDimensions(original,dir);
    const int x0=std::max(0,int(std::floor(bound.from.x))), x1=std::min(dim[0],int(std::ceil(bound.to.x)));
    const int y0=std::max(0,int(std::floor(bound.from.y))), y1=std::min(dim[1],int(std::ceil(bound.to.y)));
    const int z0=std::max(0,int(std::floor(bound.from.z))), z1=std::min(dim[2],int(std::ceil(bound.to.z)));
    const std::size_t volume=std::size_t(std::max(0,x1-x0))*std::size_t(std::max(0,y1-y0))*std::size_t(std::max(0,z1-z0));
    if(volume>maxCells) throw std::length_error("Part covers too many block cells");
    std::vector<SlicedCuboid> out; out.reserve(volume);
    for(int y=y0;y<y1;y++) for(int z=z0;z<z1;z++) for(int x=x0;x<x1;x++) {
        const Bounds clip{{std::max(bound.from.x,double(x)),std::max(bound.from.y,double(y)),std::max(bound.from.z,double(z))},
            {std::min(bound.to.x,double(x+1)),std::min(bound.to.y,double(y+1)),std::min(bound.to.z,double(z+1))}};
        if(clip.from.x>=clip.to.x || clip.from.y>=clip.to.y || clip.from.z>=clip.to.z) continue;
        out.push_back({{x,y,z}, {clip.from-Vec3{double(x),double(y),double(z)},
            clip.to-Vec3{double(x),double(y),double(z)}},part.id,part.texture,part.material});
    }
    return out;
}

} // namespace studio
