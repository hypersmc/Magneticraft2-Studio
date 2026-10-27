#include "VisualContract.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <functional>

namespace studio {
namespace {
bool fail(QString& error,const QString& explanation){error=explanation;return false;}
bool id(const QString& s,bool dotted=false){
    static const QRegularExpression simple("^[a-z][a-z0-9_]*$");
    static const QRegularExpression dot("^[a-z][a-z0-9_.]*$");
    return s.size()<=128&&(dotted?dot:simple).match(s).hasMatch();
}
bool resource(const QString& s){
    static const QRegularExpression re("^[a-z0-9_.-]+:[a-z0-9_./-]+$");
    return s.size()<=255 && !s.contains("..") && re.match(s).hasMatch();
}
bool str(const QJsonValue& v){return v.isString();}
bool num(const QJsonValue& v,double lim){
    return v.isDouble()&&std::isfinite(v.toDouble())&&std::abs(v.toDouble())<=lim;
}
bool vec(const QJsonValue& v){
    if(!v.isArray()||v.toArray().size()!=3)return false;
    for(const auto& n:v.toArray())if(!num(n,256))return false;
    return true;
}
QVector3D xyz(const QJsonValue& v){
    const auto a=v.toArray();
    return {float(a[0].toDouble()),float(a[1].toDouble()),float(a[2].toDouble())};
}
bool axis(const QJsonValue& value,bool positiveOnly=false){
    if(value.isString()) {
        const auto s=value.toString();
        if(s=="x"||s=="y"||s=="z")return true;
        return !positiveOnly&&(s=="-x"||s=="-y"||s=="-z");
    }
    if(positiveOnly||!vec(value))return false;
    auto a=value.toArray();
    const int count=int(a[0].toDouble()!=0)+int(a[1].toDouble()!=0)+int(a[2].toDouble()!=0);
    return count==1 && (std::abs(a[0].toDouble())==1||
                         std::abs(a[1].toDouble())==1||std::abs(a[2].toDouble())==1);
}
bool condition(const QString& s){
    return s=="always"||s=="running"||s=="stopped"||s=="overloaded"||s=="worn";
}
bool object(const QJsonObject& source,const QString& field){
    return source.contains(field)&&source.value(field).isObject();
}
bool integerOne(const QJsonObject& o,const QString& name){
    return o.value(name).isDouble()&&o.value(name).toDouble()==1.0;
}
QJsonArray triple(const QVector3D& v){return {double(v.x()),double(v.y()),double(v.z())};}
bool writeFile(const QString& path,const QJsonObject& json,QString& error){
    const QFileInfo info(path);
    if(!QDir().mkpath(info.absolutePath()))return fail(error,"Cannot create resource directory: "+info.absolutePath());
    QSaveFile file(path);
    if(!file.open(QIODevice::WriteOnly))return fail(error,file.errorString());
    const QByteArray bytes=QJsonDocument(json).toJson(QJsonDocument::Indented);
    if(file.write(bytes)!=bytes.size()){error=file.errorString();file.cancelWriting();return false;}
    if(!file.commit())return fail(error,file.errorString());
    return true;
}
QMap<QString,QStringList> physicalAngles(){
    return {
        {"bronze_mechanical_lathe",{"mechanical_input","spindle"}},
        {"mechanical_sifter",{"mechanical_input"}},
        {"primitive_grinder",{"mechanical_input"}},
        {"mechanical_ore_washer",{"trommel"}},
        {"bronze_stamp_mill",{"mechanical_input","output_shaft"}},
        {"rotary_ore_classifier",{"drum"}}
    };
}
QMap<QString,QStringList> physicalIntegers(){
    return {{"mechanical_sifter",{"sifter.wear_stage"}},
            {"bronze_stamp_mill",{"stamp_mill.wear_stage"}},
            {"rotary_ore_classifier",{"classifier.wear_stage"}}};
}
QStringList mandatory(const QString& machine){
    if(machine=="bronze_mechanical_lathe")return {"input_rotor","input_output_rotor"};
    if(machine=="primitive_grinder")return {"input_axle","grinding_rotor"};
    if(machine=="mechanical_ore_washer")return {"drum"};
    if(machine=="rotary_ore_classifier")return {"drum_rear","drum_middle","drum_front"};
    if(machine=="mechanical_sifter")return {"sieve_crank","sieve_rod","sieve_follower","sieve_carriage"};
    if(machine=="bronze_stamp_mill")
      return QString("gearbox_frame cross_bearing side_lower_mount input_coupler input_shaft input_spindle "
          "input_bevel output_bevel lower_spindle lower_drive_shaft lower_shaft_tip lower_pulley "
          "upper_pulley upper_shaft_start upper_shaft_zero upper_shaft_middle upper_shaft_end upper_shaft_tip")
          .split(' ',Qt::SkipEmptyParts);
    return {};
}
QStringList stampHeads(){
    return QString("plate_left plate_middle plate_right cam_left cam_middle cam_right "
                   "hammer_left hammer_middle hammer_right").split(' ');
}
bool isActive(const QString& w,bool running,bool overloaded,bool worn){
    return w=="always"||(w=="running"&&running)||(w=="stopped"&&!running)
        ||(w=="overloaded"&&overloaded)||(w=="worn"&&worn);
}
int priority(const QString& when){
    if(when=="overloaded")return 5;
    if(when=="worn")return 4;
    if(when=="running")return 3;
    if(when=="stopped")return 2;
    return 1;
}
// Estimate the same physical tangent/arc segmentation the Java BeltPath uses;
// Java may reject additional numerical degeneracies, so export warns to test
// belt previews in Minecraft before claiming identical tessellation.
double estimatedBeltTiles(const QJsonObject& b){
    const QVector3D a=xyz(b.value("start_center")),z=xyz(b.value("end_center"));
    const double d=double((z-a).length()),r=b.value("start_radius").toDouble(),
        R=b.value("end_radius").toDouble(),delta=r-R;
    const double alpha=std::asin(std::clamp(delta/d,-1.0,1.0));
    const double l=2.0*std::sqrt(std::max(0.0,d*d-delta*delta))
         + 3.141592653589793*(r+R)+2.0*delta*alpha;
    // conservative 2 straight pieces + 2*ceil(32) arc pieces.
    return 66.0+l/b.value("texture_repeat").toDouble()+1.0;
}
}
QStringList VisualContract::availableAngles(const QString& machine){
    return physicalAngles().value(machine);
}
QStringList VisualContract::availableIntegers(const QString& machine){
    return physicalIntegers().value(machine);
}
bool VisualContract::parseObject(const QByteArray& bytes,QJsonObject& out,QString& error){
    if(bytes.size()>2*1024*1024)return fail(error,"JSON file exceeds 2 MiB");
    QJsonParseError parse;
    const auto doc=QJsonDocument::fromJson(bytes,&parse);
    if(parse.error!=QJsonParseError::NoError || !doc.isObject())
        return fail(error,"Invalid JSON object: "+parse.errorString());
    out=doc.object();error.clear();return true;
}
bool VisualContract::validateManifest(const QJsonObject& m,const QString& machine,QString& error){
    error.clear();
    if(m.value("format").toString()!="magneticraft2:machine_visual")
        return fail(error,"Expected machine_visuals format magneticraft2:machine_visual");
    if(!integerOne(m,"format_version"))return fail(error,"machine_visual format_version must be 1");
    if(!str(m.value("animation"))||!resource(m.value("animation").toString()))
        return fail(error,"machine_visual animation must be a namespace:path");
    if(!object(m,"angle_channels")||!object(m,"integer_channels"))
        return fail(error,"angle_channels and integer_channels must both be objects");
    for(const auto& field:{"angle_channels","integer_channels"}){
        const auto channels=m.value(field).toObject();
        if(channels.size()>64)return fail(error,QString(field)+" exceeds 64 mappings");
        const bool integer=QString(field)=="integer_channels";
        for(auto it=channels.begin();it!=channels.end();++it){
            if(!id(it.key(),integer)||!str(it.value())||!id(it.value().toString(),integer))
                return fail(error,QString(field)+" has an invalid alias or source: "+it.key());
        }
    }
    if(!physicalAngles().contains(machine))
        return fail(error,"Machine ID is not implemented by a Java visual renderer: "+machine);
    const auto angles=m.value("angle_channels").toObject();
    for(auto it=angles.begin();it!=angles.end();++it)
        if(!availableAngles(machine).contains(it.value().toString()))
            return fail(error,"Java machine "+machine+" has no angle source "+it.value().toString());
    const auto ints=m.value("integer_channels").toObject();
    for(auto it=ints.begin();it!=ints.end();++it)
        if(!availableIntegers(machine).contains(it.value().toString()))
            return fail(error,"Java machine "+machine+" has no integer source "+it.value().toString());
    return true;
}
bool VisualContract::validateScene(const QJsonObject& s,QString& error){
    error.clear();
    if(s.contains("format")&&s.value("format").toString()!="magneticraft2:visual_animation")
        return fail(error,"Unknown visual animation format");
    if(!integerOne(s,"format_version"))return fail(error,"visual_animation format_version must be 1");
    if(!object(s,"coordinate_system"))return fail(error,"Missing coordinate_system");
    const auto frame=s.value("coordinate_system").toObject();
    if(frame.value("units")!="blocks"||frame.value("origin")!="controller"||
       frame.value("reference_facing")!="south")
        return fail(error,"Coordinate system must be blocks/controller/south");
    if(!object(s,"components")||!object(s,"inputs")||!object(s,"animations"))
        return fail(error,"components, inputs, animations must be objects");
    const auto components=s.value("components").toObject(),
        inputs=s.value("inputs").toObject(),animations=s.value("animations").toObject();
    if(components.size()<1||components.size()>256||inputs.size()>64||animations.size()>128)
        return fail(error,"Scene exceeds component/input/animation count limits");
    QMap<QString,QString> parents;
    for(auto it=components.begin();it!=components.end();++it){
        const QString name=it.key();
        if(!id(name)||!it.value().isObject())return fail(error,"Invalid component "+name);
        const auto c=it.value().toObject();
        if(!vec(c.value("model_offset"))||!vec(c.value("pivot")))
            return fail(error,name+": model_offset and pivot require three finite block coordinates");
        if(c.contains("model")&&!c.value("model").isNull()&&
           (!str(c.value("model"))||!resource(c.value("model").toString())))
            return fail(error,name+": invalid model resource location");
        if(c.contains("parent")&&!c.value("parent").isNull()){
            if(!str(c.value("parent"))||!id(c.value("parent").toString()))
                return fail(error,name+": invalid parent");
            parents[name]=c.value("parent").toString();
        }
        const auto layer=c.value("render_layer").toString("cutout");
        if(!QStringList{"solid","cutout","translucent"}.contains(layer))
            return fail(error,name+": invalid render_layer");
        if(!condition(c.value("visible_when").toString("always")))
            return fail(error,name+": invalid visible_when");
        if(c.contains("static_rotation")){
            if(!c.value("static_rotation").isObject())return fail(error,name+": malformed static_rotation");
            const auto r=c.value("static_rotation").toObject();
            if(!axis(r.value("axis"))||!num(r.value("degrees"),3600))
                return fail(error,name+": invalid static_rotation axis/degrees");
        }
        if(c.contains("model_variants")){
            if(!c.contains("model")||c.value("model").isNull()||
               !c.value("model_variants").isObject())return fail(error,name+": variants need a base model");
            const auto v=c.value("model_variants").toObject(),models=v.value("models").toObject();
            if(!str(v.value("source"))||!id(v.value("source").toString(),true)||
               !object(v,"models")||models.isEmpty()||models.size()>6)
                return fail(error,name+": invalid model_variants source/models");
            for(auto a=models.begin();a!=models.end();++a){
                bool ok=false;const int stage=a.key().toInt(&ok);
                if(!ok||stage<0||stage>5||QString::number(stage)!=a.key()||
                   !str(a.value())||!resource(a.value().toString()))
                    return fail(error,name+": invalid model variant stage/model "+a.key());
            }
        }
    }
    for(auto it=parents.begin();it!=parents.end();++it)
        if(!components.contains(it.value())||it.key()==it.value())
            return fail(error,"Unknown or self-parented component "+it.key());
    for(auto it=components.begin();it!=components.end();++it){
        QSet<QString> seen;QString node=it.key();int depth=0;
        while(!node.isEmpty()){
            if(seen.contains(node))return fail(error,"Component hierarchy cycle at "+node);
            seen.insert(node);node=parents.value(node);
            if(++depth>32)return fail(error,"Component hierarchy exceeds depth 32");
        }
    }
    for(auto it=inputs.begin();it!=inputs.end();++it){
        if(!id(it.key())||!it.value().isObject())return fail(error,"Invalid input "+it.key());
        const auto i=it.value().toObject();
        if(i.value("source")!="mechanical.visual_angle_degrees"||
           !str(i.value("module"))||!id(i.value("module").toString()))
            return fail(error,it.key()+": unsupported driver or invalid module alias");
    }
    QSet<QString> seenTracks;QSet<QString> mechanismRoles;
    int tracks=0;
    for(auto it=animations.begin();it!=animations.end();++it){
        if(!id(it.key())||!it.value().isObject())return fail(error,"Invalid animation "+it.key());
        const auto a=it.value().toObject();
        if(a.value("type")!="driven"||!a.value("tracks").isArray())
            return fail(error,it.key()+": only driven track arrays are supported");
        for(const auto& raw:a.value("tracks").toArray()){
            if(++tracks>512||!raw.isObject())return fail(error,"Invalid/too many animation tracks");
            const auto t=raw.toObject();
            const QString target=t.value("target").toString(),input=t.value("input").toString(),
                property=t.value("property").toString(),when=t.value("when").toString("always");
            if(!id(target)||!components.contains(target)||!id(input)||!inputs.contains(input))
                return fail(error,"Track references nonexistent target or input");
            if(!axis(t.value("axis"))||!condition(when)||
               !num(t.value("scale").isUndefined()?QJsonValue(1):t.value("scale"),128)||
               !num(t.value("phase_degrees").isUndefined()?QJsonValue(0):t.value("phase_degrees"),3600))
                return fail(error,"Invalid axis, condition, scale or phase");
            if(property!="rotation"&&property!="translation")
                return fail(error,"Unsupported track property "+property);
            const auto identity=target+"/"+property+"/"+when;
            if(seenTracks.contains(identity))return fail(error,"Duplicate driven track "+identity);
            seenTracks.insert(identity);
            if(property=="translation"){
                const double distance=t.value("distance").toDouble(),
                    bias=t.value("bias").toDouble(0),amplitude=t.value("amplitude").toDouble(1);
                if(!num(t.value("distance"),16)||
                   !num(t.value("bias").isUndefined()?QJsonValue(0):t.value("bias"),4)||
                   !num(t.value("amplitude").isUndefined()?QJsonValue(1):t.value("amplitude"),4)||
                   std::abs(distance)*(std::abs(bias)+std::abs(amplitude))>16||
                   !QStringList{"sine","cosine"}.contains(t.value("waveform").toString()))
                    return fail(error,"Invalid driven translation distance/bias/amplitude/waveform");
            }
        }
    }
    if(s.contains("mechanisms")&&!s.value("mechanisms").isObject())
        return fail(error,"mechanisms must be an object");
    const auto mechanisms=s.value("mechanisms").toObject();
    if(mechanisms.size()>16)return fail(error,"Too many mechanisms");
    for(auto it=mechanisms.begin();it!=mechanisms.end();++it){
        if(!id(it.key())||!it.value().isObject())return fail(error,"Invalid mechanism "+it.key());
        const auto m=it.value().toObject();
        if(m.value("type")!="slider_crank"||m.value("rotation_axis")!="x"||
           m.value("slide_axis")!="z"||!vec(m.value("center"))||
           !inputs.contains(m.value("input").toString()))
            return fail(error,it.key()+": only x/z slider_crank with declared input is supported");
        const double radius=m.value("radius").toDouble(),length=m.value("rod_length").toDouble();
        if(!num(m.value("radius"),16)||!num(m.value("rod_length"),16)||
           radius<=0||radius>=length||
           !num(m.value("phase_degrees").isUndefined()?QJsonValue(0):m.value("phase_degrees"),3600))
            return fail(error,it.key()+": slider_crank needs 0 < radius < rod_length <= 16");
        if(!object(m,"parts"))return fail(error,it.key()+": missing mechanism parts");
        const auto roles=m.value("parts").toObject();
        for(const auto& role:{"crank","rod","follower","slider"}){
            const QString target=roles.value(role).toString();
            if(!str(roles.value(role))||!id(target)||!components.contains(target)||
               parents.contains(target)||mechanismRoles.contains(target))
                return fail(error,it.key()+": role "+role+" must use a distinct root component");
            if(seenTracks.contains(target+"/rotation/always")||seenTracks.contains(target+"/translation/always"))
                return fail(error,it.key()+": driven track conflicts with mechanism role "+target);
            for(const auto& state:{"running","stopped","overloaded","worn"}){
                if(seenTracks.contains(target+"/rotation/"+state)||seenTracks.contains(target+"/translation/"+state))
                    return fail(error,it.key()+": driven track conflicts with mechanism role "+target);
            }
            mechanismRoles.insert(target);
        }
    }
    if(s.contains("belt_loops")&&!s.value("belt_loops").isObject())
        return fail(error,"belt_loops must be an object");
    const auto belts=s.value("belt_loops").toObject();
    if(belts.size()>8)return fail(error,"Too many belt loops");
    double budget=0;
    for(auto it=belts.begin();it!=belts.end();++it){
        if(!id(it.key())||!it.value().isObject())return fail(error,"Invalid belt "+it.key());
        const auto b=it.value().toObject();
        const QString texture=b.value("texture").toString();
        if(b.value("type")!="open_belt"||!inputs.contains(b.value("input").toString())||
           !resource(texture)||!texture.section(':',1).startsWith("textures/")||!texture.endsWith(".png")||
           !vec(b.value("start_center"))||!vec(b.value("end_center"))||
           !axis(b.value("rotation_axis"),true))
            return fail(error,it.key()+": invalid belt input/texture/geometry");
        const double r=b.value("start_radius").toDouble(),R=b.value("end_radius").toDouble(),
            w=b.value("half_width").toDouble(),t=b.value("half_thickness").toDouble(),
            repeat=b.value("texture_repeat").toDouble();
        if(!num(b.value("start_radius"),4)||!num(b.value("end_radius"),4)||
           r<=0.03||R<=0.03||
           !num(b.value("drive_radius").isUndefined()?QJsonValue(r):b.value("drive_radius"),4)||
           b.value("drive_radius").toDouble(r)<=0||
           !num(b.value("half_width"),0.5)||w<=0||
           !num(b.value("half_thickness"),0.5)||t<=0||
           !num(b.value("texture_repeat"),4)||repeat<0.0625||
           2*w>repeat||2*t>repeat||
           !num(b.value("speed_scale").isUndefined()?QJsonValue(1):b.value("speed_scale"),16)||
           !num(b.value("texture_offset").isUndefined()?QJsonValue(0):b.value("texture_offset"),64))
            return fail(error,it.key()+": invalid belt radii/tile dimensions/speed");
        const QVector3D from=xyz(b.value("start_center")),to=xyz(b.value("end_center"));
        const double span=double((to-from).length());
        if(span<0.125||span>16||span<=std::abs(r-R))
            return fail(error,it.key()+": belt span/radii invalid");
        const auto direction=(to-from).normalized(),ax=b.value("rotation_axis").toString();
        const QVector3D principal=ax=="x"?QVector3D(1,0,0):ax=="y"?QVector3D(0,1,0):QVector3D(0,0,1);
        if(QVector3D::crossProduct(principal,direction).lengthSquared()<0.0001)
            return fail(error,it.key()+": belt pulley axis is parallel to center line");
        const double tiles=estimatedBeltTiles(b);
        if(tiles>192||((budget+=tiles)>512))
            return fail(error,it.key()+": belt exceeds tessellation budget");
    }
    return true;
}
bool VisualContract::validatePair(const QString& machine,const QJsonObject& manifest,
                                  const QJsonObject& scene,QString& error,QStringList* warnings){
    if(warnings)warnings->clear();
    if(!validateManifest(manifest,machine,error)||!validateScene(scene,error))return false;
    const auto components=scene.value("components").toObject(),
        inputs=scene.value("inputs").toObject(),
        angles=manifest.value("angle_channels").toObject(),
        ints=manifest.value("integer_channels").toObject();
    for(auto it=inputs.begin();it!=inputs.end();++it){
        const QString alias=it.value().toObject().value("module").toString();
        if(!angles.contains(alias))return fail(error,"Missing manifest angle alias "+alias);
    }
    for(auto it=components.begin();it!=components.end();++it){
        const auto v=it.value().toObject().value("model_variants").toObject();
        if(!v.isEmpty()&&!ints.contains(v.value("source").toString()))
            return fail(error,"Missing manifest integer alias "+v.value("source").toString());
    }
    for(auto it=angles.begin();it!=angles.end();++it){
        bool used=false;
        for(auto input=inputs.begin();input!=inputs.end();++input)
            used|=input.value().toObject().value("module")==it.key();
        if(!used && warnings)warnings->append("Unused physical angle alias "+it.key()+"; Java still requires it");
    }
    for(const auto& required:mandatory(machine))
        if(!components.contains(required))return fail(error,"Machine "+machine+" requires component "+required);
    if(machine=="mechanical_sifter"){
        const auto mechanism=scene.value("mechanisms").toObject().value("sieve_drive").toObject();
        if(mechanism.isEmpty())return fail(error,"Sifter requires mechanisms.sieve_drive");
        const auto roles=mechanism.value("parts").toObject();
        for(const auto& role:{"crank","rod","follower","slider"}){
            const QMap<QString,QString> expected={{"crank","sieve_crank"},{"rod","sieve_rod"},
                  {"follower","sieve_follower"},{"slider","sieve_carriage"}};
            if(roles.value(role)!=expected.value(role))
                return fail(error,"Sifter sieve_drive role must use "+expected.value(role));
        }
    }
    if(machine=="bronze_stamp_mill"){
        int heads=0;for(const auto& name:stampHeads())heads+=int(components.contains(name));
        if(heads!=0&&heads!=9)return fail(error,"Stamp Mill head set must contain all nine parts or none");
    }
    if(machine!="bronze_stamp_mill" && !scene.value("belt_loops").toObject().isEmpty()
       && warnings)
        warnings->append("Java renderer for "+machine+" does not currently render generic belt_loops");
    if(!scene.contains("format") && warnings)
        warnings->append("Scene should declare magneticraft2:visual_animation for third-party namespace registration");
    error.clear();return true;
}
bool VisualContract::importPair(const QString& machine,const QByteArray& mb,const QByteArray& sb,
                                QJsonObject& manifest,QJsonObject& scene,QString& error,QStringList* warnings){
    QJsonObject m,s;
    if(!parseObject(mb,m,error)||!parseObject(sb,s,error)||
       !validatePair(machine,m,s,error,warnings))return false;
    manifest=m;scene=s;return true;
}
bool VisualContract::exportPair(const QString& directory,const QString& machine,
                                const QJsonObject& manifest,const QJsonObject& scene,
                                QString& error,QStringList* warnings){
    if(!validatePair(machine,manifest,scene,error,warnings))return false;
    const QString manifestPath=QDir(directory).filePath("assets/magneticraft2/machine_visuals/"+machine+".json");
    const QString sceneId=manifest.value("animation").toString();
    const QString scenePath=QDir(directory).filePath("assets/"+sceneId.section(':',0,0)+
                                        "/animations/"+sceneId.section(':',1)+".json");
    // Exports valid binding + animation JSON, not a complete installable pack:
    // referenced baked model files and PNGs must be supplied separately.
    if(!writeFile(scenePath,scene,error))return false;
    return writeFile(manifestPath,manifest,error);
}
}
