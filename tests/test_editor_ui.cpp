#include "ui/MainWindow.hpp"
#include "ui/StartPage.hpp"
#include <QPushButton>
#include "ui/Viewport.hpp"
#include <QApplication>
#include <QToolButton>
#include <QLineEdit>
#include <QTreeWidget>
#include <QDockWidget>
#include <QTest>
#include <QTimer>
#include <QDebug>

using namespace studio;
int main(int argc,char** argv){
    QApplication app(argc,argv);
    MainWindow editor;
    editor.resize(1600,940);editor.show();
    QTest::qWait(240);
    auto* home=editor.findChild<StartPage*>("StartPage");
    if(!home || !editor.isAtHome() || !home->isVisible()){
        qCritical("Startup should show the native Start page");return 10;
    }
    auto* newJava=home->findChild<QPushButton*>("NewJavaBlockCard");
    auto* example=home->findChild<QPushButton*>("ExampleWorkshop");
    auto* washer=home->findChild<QPushButton*>("ExampleOreWasher");
    auto* sifter=home->findChild<QPushButton*>("ExampleSifter");
    if(!newJava||!example||!washer||!sifter){qCritical("Start page project and examples missing");return 11;}
    newJava->click();QTest::qWait(250);
    if(editor.isAtHome()||editor.projectKind()!=ProjectKind::JavaBlock){
        qCritical("Java model card did not create the right project type");return 12;
    }
    editor.openHome();QTest::qWait(100);
    if(!editor.isAtHome()){qCritical("Home navigation failed");return 13;}
    example->click();QTest::qWait(600);
    if(editor.isAtHome()||editor.projectKind()!=ProjectKind::Multiblock){
        qCritical("Workshop example failed to open");return 14;
    }
    auto* build=editor.findChild<QToolButton*>("WorkspaceBuild");
    auto* model=editor.findChild<QToolButton*>("WorkspaceModel");
    auto* uv=editor.findChild<QToolButton*>("WorkspaceUV");
    auto* rail=editor.findChild<QWidget*>("ToolRail");
    auto* search=editor.findChild<QLineEdit*>("OutlinerSearch");
    auto* tree=editor.findChild<QTreeWidget*>("PartHierarchy");
    auto* viewport=editor.findChild<Viewport*>();
    if(!build||!model||!uv||!rail||!search||!tree||!viewport){qCritical("Editor redesign controls are missing");return 2;}
    if(!viewport->rendererReady()){qCritical("OpenGL renderer not initialized");return 3;}
    build->click();QTest::qWait(150);
    if(rail->isVisible() || !build->isChecked()){qCritical("Build workspace layout did not activate");return 4;}
    model->click();QTest::qWait(150);
    if(!rail->isVisible() || !model->isChecked()){qCritical("Model workspace tool rail missing");return 5;}
    search->setText("drum");QTest::qWait(30);
    bool drum=false;
    for(int i=0;i<tree->topLevelItemCount();i++){
       auto* group=tree->topLevelItem(i);
       for(int j=0;j<group->childCount();j++){
          auto* item=group->child(j);
          if(!item->isHidden() && item->text(0).contains("Drum",Qt::CaseInsensitive))drum=true;
          else if(!item->isHidden()){qCritical("Outliner search retained an unrelated part");return 6;}
       }
    }
    if(!drum){qCritical("Outliner search failed to find sample drum");return 7;}
    search->clear();
    uv->click();QTest::qWait(150);
    auto* texture=editor.findChild<QDockWidget*>("TextureDock");
    if(!uv->isChecked() || !texture || !texture->isVisible()){qCritical("Texture workspace not visible");return 8;}
    model->click();QTest::qWait(180);
    if(argc>1 && !editor.grab().save(QString::fromLocal8Bit(argv[1]))){qCritical("Could not write UI screenshot");return 9;}
    qInfo("Editor UX test passed: Build, Model, UV, Outliner search and OpenGL");
    return 0;
}
