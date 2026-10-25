#include "ui/MainWindow.hpp"
#include "ui/Viewport.hpp"

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
    QCoreApplication::setApplicationVersion("0.4.0-alpha");
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
    app.setStyleSheet(R"(
        QMainWindow, QMenuBar, QMenu { background: #101920; color: #e0e9ef; }
        QMenuBar::item:selected, QMenu::item:selected { background: #8c5634; }
        QDockWidget { color: #d6a477; font-weight: 600; }
        QDockWidget::title { background: #1b2b35; padding: 8px; }
        QToolBar { border: 0; background: #1b2b35; spacing: 6px; padding: 5px; }
        QStatusBar { background: #172631; color: #a8bcc8; }
        QLineEdit,QDoubleSpinBox,QSpinBox,QComboBox,QTreeWidget {
            background: #182833; color: #d9e5ed; border: 1px solid #38505c;
            border-radius: 4px; padding: 5px;
        }
        QTreeWidget::item:selected { background: #875337; }
        QGroupBox { border: 1px solid #2c4251; border-radius: 6px; margin-top: 12px;
                    font-weight: 600; color: #d9a477; padding: 10px; }
        QGroupBox::title { subcontrol-origin: margin; left: 9px; padding: 0 5px; }
        QPushButton, QToolButton { background: #253845; color: #e0e8ef; border-radius: 5px;
            padding: 6px 10px; }
        QPushButton:hover, QToolButton:hover { background: #365260; }
        QSplitter::handle { background: #2e4350; }
        QLabel#InspectorHelp { color: #8297a6; font-size: 9pt; }
    )");
    studio::MainWindow window;
    bool smoke=false;
    QString screenshot;
    for(int i=1;i<argc;i++){
        const QString argument=QString::fromLocal8Bit(argv[i]);
        if(argument=="--smoke")smoke=true;
        else if(argument=="--screenshot" && i+1<argc)screenshot=QString::fromLocal8Bit(argv[++i]);
        else if(QFileInfo::exists(argument))window.openPath(argument);
    }
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
    return app.exec();
}
