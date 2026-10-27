#pragma once
#include <QDialog>
#include <QJsonObject>
class QComboBox;
class QPlainTextEdit;
class QDoubleSpinBox;
class QSpinBox;
class QCheckBox;
class QLabel;

namespace studio {
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
    bool parsePair(QJsonObject& manifest,QJsonObject& scene,QString& error);
    void refreshSources();
    QComboBox* machine_=nullptr;
    QPlainTextEdit* manifest_=nullptr;
    QPlainTextEdit* scene_=nullptr;
    QPlainTextEdit* preview_=nullptr;
    QDoubleSpinBox* shaftAngle_=nullptr;
    QSpinBox* wearStage_=nullptr;
    QCheckBox* running_=nullptr;
    QCheckBox* overloaded_=nullptr;
    QCheckBox* worn_=nullptr;
    QLabel* status_=nullptr;
    bool valid_=false;
};
} // namespace studio
