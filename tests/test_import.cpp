#include "io/BlockModelImport.hpp"
#include "core/Project.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <QFile>
using namespace studio;
int main(){
 const auto p=makeProject("Washer",{3,2,3});
 const char* model=R"({"textures":{"frame":"block/stripped_spruce_log","shell":"minecraft:block/copper_block"},"elements":[{"from":[-10,0,-13],"to":[-7,3,3],"faces":{"north":{"texture":"#frame","uv":[0,0,3,3]},"south":{"texture":"#frame","uv":[0,0,3,3]}}},{"from":[6,13,0],"to":[10,18,24],"rotation":{"origin":[8,15,12],"axis":"z","angle":22.5},"faces":{"up":{"texture":"#shell","rotation":90,"uv":[1,2,11,12]}}}]})";
 const auto result=importBlockModel(model,p,"Washer body",{1,0,1});
 assert(result.error.isEmpty());assert(result.parts.size()==2);
 assert(std::abs(result.parts[0].bounds.from.x-0.375)<.0001);
 assert(std::abs(result.parts[0].bounds.from.z-0.1875)<.0001);
 assert(result.parts[0].faces[1].texture=="minecraft:block/stripped_spruce_log");
 assert(std::abs(result.parts[0].faces[1].uv[2]-3./16.)<.0001);
 assert(result.parts[1].rotationDegrees.z==22.5);
 assert(result.parts[1].faces[4].quarterTurns==1);
 assert(result.parts[1].group=="Washer body");
 assert(importBlockModel(R"({"parent":"minecraft:block/cube_all"})",p,"Parent",{}).parts.empty());
 const char* invalid=R"({"elements":[{"from":[4,4,4],"to":[0,5,5]}]})";
 assert(!importBlockModel(invalid,p,"Bad",{}).error.isEmpty());
 #ifdef STUDIO_TEST_SOURCE_DIR
 const QString path=QString(STUDIO_TEST_SOURCE_DIR)+"/samples/ore_washer_models/mechanical_ore_washer_north.json";
 QFile original(path);
 assert(original.open(QIODevice::ReadOnly));
 const auto washer=importBlockModel(original.readAll(),p,"Real Ore Washer",{1,0,1});
 if(!washer.error.isEmpty())std::cerr<<washer.error.toStdString()<<"\n";
 assert(washer.error.isEmpty());
 assert(washer.parts.size()>=20);
 assert(washer.parts.front().group=="Real Ore Washer");
 #endif
 std::cout<<"Minecraft Java model import tests passed\n";
}
