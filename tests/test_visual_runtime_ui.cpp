#include "ui/VisualRuntimeDialog.hpp"
#include "ui/Viewport.hpp"
#include "ui/Theme.hpp"
#include <QApplication>
#include <QDoubleSpinBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QPalette>
#include <QStyleFactory>
#include <QFont>
#include <QColor>
#include <vector>
#include <utility>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPlainTextEdit>
#include <QTemporaryDir>
#include <QtTest/QTest>
#include <iostream>

using namespace studio;
static QByteArray load(const QString& path){
    QFile f(path);
    if(!f.open(QIODevice::ReadOnly))qFatal("Cannot open fixture %s",qPrintable(path));
    return f.readAll();
}
static bool saveTexture(const QString& folder,const QString& id,const QColor& color){
    QDir dir(folder);
    if(!dir.mkpath("assets/minecraft/textures/block"))return false;
    QImage image(16,16,QImage::Format_RGBA8888);
    image.fill(color);
    for(int y=0;y<16;y++)for(int x=0;x<16;x++){
        if((x/4+y/4)%2)image.setPixelColor(x,y,color.darker(125));
    }
    return image.save(dir.filePath("assets/minecraft/textures/block/"+id+".png"),"PNG");
}
int main(int argc,char** argv){
    QApplication app(argc,argv);
    app.setStyle(QStyleFactory::create("Fusion"));
    QPalette pal=app.palette();
    pal.setColor(QPalette::Window,QColor("#101920"));
    pal.setColor(QPalette::WindowText,QColor("#dbe5ec"));
    pal.setColor(QPalette::Base,QColor("#17252e"));
    pal.setColor(QPalette::Text,QColor("#e5edf2"));
    pal.setColor(QPalette::Button,QColor("#22323e"));
    pal.setColor(QPalette::ButtonText,QColor("#e5edf2"));
    pal.setColor(QPalette::Highlight,QColor("#aa673c"));
    app.setPalette(pal);
    app.setStyleSheet(editorStyle);
    const QString base=QString(STUDIO_TEST_SOURCE_DIR)+"/tests/fixtures/";
    const QString machine="primitive_grinder";
    const auto manifest=QJsonDocument::fromJson(load(base+"animation-v1/machine_visuals/"+machine+".json")).object();
    const auto scene=QJsonDocument::fromJson(load(base+"animation-v1/animations/"+machine+".json")).object();
    const QJsonObject metadata{{"machine",machine},{"manifest",manifest},{"scene",scene}};
    VisualRuntimeDialog dialog(metadata);
    dialog.show();
    if(!QTest::qWaitForWindowExposed(&dialog,10000)){
        std::cerr<<"Runtime editor did not expose its window\n";return 3;
    }
    auto* viewport=dialog.findChild<Viewport*>("VisualRigViewport");
    auto* angle=dialog.findChild<QDoubleSpinBox*>("VisualPhysicalAngle");
    auto* numeric=dialog.findChild<QPlainTextEdit*>("VisualPoseSample");
    if(!viewport||!angle||!numeric||!viewport->rendererReady()){
        std::cerr<<"OpenGL rig viewport or physical input controls unavailable\n";return 4;
    }
    QTemporaryDir folder;
    if(!folder.isValid())return 5;
    QDir root(folder.path());
    if(!root.mkpath("assets/magneticraft2/models/multiblock"))return 6;
    for(const QString& name:QStringList{"primitive_grinder_axle","primitive_grinder_rotor"}){
        if(!QFile::copy(base+"rig-assets/assets/magneticraft2/models/multiblock/"+name+".json",
                       root.filePath("assets/magneticraft2/models/multiblock/"+name+".json")))return 7;
    }
    for(const auto& item:std::vector<std::pair<QString,QColor>>{
        {"oak_log",QColor("#845d34")},{"oak_log_top",QColor("#b28f65")},
        {"cobblestone",QColor("#7b7d7e")},{"stone",QColor("#858782")},
        {"grindstone_side",QColor("#8d9293")}}){
        if(!saveTexture(folder.path(),item.first,item.second))return 8;
    }
    QString error;
    if(!dialog.loadAssetsFromDirectory(folder.path(),error)){
        std::cerr<<error.toStdString()<<"\n";return 9;
    }
    QTest::qWait(220);
    const auto first=viewport->grabFramebuffer();
    angle->setValue(135.0);
    QTest::qWait(220);
    const auto second=viewport->grabFramebuffer();
    if(first.isNull()||second.isNull()||first==second||!numeric->toPlainText().contains("135")){
        std::cerr<<"The rig did not change visually when its physical angle changed\n";return 10;
    }
    if(argc>1&&!dialog.grab().save(QString::fromLocal8Bit(argv[1])))return 11;
    std::cout<<"Real mod Grinder geometry rendered and rotated in the native OpenGL dialog\n";
}
