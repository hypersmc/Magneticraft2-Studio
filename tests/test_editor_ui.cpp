#include "ui/MainWindow.hpp"
#include "ui/StartPage.hpp"
#include <QPushButton>
#include "ui/Viewport.hpp"
#include "ui/UvFaceOverview.hpp"
#include "ui/UvEditor.hpp"
#include <QComboBox>
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
    // Select a model part before visiting the UV workspace. Its six faces
    // must be shown together, not only through a dropdown.
    for(int i=0;i<tree->topLevelItemCount();i++){
        auto* group=tree->topLevelItem(i);
        if(group->childCount()){
            tree->setCurrentItem(group->child(0));break;
        }
    }
    uv->click();QTest::qWait(250);
    auto* texture=editor.findChild<QDockWidget*>("TextureDock");
    auto* overview=editor.findChild<UvFaceOverview*>("UvFaceOverview");
    auto* atlas=editor.findChild<UvEditor*>("UvCanvas");
    auto* faceDropdown=editor.findChild<QComboBox*>("UvFaceSelect");
    auto* workspace=editor.findChild<QWidget*>("UVWorkspacePanel");
    if(!uv->isChecked() || !texture || !texture->isVisible() || !overview || !atlas
       || !faceDropdown || !workspace || !workspace->isVisible()){
        qCritical("Full six-face UV workspace not visible");return 8;
    }
    QTest::mouseClick(overview,Qt::LeftButton,Qt::NoModifier,overview->faceCenter(4).toPoint());
    if(faceDropdown->currentIndex()!=4 || overview->selectedFace()!=4){
        qCritical("Selecting the Up face in the overview did not activate its independent UV mapping");return 15;
    }
    if(atlas->width()<260){qCritical("UV canvas is still constrained to a tiny dock");return 16;}
    model->click();QTest::qWait(180);
    if(argc>1 && !editor.grab().save(QString::fromLocal8Bit(argv[1]))){qCritical("Could not write UI screenshot");return 9;}
    qInfo("Editor UX test passed: Build, Model, UV, Outliner search and OpenGL");
    return 0;
}
