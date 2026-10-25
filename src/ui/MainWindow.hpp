#pragma once
#include "core/Project.hpp"
#include <QJsonObject>
#include <QImage>
#include <QMainWindow>
#include <QString>
#include <vector>
#include <map>

class QAction;
class QComboBox;
class QDoubleSpinBox;
class QLineEdit;
class QToolBar;
class QSpinBox;
class QTreeWidget;
class QCheckBox;
class QCloseEvent;
class QDockWidget;
class QLabel;
class QPushButton;
class QToolButton;
class QVBoxLayout;

namespace studio {
class LayerCanvas;
class Viewport;
class UvEditor;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent=nullptr);
    void openPath(const QString& path);
    bool openOreWasherReference(); // Load bundled actual ore washer geometry for editor evaluation.
protected:
    void closeEvent(QCloseEvent* event) override;
private:
    void buildUI();
    void createMenus();
    void createInspector();
    void createTextureDock();
    void refreshTexturePanel();
    void importPngTexture();
    void applyTextureToCube();
    void applyFaceTexture();
    void setAutoUv();
    void rotateUv();
    void flipUv(bool horizontal);
    void applyUvNumbers();
    void refreshAll();
    void refreshScene();
    void refreshInspector();
    void refreshPartTree();
    void refreshRolePalette();
    void updateTitle();
    void snapshot();
    void createNew();
    void save();
    void saveAs();
    bool saveTo(const QString& path);
    bool maybeSave();
    void openDialog();
    void exportDialog();
    void addPart();
    void duplicatePart();
    void removePart();
    void undo();
    void redo();
    void setSelected(int index);
    void applyInspector();
    void editCell(int x,int y,int z,const QString& role);
    void resizeDocument();
    void applyTransformDelta(double amount,char axis,char tool);
    void addDrawnBox(double x0,double z0,double x1,double z1);
    void switchWorkspace();
    void commitChanged();
    void importBlockModels();
    void filterOutliner();
    void syncWorkspaceChrome();
    void resetWorkspaceLayout();
    void renameSelected();
    void addPrimitive(const QString& kind);

    Project project_;
    QJsonObject loadedJson_;
    QString filename_;
    bool dirty_=false;
    bool updating_=false;
    bool dragChanged_=false;
    bool uvDragChanged_=false;
    std::string uvImageKey_;
    QImage cachedUvImage_;
    std::vector<Project> undo_,redo_;
    int selected_=-1;
    int layer_=0;
    Viewport* viewport_=nullptr;
    QWidget* toolRail_=nullptr;
    QVBoxLayout* railLayout_=nullptr;
    QLineEdit* outlinerSearch_=nullptr;
    QLabel* sceneTitle_=nullptr;
    QLabel* sceneInfo_=nullptr;
    QLabel* projectHeading_=nullptr;
    QLabel* projectContext_=nullptr;
    QLabel* statusMetrics_=nullptr;
    QToolButton* workspaceBuild_=nullptr;
    QToolButton* workspaceModel_=nullptr;
    QToolButton* workspaceUV_=nullptr;
    std::map<char,QToolButton*> railButtons_;
    bool uvWorkspace_=false;
    LayerCanvas* layers_=nullptr;
    QWidget* layerContainer_=nullptr;
    QToolBar* modelToolbar_=nullptr;
    QTreeWidget* tree_=nullptr;
    QComboBox* role_=nullptr;
    QComboBox* material_=nullptr;
    QComboBox* mode_=nullptr;
    QCheckBox* ghost_=nullptr;
    QComboBox* tool_=nullptr;
    QComboBox* axis_=nullptr;
    QComboBox* snap_=nullptr;
    QComboBox* viewPreset_=nullptr;
    QDoubleSpinBox* workPlane_=nullptr;
    QDoubleSpinBox* drawHeight_=nullptr;
    QSpinBox* layerSpinner_=nullptr;
    QLineEdit* partName_=nullptr;
    QLineEdit* texture_=nullptr;
    QLineEdit* groupName_=nullptr;
    QDoubleSpinBox* from_[3]{};
    QDoubleSpinBox* to_[3]{};
    QDoubleSpinBox* angle_[3]{};
    QDoubleSpinBox* pivot_[3]{};
    QCheckBox* visible_=nullptr;
    QDockWidget* textureDock_=nullptr;
    UvEditor* uvCanvas_=nullptr;
    QComboBox* uvFace_=nullptr;
    QComboBox* uvTexture_=nullptr;
    QDoubleSpinBox* uvNumbers_[4]{};
    QLabel* textureInfo_=nullptr;
    QPushButton* applyTexture_=nullptr;
    QAction* undoAction_=nullptr;
    QAction* redoAction_=nullptr;
};

} // namespace studio
