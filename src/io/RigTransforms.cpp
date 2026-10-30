#include "RigTransforms.hpp"
#include "io/VisualContract.hpp"

#include <QJsonArray>
#include <QSet>
#include <QVector3D>
#include <cmath>
#include <functional>
#include <utility>

namespace studio {
namespace {
QVector3D vector(const QJsonValue& value){
    const auto a=value.toArray();
    return {float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble())};
}
void translate(QMatrix4x4& matrix,const QJsonValue& value) {
    if(value.isArray())matrix.translate(vector(value));
}
void rotate(QMatrix4x4& matrix,const QJsonValue& value){
    const auto r=value.toObject();
    if(r.isEmpty())return;
    const QString axis=r.value("axis").toString();
    const float degrees=float(r.value("degrees").toDouble());
    if(axis=="x")matrix.rotate(degrees,1,0,0);
    else if(axis=="y")matrix.rotate(degrees,0,1,0);
    else if(axis=="z")matrix.rotate(degrees,0,0,1);
}
void aboutPivot(QMatrix4x4& world,const QJsonObject& rotation,const QJsonValue& pivot){
    if(rotation.isEmpty())return;
    const QVector3D local=vector(pivot);
    world.translate(local);
    rotate(world,rotation);
    world.translate(-local);
}
QMatrix4x4 mechanismTransform(const QString& id,const QString& role,
                             const QJsonObject& mechanism,const QJsonObject& solved){
    QMatrix4x4 matrix;matrix.setToIdentity();
    const auto center=vector(mechanism.value("center"));
    if(role=="crank"){
        matrix.translate(center);
        matrix.rotate(float(solved.value("crank_angle_degrees").toDouble()),1,0,0);
    }else if(role=="rod"){
        matrix.translate(center.x(),float(solved.value("pin_y").toDouble()),
                                  float(solved.value("pin_z").toDouble()));
        matrix.rotate(float(solved.value("rod_angle_degrees").toDouble()),1,0,0);
    }else if(role=="follower"){
        matrix.translate(center.x(),center.y(),float(solved.value("follower_z").toDouble()));
    }else if(role=="slider"){
        matrix.translate(0,0,float(solved.value("stroke_z").toDouble()));
    }
    Q_UNUSED(id);
    return matrix;
}
}
// Follows Java AnimatedComponentRenderer.renderSubtree, DrivenRotation and
// SliderCrankSolver: parent * [mechanism] * T(model_offset) * T(translation)
// * pivot/static rotation * pivot/driven rotation.
// Do not substitute independent sine animations for a constrained rod.
bool RigTransforms::evaluate(const QString& machine,const QJsonObject& manifest,
                             const QJsonObject& scene,const QMap<QString,double>& physicalAngles,
                             bool running,bool overloaded,bool worn,int wearStage,
                             RigPose& result,QString& error){
    RigPose computed;
    QJsonObject bound=manifest;
    bound["_studio_machine"]=machine;
    const auto sampled=VisualContract::sample(bound,scene,physicalAngles,
                                               running,overloaded,worn,wearStage,error);
    if(!error.isEmpty())return false;
    const auto definitions=scene.value("components").toObject(),
          sampledParts=sampled.value("components").toObject(),
          mechanisms=scene.value("mechanisms").toObject(),
          mechanismSamples=sampled.value("mechanisms").toObject();
    struct Role {QString mechanismId, name;};
    std::map<QString,Role> roles;
    for(auto it=mechanisms.begin();it!=mechanisms.end();++it){
        const auto mapping=it.value().toObject().value("parts").toObject();
        for(const auto& role:{"crank","rod","follower","slider"}){
            roles[mapping.value(role).toString()]={it.key(),QString::fromLatin1(role)};
            // Unique roles are already enforced by the validator.
        }
    }
    QSet<QString> seen;
    std::function<bool(QString,const QMatrix4x4&,bool)> subtree =
        [&](QString id,const QMatrix4x4& parent,bool parentVisible)->bool{
        if(seen.contains(id)){error="Unexpected cycle in rig at "+id;return false;}
        seen.insert(id);
        const auto definition=definitions.value(id).toObject();
        const auto evaluated=sampledParts.value(id).toObject();
        bool visible=parentVisible && evaluated.value("visible").toBool(true);
        QMatrix4x4 world=parent;
        const auto role=roles.find(id);
        if(role!=roles.end()){
            const auto& binding=role->second;
            world*=mechanismTransform(binding.mechanismId,binding.name,
                mechanisms.value(binding.mechanismId).toObject(),
                mechanismSamples.value(binding.mechanismId).toObject());
        }
        translate(world,definition.value("model_offset"));
        const auto trans=evaluated.value("translation").toObject();
        if(!trans.isEmpty()){
            const auto axis=trans.value("axis").toString();
            const float distance=float(trans.value("blocks").toDouble());
            if(axis=="x")world.translate(distance,0,0);
            else if(axis=="y")world.translate(0,distance,0);
            else if(axis=="z")world.translate(0,0,distance);
        }
        aboutPivot(world,definition.value("static_rotation").toObject(),definition.value("pivot"));
        aboutPivot(world,evaluated.value("rotation").toObject(),definition.value("pivot"));
        RigComponentPose pose;
        pose.visible=visible;
        pose.modelId=evaluated.value("model").toString().toStdString();
        pose.world=world;
        pose.renderLayer=definition.value("render_layer").toString("cutout");
        computed.components[id.toStdString()]=std::move(pose);
        for(auto it=definitions.begin();it!=definitions.end();++it)
            if(it.value().toObject().value("parent").toString()==id)
                if(!subtree(it.key(),world,visible))return false;
        return true;
    };
    QMatrix4x4 identity;identity.setToIdentity();
    for(auto it=definitions.begin();it!=definitions.end();++it)
        if(it.value().toObject().value("parent").isNull() ||
           !it.value().toObject().contains("parent"))
            if(!subtree(it.key(),identity,true))return false;
    if(seen.size()!=definitions.size()){
        error="Rig contains unresolved components";return false;
    }
    const auto belts=sampled.value("belt_loops").toObject();
    for(auto it=belts.begin();it!=belts.end();++it)
        computed.beltDistanceBlocks[it.key().toStdString()]=
            it.value().toObject().value("surface_distance_blocks").toDouble();
    result=std::move(computed);
    error.clear();
    return true;
}
} // namespace studio
