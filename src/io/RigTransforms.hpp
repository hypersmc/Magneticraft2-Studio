#pragma once
#include <QJsonObject>
#include <QMap>
#include <QMatrix4x4>
#include <QString>
#include <map>
#include <string>

namespace studio {

// Per-component camera-independent visual state. No gameplay clocks or
// inferred power values: every input is supplied by the editor's explicit
// preview controls, via the real machine_visuals alias bindings.
struct RigComponentPose {
    std::string modelId;       // Baked Minecraft model ID; empty for a joint
    QMatrix4x4 world;          // Controller-relative, canonical SOUTH frame
    bool visible = true;
    QString renderLayer = "cutout";
};
struct RigPose {
    std::map<std::string,RigComponentPose> components;
    std::map<std::string,double> beltDistanceBlocks;
};

class RigTransforms final {
public:
    static bool evaluate(const QString& machine,const QJsonObject& manifest,
                         const QJsonObject& scene,const QMap<QString,double>& physicalAngles,
                         bool running,bool overloaded,bool worn,int wearStage,
                         RigPose& result,QString& error);
};
} // namespace studio
