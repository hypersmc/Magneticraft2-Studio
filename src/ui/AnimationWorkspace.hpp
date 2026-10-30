#pragma once
#include "core/Project.hpp"
#include "io/RigAssetLoader.hpp"
#include <QElapsedTimer>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QWidget>
#include <memory>

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTimer;

namespace studio {
class Viewport;

// Animate edits the same Studio document as Build/Model/UV. The viewport and
// project PNGs belong to MainWindow; this panel owns only scene metadata,
// preview-only inputs, and reusable read-only external rig assets.
class AnimationWorkspace final : public QWidget {
    Q_OBJECT
public:
    explicit AnimationWorkspace(Viewport* viewport,QWidget* parent=nullptr);
    void setProject(const Project& project,const QJsonObject& visualRuntime);
    void activate(bool visible);
    QStringList componentIds() const;
    QStringList boundComponentIds() const;
    void setAuthoredProject(const Project& project);
    void setSelectedModelPart(const QString& partId,const QString& partName,
                              const QString& currentRig,Vec3 partPivot);
    QString selectedComponentId() const;
    void selectComponent(const QString& id);
    QJsonObject metadata() const;
    bool loadAssetSource(const QString& source,QString& error,bool remember=true);
signals:
    void animationChanged(const QJsonObject& metadata);
    void componentsChanged();
    void message(const QString& text);
    void bindPartRequested(const QString& partId,const QString& componentId);
private:
    void refreshComponents();
    void refreshTrackControls();
    void refreshInputs();
    void inspectPose();
    void refreshAssets(bool force=false);
    void rebuildAuthoredGeometry();
    void publishRigModels();
    void changeMachine(const QString& machine);
    void changeRotation();
    void changePivot();
    void chooseModJar();
    void chooseAssetFolder();
    void chooseVanillaJar();
    void tick();
    void stopPlayback();
    bool saveChanges(QJsonObject manifest,QJsonObject scene,const QString& action);
    bool ensureInput(QJsonObject& manifest,QJsonObject& scene,QString& id);
    QString detectMachine(const QString& name) const;

    Viewport* viewport_=nullptr;
    QComboBox* machine_=nullptr;
    QComboBox* component_=nullptr;
    QComboBox* track_=nullptr;
    QComboBox* driver_=nullptr;
    QComboBox* axis_=nullptr;
    QComboBox* when_=nullptr;
    QComboBox* previewSource_=nullptr;
    QDoubleSpinBox* scale_=nullptr;
    QDoubleSpinBox* phase_=nullptr;
    QDoubleSpinBox* pivot_[3]{};
    QDoubleSpinBox* previewAngle_=nullptr;
    QDoubleSpinBox* previewSpeed_=nullptr;
    QCheckBox* running_=nullptr;
    QCheckBox* overloaded_=nullptr;
    QCheckBox* worn_=nullptr;
    QSpinBox* wearStage_=nullptr;
    QPushButton* play_=nullptr;
    QLabel* status_=nullptr;
    QLabel* assetStatus_=nullptr;
    QLabel* selectedPartStatus_=nullptr;
    QPushButton* bindPartButton_=nullptr;
    QPushButton* unbindPartButton_=nullptr;
    QPushButton* pivotFromPart_=nullptr;
    Vec3 selectedPartPivot_{};
    QTimer* timer_=nullptr;
    QElapsedTimer clock_;
    QJsonObject manifest_,scene_;
    QMap<QString,double> previewAngles_;
    std::shared_ptr<const RigAssets> assets_;
    QString selectedPartId_;
    Project authoredProject_;
    std::map<std::string,Geometry> authoredMeshes_;
    QString sourcePath_;
    QString vanillaPath_;
    bool active_=false;
    bool settingUi_=false;
    bool hasSavedRuntime_=false;
    QString acceptedMachine_;
    ProjectKind kind_=ProjectKind::Multiblock;
};
} // namespace studio
