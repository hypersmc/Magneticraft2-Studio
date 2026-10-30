#include "VisualTemplates.hpp"
#include "VisualContract.hpp"
#include <QFile>
namespace studio {
QStringList VisualTemplates::machines(){
    return {"bronze_mechanical_lathe","mechanical_sifter","primitive_grinder",
            "mechanical_ore_washer","bronze_stamp_mill","rotary_ore_classifier"};
}
bool VisualTemplates::bundled(const QString& machine,QJsonObject& manifest,
                              QJsonObject& scene,QString& error){
    if(!machines().contains(machine)){
        error="No bundled v1 machine definition for "+machine;return false;
    }
    const QString prefix=":/studio/visual-v1/";
    QFile m(prefix+"manifest/"+machine+".json"),s(prefix+"scene/"+machine+".json");
    if(!m.open(QIODevice::ReadOnly)||!s.open(QIODevice::ReadOnly)){
        error="Studio's machine template resources are unavailable: "+machine;
        return false;
    }
    return VisualContract::importPair(machine,m.readAll(),s.readAll(),
                                      manifest,scene,error);
}
} // namespace studio
