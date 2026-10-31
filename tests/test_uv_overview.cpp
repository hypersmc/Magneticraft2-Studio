#include "ui/UvFaceOverview.hpp"

#include <QApplication>
#include <QColor>
#include <QImage>
#include <QSignalSpy>
#include <QtTest/QTest>
#include <iostream>

int main(int argc,char** argv){
    QApplication app(argc,argv);
    studio::UvFaceOverview overview;
    overview.setObjectName("UvFaceOverview");
    overview.resize(580,420);
    std::array<studio::UvFaceOverview::Face,6> faces{};
    for(int i=0;i<6;i++){
        faces[std::size_t(i)].image=QImage(16,16,QImage::Format_RGBA8888);
        faces[std::size_t(i)].image.fill(QColor::fromHsv(i*50,220,230));
        faces[std::size_t(i)].texture=QString("test:face_%1").arg(i);
        faces[std::size_t(i)].uv={0,0,1.0+0.25*i,1.0};
        faces[std::size_t(i)].automatic=i%2==0;
        faces[std::size_t(i)].quarterTurns=i%4;
    }
    overview.setFaces(faces,0);
    overview.show();
    if(!QTest::qWaitForWindowExposed(&overview,7000))return 1;
    QSignalSpy selected(&overview,&studio::UvFaceOverview::facePicked);
    // Every side must be directly visible and selectable, not hidden in a
    // six-item combo box. The face net emits a selection without modifying UVs.
    for(int side=0;side<6;side++){
        const QPoint center=overview.faceCenter(side).toPoint();
        if(center.x()<0||center.x()>=overview.width()||center.y()<0||center.y()>=overview.height())return 2;
        QTest::mouseClick(&overview,Qt::LeftButton,Qt::NoModifier,center);
        if(overview.selectedFace()!=side || selected.count()!=side+1)return 3;
        if(selected.last().first().toInt()!=side)return 4;
    }
    const QImage picture=overview.grab().toImage();
    if(picture.isNull())return 5;
    if(argc>1 && !picture.save(QString::fromLocal8Bit(argv[1])))return 6;
    overview.clear();
    if(overview.selectedFace()!=-1)return 7;
    std::cout<<"Six independent cuboid face UVs are visible and directly selectable\n";
    return 0;
}
