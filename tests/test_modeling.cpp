#include "core/Modeling.hpp"
#include "core/Geometry.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace studio;
static bool near(double a,double b){return std::abs(a-b)<1e-9;}
int main(){
    constexpr double snap=1.0/16.0;
    auto box=drawnCuboid({2.1,100,4.0},{0.99,-100,1.21},0.3,1.2,snap);
    assert(near(box.from.x,1.0));assert(near(box.to.x,2.125));
    assert(near(box.from.z,1.1875));assert(near(box.to.z,4.0));
    assert(near(box.from.y,0.3125));assert(near(box.to.y,1.5));
    auto click=drawnCuboid({2,0,2},{2,0,2},1,0.01,snap);
    assert(near(click.to.x-click.from.x,snap));
    assert(near(click.to.z-click.from.z,snap));
    assert(near(click.to.y-click.from.y,snap));
    auto negative=drawnCuboid({-2,0,-1},{-3.01,0,-5},-0.5,2.5,0.25);
    assert(near(negative.from.x,-3));assert(near(negative.to.x,-2));
    assert(near(negative.from.y,-0.5));assert(near(negative.to.y,2.0));
    for(double bad : {0.0,-1.0}){
        bool threw=false;try{snapGrid(1,bad);}catch(const std::invalid_argument&){threw=true;}
        assert(threw);
    }
    const auto project=makeProject("Large custom furnace",{40,20,45});
    auto part=makeBox(project,drawnCuboid({0,0,0},{40,0,45},0,20,snap));
    assert(validBounds(part.bounds));
    auto mesh=buildGeometry(project,BuildOptions{false,true,false,true,-1,-1});
    (void)mesh;
    std::cout<<"Direct modeling primitives and grid snap tests passed\n";
}
