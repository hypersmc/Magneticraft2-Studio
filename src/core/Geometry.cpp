#include "Geometry.hpp"
#include <algorithm>
#include <array>
#include <cmath>

namespace studio {

Rgb materialColor(const std::string& material) {
    if(material=="wood") return {0.65f,0.44f,0.26f};
    if(material=="copper") return {0.77f,0.41f,0.28f};
    if(material=="stone") return {0.43f,0.48f,0.53f};
    if(material=="iron") return {0.67f,0.73f,0.76f};
    if(material=="dark") return {0.28f,0.34f,0.39f};
    if(material=="glass") return {0.35f,0.65f,0.63f};
    if(material=="water") return {0.19f,0.43f,0.70f};
    return {0.64f,0.54f,0.42f};
}
Vec3 rotateEuler(Vec3 point, Vec3 pivot, Vec3 degrees) {
    const auto radians=[](double d){return d*3.14159265358979323846/180.0;};
    double x=point.x-pivot.x, y=point.y-pivot.y, z=point.z-pivot.z;
    const double ax=radians(degrees.x), ay=radians(degrees.y), az=radians(degrees.z);
    double t=y*std::cos(ax)-z*std::sin(ax);z=y*std::sin(ax)+z*std::cos(ax);y=t;
    t=x*std::cos(ay)+z*std::sin(ay);z=-x*std::sin(ay)+z*std::cos(ay);x=t;
    t=x*std::cos(az)-y*std::sin(az);y=x*std::sin(az)+y*std::cos(az);x=t;
    return {x+pivot.x,y+pivot.y,z+pivot.z};
}
static std::array<Vec3,8> corners(Bounds b) {
    const auto a=b.from,c=b.to;
    return {{{a.x,a.y,a.z},{c.x,a.y,a.z},{c.x,c.y,a.z},{a.x,c.y,a.z},
        {a.x,a.y,c.z},{c.x,a.y,c.z},{c.x,c.y,c.z},{a.x,c.y,c.z}}};
}
Bounds transformedBounds(const Part& part) {
    auto points=corners(part.bounds);
    Bounds out{{1e100,1e100,1e100},{-1e100,-1e100,-1e100}};
    for(auto& p:points) {
        p=rotateEuler(p,part.pivot,part.rotationDegrees);
        out.from.x=std::min(out.from.x,p.x);out.from.y=std::min(out.from.y,p.y);out.from.z=std::min(out.from.z,p.z);
        out.to.x=std::max(out.to.x,p.x);out.to.y=std::max(out.to.y,p.y);out.to.z=std::max(out.to.z,p.z);
    }
    return out;
}
static void line(Geometry& mesh, Vec3 a, Vec3 b, Rgb color) {
    auto append=[&](Vec3 v){mesh.lines.push_back({float(v.x),float(v.y),float(v.z),color.r,color.g,color.b,0,0,0});};
    append(a);append(b);
}
static void drawEdges(Geometry& mesh, const std::array<Vec3,8>& points,Rgb color) {
    constexpr int edges[12][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},
        {0,4},{1,5},{2,6},{3,7}};
    for(const auto& e:edges)line(mesh,points[e[0]],points[e[1]],color);
}
// Face order with outward normals for counter-clockwise triangles.
static constexpr int faces[6][4]={{4,5,6,7},{1,0,3,2},{0,4,7,3},
    {5,1,2,6},{3,7,6,2},{0,1,5,4}};
static void drawCube(Geometry& mesh,const std::array<Vec3,8>& pts,Rgb color,
    const std::array<bool,6>& visible) {
    for(int face=0;face<6;face++) {
        if(!visible[face])continue;
        const auto& idx=faces[face];
        Vec3 a=pts[idx[0]],b=pts[idx[1]],c=pts[idx[2]];
        const Vec3 u=b-a,v=c-a;
        Vec3 n{u.y*v.z-u.z*v.y,u.z*v.x-u.x*v.z,u.x*v.y-u.y*v.x};
        const double inv=1.0/std::max(1e-9,std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z));
        n=n*inv;
        const auto add=[&](int i) {
            const Vec3 p=pts[idx[i]];
            mesh.triangles.push_back({float(p.x),float(p.y),float(p.z),
                color.r,color.g,color.b,float(n.x),float(n.y),float(n.z)});
        };
        add(0);add(1);add(2);add(0);add(2);add(3);
    }
}
Geometry buildGeometry(const Project& p,BuildOptions options) {
    Geometry result;
    const auto& dim=p.dimensions;
    if(options.showGrid) {
        const Rgb faint={0.23f,0.29f,0.34f};
        for(int x=0;x<=dim[0];x++)line(result,{double(x),-0.012,0},{double(x),-0.012,double(dim[2])},faint);
        for(int z=0;z<=dim[2];z++)line(result,{0,-0.012,double(z)},{double(dim[0]),-0.012,double(z)},faint);
        const Rgb axis={0.42f,0.52f,0.65f};
        line(result,{0,0,0},{double(dim[0]),0,0},axis);
        line(result,{0,0,0},{0,double(dim[1]),0},{0.35f,0.76f,0.46f});
        line(result,{0,0,0},{0,0,double(dim[2])},{0.71f,0.45f,0.44f});
    }
    if(options.showConstruction) {
        result.triangles.reserve(result.triangles.size()+p.cells.size()*24);
        for(const auto& [coord,role] : p.cells) {
            if(options.layer>=0 && coord.y!=options.layer)continue;
            const Vec3 from{double(coord.x),double(coord.y),double(coord.z)};
            const Vec3 to=from+Vec3{1,1,1};
            const std::array<Cell,6> neighbors{{{coord.x,coord.y,coord.z+1},
                {coord.x,coord.y,coord.z-1},{coord.x-1,coord.y,coord.z},
                {coord.x+1,coord.y,coord.z},{coord.x,coord.y+1,coord.z},
                {coord.x,coord.y-1,coord.z}}};
            std::array<bool,6> visible{};
            for(int i=0;i<6;i++){
                auto it=p.cells.find(neighbors[i]);
                visible[i]=(it==p.cells.end() || (options.layer>=0 && neighbors[i].y!=options.layer));
            }
            Rgb color;
            if(role=="Controller")color={0.77f,0.59f,0.32f};
            else if(role.find("Fluid")!=std::string::npos)color={0.21f,0.60f,0.75f};
            else if(role.find("Item")!=std::string::npos)color={0.40f,0.73f,0.46f};
            else if(role.find("Mechanical")!=std::string::npos)color={0.79f,0.40f,0.28f};
            else color=materialColor("wood");
            if(options.ghostStructure)color={color.r*0.45f,color.g*0.45f,color.b*0.45f};
            drawCube(result,corners({from,to}),color,visible);
            ++result.visibleObjects;
        }
    }
    if(options.showFormed) {
        for(std::size_t i=0;i<p.parts.size();i++) {
            const auto& part=p.parts[i];
            if(!part.visible || !validBounds(part.bounds))continue;
            auto pts=corners(part.bounds);
            for(auto& v:pts)v=rotateEuler(v,part.pivot,part.rotationDegrees);
            drawCube(result,pts,materialColor(part.material),{true,true,true,true,true,true});
            if(int(i)==options.selectedPart) {
                drawEdges(result,pts,{1.0f,0.79f,0.27f});
                const Vec3 pivot=part.pivot;
                const double length=std::max(0.6,(part.bounds.to.x-part.bounds.from.x)*0.45);
                line(result,pivot,pivot+Vec3{length,0,0},{0.98f,0.33f,0.29f});
                line(result,pivot,pivot+Vec3{0,length,0},{0.28f,0.95f,0.44f});
                line(result,pivot,pivot+Vec3{0,0,length},{0.35f,0.55f,1.0f});
            }
            ++result.visibleObjects;
        }
    }
    return result;
}

} // namespace studio
