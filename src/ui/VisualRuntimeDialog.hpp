#pragma once
#include <QDialog>
#include <QJsonObject>
#include "io/RigAssetLoader.hpp"
#include <QMap>
class QComboBox;
class QPlainTextEdit;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLabel;
class QTimer;
class QElapsedTimer;
class QPushButton;

namespace studio {
class Viewport;
/** Editor for the existing Java v1 machine manifest and scene graph JSON.
 * Source text is intentionally non-destructively editable; unknown fields
 * remain in place, and Save is blocked until both exact schemas validate.
 */
class VisualRuntimeDialog final : public QDialog {
    Q_OBJECT
public:
    explicit VisualRuntimeDialog(const QJsonObject& existing,QWidget* parent=nullptr);
    QJsonObject metadata() const;
private:
    void importManifest();
    void importScene();
    void validate();
    void sample();
    void exportFiles();
    void loadRigAssets();
    void loadExtraTextures();
    void syncAngleSource();
    void advancePlayback();
    bool parsePair(QJsonObject& manifest,QJsonObject& scene,QString& error);
    void refreshSources();
    QComboBox* machine_=nullptr;
    QPlainTextEdit* manifest_=nullptr;
    QPlainTextEdit* scene_=nullptr;
    QPlainTextEdit* preview_=nullptr;
    Viewport* rigViewport_=nullptr;
    QLabel* rigAssetStatus_=nullptr;
    QComboBox* angleSource_=nullptr;
    QDoubleSpinBox* playbackSpeed_=nullptr;
    QPushButton* playPause_=nullptr;
    QTimer* playbackTimer_=nullptr;
    QMap<QString,double> previewAngles_;
    RigAssets rigAssets_;
    bool rigAssetsLoaded_=false;
    QDoubleSpinBox* shaftAngle_=nullptr;
    QSpinBox* wearStage_=nullptr;
    QCheckBox* running_=nullptr;
    QCheckBox* overloaded_=nullptr;
    QCheckBox* worn_=nullptr;
    QLabel* status_=nullptr;
    bool valid_=false;
};
} // namespace studio
