#include "AnimationExport.hpp"
#include "io/ProjectIO.hpp"

#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>

#include <cmath>
#include <map>
#include <set>
#include <string>

namespace studio {
namespace {

bool identifier(const QString& value) {
    static const QRegularExpression id(QStringLiteral("^[a-z][a-z0-9_]*$"));
    return id.match(value).hasMatch();
}
bool modelId(const QString& value) {
    static const QRegularExpression id(
        QStringLiteral("^[a-z0-9_.-]+:[a-z0-9_./-]+$"));
    return id.match(value).hasMatch();
}
bool condition(const QString& value) {
    return value == "always" || value == "running" || value == "stopped"
        || value == "overloaded" || value == "worn";
}
bool vector3(const QJsonValue& value) {
    if (!value.isArray()) return false;
    const QJsonArray values = value.toArray();
    if (values.size() != 3) return false;
    for (const QJsonValue& coord : values) {
        if (!coord.isDouble() || !std::isfinite(coord.toDouble())
                || std::abs(coord.toDouble()) > 256.0) return false;
    }
    return true;
}
bool axis(const QString& value) {
    return value == "x" || value == "y" || value == "z"
        || value == "-x" || value == "-y" || value == "-z";
}

} // namespace

bool AnimationExport::build(const Project& project, const QJsonObject& original,
                            QJsonObject& animation, QString& error) {
    error.clear();
    animation = {};
    if (project.kind != ProjectKind::Multiblock) {
        error = "Animated multiblock export requires a multiblock project.";
        return false;
    }

    // ProjectIO retains unknown part metadata across native edits. This
    // format intentionally references existing baked Minecraft model IDs.
    // The separate formed-model generator is not finished, and this exporter
    // must never silently fabricate missing Minecraft geometry.
    const QJsonObject document = ProjectIO::encode(project, original);
    const QJsonArray parts = document.value("parts").toArray();
    const QJsonArray motions = document.value("motions").toArray();
    std::map<QString, QJsonObject> motionById;
    for (const QJsonValue& value : motions) {
        if (!value.isObject()) continue;
        const QJsonObject motion = value.toObject();
        const QString id = motion.value("id").toString();
        if (!identifier(id) || motionById.count(id)) {
            error = "Invalid or duplicate motion ID: " + id;
            return false;
        }
        motionById[id] = motion;
    }

    QJsonObject components;
    std::map<QString, QString> parents;
    std::map<QString, QStringList> partIdsByMotion;
    for (const QJsonValue& value : parts) {
        if (!value.isObject()) continue;
        const QJsonObject part = value.toObject();
        const QString id = part.value("id").toString();
        const QString motionId = part.value("motionId").toString();
        const QJsonObject binding = part.value("animation").toObject();
        if (binding.isEmpty()) {
            if (!motionId.isEmpty()) {
                error = "Part " + id + " uses motion " + motionId
                        + " but has no explicit animation model binding.";
                return false;
            }
            continue;
        }
        if (!identifier(id) || components.contains(id)) {
            error = "Invalid or duplicate animated component ID: " + id;
            return false;
        }
        if (!vector3(binding.value("model_offset")) || !vector3(binding.value("pivot"))) {
            error = "Part " + id + " requires finite controller-relative "
                    + "animation.model_offset and animation.pivot arrays.";
            return false;
        }

        QJsonObject component{
            {"model_offset", binding.value("model_offset")},
            {"pivot", binding.value("pivot")}
        };
        if (binding.contains("model") && !binding.value("model").isNull()) {
            const QString model = binding.value("model").toString();
            if (!modelId(model)) {
                error = "Invalid model resource ID in part " + id + ": " + model;
                return false;
            }
            component["model"] = model;
        }
        const QString layer = binding.value("render_layer").toString("cutout");
        if (layer != "solid" && layer != "cutout" && layer != "translucent") {
            error = "Invalid render_layer in part " + id;
            return false;
        }
        component["render_layer"] = layer;
        const QString visibility = binding.value("visible_when").toString("always");
        if (!condition(visibility)) {
            error = "Invalid visible_when state in part " + id;
            return false;
        }
        component["visible_when"] = visibility;
        const QString parent = binding.value("parent").toString();
        if (!parent.isEmpty()) {
            if (!identifier(parent) || parent == id) {
                error = "Invalid parent in part " + id;
                return false;
            }
            component["parent"] = parent;
            parents[id] = parent;
        }
        components[id] = component;
        if (!motionId.isEmpty()) {
            if (!identifier(motionId) || !motionById.count(motionId)) {
                error = "Part " + id + " refers to unknown motion: " + motionId;
                return false;
            }
            partIdsByMotion[motionId].append(id);
        }
    }
    if (components.isEmpty() || components.size() > 256) {
        error = "Provide 1..256 parts with explicit animation bindings.";
        return false;
    }
    // Validate the entire parent chain, even joints that have no animation.
    for (const auto& [child, ignored] : parents) {
        std::set<QString> visited;
        QString current = child;
        while (parents.count(current)) {
            if (visited.size() >= 32 || !visited.insert(current).second) {
                error = "Animation hierarchy has a cycle or exceeds depth 32 at " + child;
                return false;
            }
            current = parents.at(current);
            if (!components.contains(current)) {
                error = "Unknown parent " + current + " referenced by " + child;
                return false;
            }
        }
    }

    QJsonObject inputs;
    QJsonObject animations;
    for (const auto& [motionId, ids] : partIdsByMotion) {
        const QJsonObject motion = motionById.at(motionId);
        const QString type = motion.value("type").toString("rotate");
        if (type != "rotate" && type != "driven") {
            error = "Only driven mechanical rotation is exportable: " + motionId;
            return false;
        }
        const QString source = motion.value("driver_source").toString(
            "mechanical.visual_angle_degrees");
        const QString module = motion.value("driver_module").toString("mechanical_input");
        if (source != "mechanical.visual_angle_degrees" || !identifier(module)) {
            error = "Motion " + motionId + " needs a valid Gear V2 driver module.";
            return false;
        }
        const QString direction = motion.value("axis").toString("y");
        const QString when = motion.value("when").toString("always");
        if (!axis(direction) || !condition(when)) {
            error = "Invalid axis or machine-state condition for motion " + motionId;
            return false;
        }
        const QJsonValue scaleValue = motion.value("scale");
        const double scale = scaleValue.isUndefined() ? 1.0 : scaleValue.toDouble();
        if ((!scaleValue.isUndefined() && !scaleValue.isDouble())
                || !std::isfinite(scale) || std::abs(scale) > 128.0) {
            error = "Motion " + motionId + " has an invalid rotation scale.";
            return false;
        }

        const QString inputName = motionId + "_angle";
        inputs[inputName] = QJsonObject{{"source", source}, {"module", module}};
        QJsonArray tracks;
        for (const QString& id : ids) {
            tracks.append(QJsonObject{
                {"target", id},
                {"property", "rotation"},
                {"axis", direction},
                {"input", inputName},
                {"scale", scale},
                {"when", when}
            });
        }
        animations["driven_" + motionId] = QJsonObject{
            {"type", "driven"}, {"tracks", tracks}
        };
    }
    if (animations.isEmpty() || animations.size() > 128 || inputs.size() > 64) {
        error = "Animation export needs at least one motion and supports at most 64 motion inputs.";
        return false;
    }

    animation = QJsonObject{
        {"format", "magneticraft2:visual_animation"},
        {"format_version", 1},
        {"coordinate_system", QJsonObject{
            {"units", "blocks"}, {"origin", "controller"}, {"reference_facing", "south"}
        }},
        {"components", components},
        {"inputs", inputs},
        {"animations", animations}
    };
    return true;
}

bool AnimationExport::write(const QString& filename, const Project& project,
                            const QJsonObject& original, QString& error) {
    QJsonObject result;
    if (!build(project, original, result, error)) return false;
    QSaveFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        error = file.errorString();
        return false;
    }
    const QByteArray bytes = QJsonDocument(result).toJson(QJsonDocument::Indented);
    if (file.write(bytes) != bytes.size()) {
        error = file.errorString();
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        error = file.errorString();
        return false;
    }
    return true;
}

} // namespace studio
