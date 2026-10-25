#include "ui/MainWindow.hpp"
#include "ui/Viewport.hpp"
#include "ui/Theme.hpp"

#include <QApplication>
#include <QFileInfo>
#include <QFont>
#include <QPalette>
#include <QPixmap>
#include <QSettings>
#include <QStyleFactory>
#include <QTimer>
#include <QDebug>
#include <QSurfaceFormat>

int main(int argc,char** argv) {
    QSurfaceFormat format;
    format.setVersion(3,3);
    format.setProfile(QSurfaceFormat::CoreProfile);
    format.setDepthBufferSize(24);
    format.setStencilBufferSize(8);
    format.setSamples(4);
    QSurfaceFormat::setDefaultFormat(format);

    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("Magneticraft2");
    QCoreApplication::setApplicationName("Magneticraft2-Studio");
    QCoreApplication::setApplicationVersion("0.5.0-alpha");
    app.setStyle(QStyleFactory::create("Fusion"));
    QFont font=app.font();font.setPointSize(10);app.setFont(font);
    QPalette pal=app.palette();
    pal.setColor(QPalette::Window,QColor("#101920"));
    pal.setColor(QPalette::WindowText,QColor("#dbe5ec"));
    pal.setColor(QPalette::Base,QColor("#17252e"));
    pal.setColor(QPalette::AlternateBase,QColor("#1d303b"));
    pal.setColor(QPalette::Text,QColor("#e5edf2"));
    pal.setColor(QPalette::Button,QColor("#22323e"));
    pal.setColor(QPalette::ButtonText,QColor("#e5edf2"));
    pal.setColor(QPalette::Highlight,QColor("#aa673c"));
    pal.setColor(QPalette::HighlightedText,Qt::white);
    pal.setColor(QPalette::ToolTipBase,QColor("#263644"));
    pal.setColor(QPalette::ToolTipText,Qt::white);
    pal.setColor(QPalette::Disabled,QPalette::Text,QColor("#647887"));
    app.setPalette(pal);
    app.setStyleSheet(studio::editorStyle);
    studio::MainWindow window;
    bool smoke=false;
    QString screenshot;
    bool sampleWasher=false;
    for(int i=1;i<argc;i++){
        const QString argument=QString::fromLocal8Bit(argv[i]);
        if(argument=="--smoke")smoke=true;
        else if(argument=="--sample-washer")sampleWasher=true;
        else if(argument=="--screenshot" && i+1<argc)screenshot=QString::fromLocal8Bit(argv[++i]);
        else if(QFileInfo::exists(argument))window.openPath(argument);
    }
    if(sampleWasher && !window.openOreWasherReference())return 7;
    window.show();
    if(smoke) {
        QTimer::singleShot(1600,&app,[&](){
            const auto* viewport=window.findChild<studio::Viewport*>();
            if(!viewport || !viewport->isValid() || !viewport->rendererReady()){
                qCritical("Native OpenGL 3.3 viewport failed startup smoke test");
                app.exit(4);return;
            }
            if(!screenshot.isEmpty() && !window.grab().save(screenshot)){
                qCritical("Failed to save native GUI preview screenshot");
                app.exit(5);return;
            }
            qInfo("Native Qt 6 / OpenGL 3.3 startup smoke test passed");
            app.quit();
        });
    }
    const int code=app.exec();
    qInfo("Studio: Qt event loop returned with code %d",code);
    return code;
}
