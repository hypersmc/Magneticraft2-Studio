#include "core/Project.hpp"
#include "core/Geometry.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
using namespace studio;
static bool near(double a,double b){return std::abs(a-b)<1e-7;}
int main(){
    auto project=makeProject("Renderer QA",{32,8,32});
    project.cells.clear();
    project.cells[{1,0,1}]="Frame";
    BuildOptions opt;opt.showFormed=false;
    auto one=buildGeometry(project,opt);
    assert(one.visibleObjects==1);
    assert(one.triangles.size()==36); // 6 faces x 2 triangles x 3 vertices
    project.cells[{2,0,1}]="Frame";
    auto two=buildGeometry(project,opt);
    assert(two.visibleObjects==2);
    assert(two.triangles.size()==60); // Shared face correctly culled.
    opt.layer=1;
    auto filtered=buildGeometry(project,opt);
    assert(filtered.triangles.empty());
    project.parts.push_back(makeBox(project,{{1,0,1},{2,1,2}}));
    opt.showConstruction=false;opt.showFormed=true;opt.selectedPart=0;
    auto selected=buildGeometry(project,opt);
    assert(selected.triangles.size()==36);
    assert(selected.lines.size()>two.lines.size());
    auto center=rotateEuler({1,0,0},{0,0,0},{0,90,0});
    assert(near(center.x,0) && near(center.z,-1));
    auto transformed=project.parts.front();
    transformed.rotationDegrees={0,45,0};
    auto bounds=transformedBounds(transformed);
    assert(bounds.from.x<1 && bounds.to.z>2);
    // Geometry can be built for large dimensions without dense allocation.
    auto giant=makeProject("Big",{128,128,128});
    giant.cells.clear();giant.cells[{127,127,127}]="Frame";
    auto geometry=buildGeometry(giant,{true,false,false,true,-1,-1});
    assert(geometry.triangles.size()==36);
    std::cout<<"Native renderer mesh and sparse-culling tests passed\n";
}
