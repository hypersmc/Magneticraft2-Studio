#include "core/Project.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace studio;
static bool close(double a,double b) {return std::abs(a-b)<1e-8;}

int main() {
    auto p=makeProject("My Giant Multiblock",{16,12,20});
    assert(p.id=="my_giant_multiblock" && p.dimensions[2]==20);
    assert(p.cells.size()==1);
    setCell(p,{15,11,19},"Frame");
    assert(p.cells.at({15,11,19})=="Frame");
    setCell(p,{15,11,19},"Air"); assert(p.cells.size()==1);
    bool threw=false;
    try { setCell(p,{16,0,0},"Frame"); } catch (const std::out_of_range&) {threw=true;}
    assert(threw);
    for(const auto dir : {Direction::North,Direction::East,Direction::South,Direction::West}) {
        const auto rotated=rotatedDimensions({7,5,9},dir);
        assert(rotated[1]==5);
        auto c=rotateCell({3,1,4},{7,5,9},dir);
        assert(contains(rotated,c));
    }
    assert((rotatedDimensions({7,5,9},Direction::East)==Dimensions{9,5,7}));
    assert((rotateCell({0,0,0},{7,5,9},Direction::East)==Cell{8,0,0}));
    assert((rotateCell({0,0,0},{7,5,9},Direction::West)==Cell{0,0,6}));
    assert((rotateCell({0,0,0},{7,5,9},Direction::South)==Cell{6,0,8}));
    auto b=makeBox(p,{{1.5,0.0,2.0},{4.5,1.5,5.25}});
    p.parts.push_back(b);
    assert(b.id=="part_1"); assert(nextPartId(p)=="part_2");
    const auto slices=sliceStaticPart(b,p.dimensions,Direction::North);
    assert(slices.size()==32); // 4 x 2 x 4 cells
    for(const auto& s:slices) {
        assert(validBounds(s.local));
        assert(s.local.from.x>=0 && s.local.to.x<=1);
        assert(s.local.from.y>=0 && s.local.to.y<=1);
        assert(s.local.from.z>=0 && s.local.to.z<=1);
    }
    auto east=sliceStaticPart(b,p.dimensions,Direction::East);
    assert(east.size()==32);
    auto rotated=rotateBox(b.bounds,p.dimensions,Direction::East);
    assert(close(rotated.from.x,20.0-5.25) && close(rotated.to.z,4.5));
    assert(close(snapSixteenth(1.1),1.125));
    Cell parsed; assert(parseCell("5,3,10",parsed) && (parsed==Cell{5,3,10}));
    assert(!parseCell("5,3,10garbage",parsed));
    setCell(p,{15,11,19},"Frame");resize(p,{6,5,7});
    assert(p.cells.size()==1);assert(p.parts.size()==1); // No accidental geometry deletion
    threw=false;
    try { resize(p,{129,5,7}); } catch (const std::invalid_argument&) {threw=true;}
    assert(threw);
    auto java=makeJavaBlockProject("Copper Casing");
    assert(java.kind==ProjectKind::JavaBlock);
    assert((java.dimensions==Dimensions{3,3,3}) && java.cells.empty());
    assert(java.parts.size()==1 && javaBlockBoundsAllowed(java.parts[0].bounds));
    assert(javaBlockBoundsAllowed({{0,0,0},{3,3,3}}));
    assert(!javaBlockBoundsAllowed({{-1.0/16.0,0,0},{1,1,1}}));
    assert(!javaBlockBoundsAllowed({{1,1,1},{3+1.0/16.0,2,2}}));
    threw=false;try{resize(java,{5,3,3});}catch(const std::invalid_argument&){threw=true;}assert(threw);
    threw=false;try{setCell(java,{1,1,1},"Frame");}catch(const std::invalid_argument&){threw=true;}assert(threw);
    auto demo=makeDemo();assert(demo.parts.size()>5);
    assert(demo.cells.at({2,0,2})=="Controller");
    auto invalid=b;invalid.motionId="motion_1";
    assert(sliceStaticPart(invalid,{16,12,20},Direction::North).empty());
    invalid.motionId="";invalid.rotationDegrees.y=20;
    threw=false;try{(void)sliceStaticPart(invalid,{16,12,20},Direction::North);}catch(const std::invalid_argument&){threw=true;}
    assert(threw);
    std::cout << "Native C++ project/geometry tests passed\n";
}
