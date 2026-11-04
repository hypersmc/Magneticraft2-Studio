#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
namespace studio {
// The built-in scenes are copies of the *actual* six merged Java runtime v1
// definitions, not machine names applied to the Grinder's input channels.
// Qt's compiled .qrc means users need neither the mod source nor a filesystem
// location for the scene templates.
class VisualTemplates final {
public:
    static QStringList machines();
    static bool bundled(const QString& machine,QJsonObject& manifest,
                        QJsonObject& scene,QString& error);
};
} // namespace studio
