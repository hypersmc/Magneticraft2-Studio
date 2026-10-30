#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QString>
#include <QStringList>

namespace studio {

/** Exact Forge 1.20.1 machine_visuals / visual_animation v1 contract.
 * Documents are retained as JSON objects so optional/unknown properties survive
 * import, editing and export without a lossy parallel schema.
 */
class VisualContract {
public:
    static bool parseObject(const QByteArray& json,QJsonObject& out,QString& error);
    static bool validateManifest(const QJsonObject& manifest,const QString& machine,
                                 QString& error);
    static bool validateScene(const QJsonObject& scene,QString& error);
    static bool validatePair(const QString& machine,const QJsonObject& manifest,
                             const QJsonObject& scene,QString& error,
                             QStringList* warnings=nullptr);
    static bool importPair(const QString& machine,const QByteArray& manifestBytes,
                           const QByteArray& sceneBytes,QJsonObject& manifest,
                           QJsonObject& scene,QString& error,QStringList* warnings=nullptr);
    static bool exportPair(const QString& resourcePackDirectory,const QString& machine,
                           const QJsonObject& manifest,const QJsonObject& scene,
                           QString& error,QStringList* warnings=nullptr);
    static QStringList availableAngles(const QString& machine);
    static QStringList availableIntegers(const QString& machine);

    // Deterministic preview sampling of Java-driven visual data; *not* a new
    // animation clock. The caller supplies physical Java source angles.
    // The returned data contains evaluated component rotations/translations,
    // wear-variant selection and constrained slider-crank values.
    static QJsonObject sample(const QJsonObject& manifest,const QJsonObject& scene,
                              const QMap<QString,double>& physicalAngles,
                              bool running,bool overloaded,bool worn,int wearStage,
                              QString& error);
};

} // namespace studio
