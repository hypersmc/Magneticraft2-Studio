#include "ui/Viewport.hpp"
#include "core/Modeling.hpp"
#include <QApplication>
#include <QImage>
#include <QSignalSpy>
#include <QSurfaceFormat>
#include <QtTest/QTest>
#include <iostream>

// Integration smoke test: exercise actual pointer events through QOpenGLWidget,
// not just the C++ cuboid constructor. Run under xvfb with software OpenGL.
int main(int argc,char** argv) {
    QSurfaceFormat format;
    format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    QSurfaceFormat::setDefaultFormat(format);
    QApplication app(argc,argv);

    auto document=studio::makeProject("Model interaction QA",{8,4,8});
    studio::BuildOptions options;
    options.showConstruction=false;
    options.showFormed=true;
    options.selectedPart=-1;
    studio::Viewport viewport;
    viewport.resize(760,620);
    viewport.setScene(document,options);
    viewport.resetCamera();
    viewport.setCameraPreset(1); // Top; the model plane has a reliable ray intersection.
    viewport.setTool('b');
    viewport.setWorkPlane(0);
    viewport.setDrawHeight(0.25);
    viewport.setGridStep(1.0/16.0);
    int boxes=0;
    QObject::connect(&viewport,&studio::Viewport::boxDrawn,&viewport,
        [&](double x0,double z0,double x1,double z1){
            auto bounds=studio::drawnCuboid({x0,0,z0},{x1,0,z1},0,0.25,1.0/16.0);
            document.parts.push_back(studio::makeBox(document,bounds));
            options.selectedPart=int(document.parts.size())-1;
            viewport.setScene(document,options);
            ++boxes;
        });
    QSignalSpy drawn(&viewport,&studio::Viewport::boxDrawn);
    viewport.show();
    if(!QTest::qWaitForWindowExposed(&viewport,7000)){
        std::cerr<<"Native GPU viewport did not become visible\n";
        return 1;
    }
    QTest::qWait(200);
    if(!viewport.rendererReady()){
        std::cerr<<"OpenGL shader/viewport initialization failed\n";
        return 2;
    }
    const QPoint start(viewport.width()/2-110,viewport.height()/2-60);
    const QPoint end(viewport.width()/2+100,viewport.height()/2+75);
    QTest::mousePress(&viewport,Qt::LeftButton,Qt::NoModifier,start);
    QTest::mouseMove(&viewport,end,20);
    QTest::mouseRelease(&viewport,Qt::LeftButton,Qt::NoModifier,end);
    if(drawn.count()!=1 || boxes!=1 || document.parts.size()!=1){
        std::cerr<<"Direct model drawing failed: signal="<<drawn.count()<<" cubes="<<boxes<<"\n";
        return 3;
    }
    if(!studio::validBounds(document.parts.front().bounds))return 4;
    QTest::qWait(550);
    const QImage frame=viewport.grabFramebuffer();
    if(frame.isNull()){
        std::cerr<<"Model viewport framebuffer not captured\n";
        return 5;
    }
    if(argc>=2 && !frame.save(QString::fromLocal8Bit(argv[1]))){
        std::cerr<<"Could not save model workspace QA screenshot\n";
        return 6;
    }
    std::cout<<"GPU model creation: drew one snapped cube with real mouse events\n";
    return 0;
}
