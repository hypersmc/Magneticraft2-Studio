#include "ui/Viewport.hpp"
#include "ui/UvEditor.hpp"
#include <QApplication>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QtTest/QTest>
#include <cassert>
#include <iostream>

int main(int argc,char** argv) {
    QSurfaceFormat format;format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);
    auto project=studio::makeProject("Gizmo test",{6,4,6});
    project.parts.push_back(studio::makeBox(project,{{2,1,2},{3,2,3}}));
    studio::Viewport viewport;
    studio::BuildOptions options;options.showConstruction=false;options.showFormed=true;options.selectedPart=0;
    viewport.resize(760,620);
    viewport.setScene(project,options);
    viewport.resetCamera();viewport.setCameraPreset(1);viewport.setTool('w');
    viewport.setGridStep(1.0/16.0);viewport.show();
    if(!QTest::qWaitForWindowExposed(&viewport,7000))return 1;
    QTest::qWait(150);
    if(!viewport.rendererReady())return 2;
    QSignalSpy begin(&viewport,&studio::Viewport::beginEdit);
    QSignalSpy moved(&viewport,&studio::Viewport::deltaEdit);
    QSignalSpy end(&viewport,&studio::Viewport::endEdit);
    const QPoint center=viewport.gizmoCenterOnScreen().toPoint();
    // The white center handle moves freely in the camera-facing plane.
    const QPoint translated=center+QPoint(90,64);
    QTest::mousePress(&viewport,Qt::LeftButton,Qt::NoModifier,center);
    QTest::mouseMove(&viewport,translated,35);
    QTest::mouseRelease(&viewport,Qt::LeftButton,Qt::NoModifier,translated);
    if(begin.count()!=1 || moved.count()<1 || end.count()!=1){
        std::cerr<<"Free move signals missing: "<<begin.count()<<"/"<<moved.count()<<"/"<<end.count()<<"\n";
        return 3;
    }
    bool movedHorizontal=false;
    for(const auto& args:moved){
        if(args.size()!=3)return 4;
        const auto axis=char(args[1].toInt());
        if(axis=='x'||axis=='z')movedHorizontal=true;
    }
    if(!movedHorizontal)return 5;
    viewport.close();
    // Dragging inside the UV selection modifies tile coordinates at pixel precision.
    studio::UvEditor uv;
    uv.resize(320,320);uv.setRect({0.10,0.10,0.50,0.50});
    uv.show();
    if(!QTest::qWaitForWindowExposed(&uv,7000))return 6;
    QSignalSpy changes(&uv,&studio::UvEditor::rectChanged);
    QTest::mousePress(&uv,Qt::LeftButton,Qt::NoModifier,QPoint(100,100));
    QTest::mouseMove(&uv,QPoint(125,126),30);
    QTest::mouseRelease(&uv,Qt::LeftButton,Qt::NoModifier,QPoint(125,126));
    if(changes.count()==0)return 7;
    const auto now=uv.uv();
    if(now[0]<=0.10||now[1]<=0.10)return 8;
    std::cout<<"Real GPU gizmo drag and UV canvas pointer interaction: OK\n";
    return 0;
}
