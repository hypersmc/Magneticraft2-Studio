#include "RigBeltMesh.hpp"
#include <QJsonArray>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <vector>

namespace studio {
namespace {
constexpr double pi=3.1415926535897932384626433832795;
struct Segment {
    QVector3D from,to,width,thickness;
    double start=0,end=0;
};
QVector3D vec(const QJsonValue& value){
    const auto v=value.toArray();
    return {float(v[0].toDouble()),float(v[1].toDouble()),float(v[2].toDouble())};
}
double dot(const QVector3D& a,const QVector3D& b){
    return QVector3D::dotProduct(a,b);
}
QVector3D radial(double angle,const QVector3D& run,const QVector3D& side){
    return (run*float(std::cos(angle))+side*float(std::sin(angle))).normalized();
}
void append(std::vector<Segment>& segments,QVector3D a,QVector3D b,
            QVector3D width,QVector3D thickness,double& traveled){
    const double length=double((b-a).length());
    if(length<1e-6)return;
    segments.push_back({a,b,width.normalized(),thickness.normalized(),traveled,traveled+length});
    traveled+=length;
}
void arc(std::vector<Segment>& segments,QVector3D center,QVector3D first,
         QVector3D last,QVector3D outer,QVector3D run,QVector3D side,
         QVector3D axis,double radius,double& traveled){
    const double a0=std::atan2(dot(first,side),dot(first,run)),
                 a1=std::atan2(dot(last,side),dot(last,run));
    double shortDelta=a1-a0;
    while(shortDelta<=-pi)shortDelta+=2*pi;
    while(shortDelta>pi)shortDelta-=2*pi;
    const double longDelta=shortDelta>=0?shortDelta-2*pi:shortDelta+2*pi;
    const double shortScore=dot(radial(a0+shortDelta*0.5,run,side),outer);
    const double longScore=dot(radial(a0+longDelta*0.5,run,side),outer);
    const double delta=longScore>shortScore?longDelta:shortDelta;
    const int count=std::max(4,int(std::ceil(16.0*std::abs(delta)/pi)));
    QVector3D previousRadial=first;
    QVector3D previousPoint=center+first*float(radius);
    for(int i=1;i<=count;i++){
        const QVector3D now=radial(a0+delta*i/double(count),run,side);
        const QVector3D point=center+now*float(radius);
        QVector3D thickness=previousRadial+now;
        if(thickness.lengthSquared()<1e-4)thickness=now;
        else thickness.normalize();
        append(segments,previousPoint,point,axis,thickness,traveled);
        previousRadial=now;previousPoint=point;
    }
}
void vertex(Geometry& geometry,QVector3D at,QVector3D normal,float u,float v){
    geometry.triangles.push_back({at.x(),at.y(),at.z(),1,1,1,
                                 normal.x(),normal.y(),normal.z(),u,v});
}
void quad(Geometry& geometry,QVector3D a,QVector3D b,QVector3D c,QVector3D d,
          QVector3D normal,float u0,float v0,float u1,float v1,
          float u2,float v2,float u3,float v3){
    const QVector3D n=normal.normalized();
    vertex(geometry,a,n,u0,v0);vertex(geometry,b,n,u1,v1);vertex(geometry,c,n,u2,v2);
    vertex(geometry,a,n,u0,v0);vertex(geometry,c,n,u2,v2);vertex(geometry,d,n,u3,v3);
}
void renderSegment(Geometry& mesh,const Segment& s,double halfWidth,double halfThickness,double repeat){
    const QVector3D w=s.width*float(halfWidth),t=s.thickness*float(halfThickness);
    const QVector3D a0=s.from-w-t,a1=s.from+w-t,a2=s.from+w+t,a3=s.from-w+t;
    const QVector3D b0=s.to-w-t,b1=s.to+w-t,b2=s.to+w+t,b3=s.to-w+t;
    const float u0=float(s.start/repeat),u1=float(s.end/repeat);
    const float vw0=float(0.5-halfWidth/repeat),vw1=float(0.5+halfWidth/repeat);
    const float vt0=float(0.5-halfThickness/repeat),vt1=float(0.5+halfThickness/repeat);
    quad(mesh,a3,a2,b2,b3,s.thickness,u0,vw0,u0,vw1,u1,vw1,u1,vw0);
    quad(mesh,a0,b0,b1,a1,-s.thickness,u0,vw0,u1,vw0,u1,vw1,u0,vw1);
    quad(mesh,a1,b1,b2,a2,s.width,u0,vt0,u1,vt0,u1,vt1,u0,vt1);
    quad(mesh,a0,a3,b3,b0,-s.width,u0,vt0,u0,vt1,u1,vt1,u1,vt0);
}
}
bool RigBeltMesh::build(const QJsonObject& belt,Geometry& out,QString& error){
    out={};
    if(belt.value("type")!="open_belt"){
        error="Only open_belt is implemented in Magneticraft2 runtime v1";
        return false;
    }
    const QVector3D start=vec(belt.value("start_center")),end=vec(belt.value("end_center"));
    const double D=(end-start).length();
    const double firstR=belt.value("start_radius").toDouble(),
                 endR=belt.value("end_radius").toDouble(),
                 repeat=belt.value("texture_repeat").toDouble();
    const auto ax=belt.value("rotation_axis").toString();
    const QVector3D axis=ax=="x"?QVector3D(1,0,0):ax=="y"?QVector3D(0,1,0):QVector3D(0,0,1);
    if(D<0.125||D>16||firstR<=0.03||endR<=0.03||
       D<=std::abs(firstR-endR)||repeat<0.0625||repeat>4||
       !QStringList{"x","y","z"}.contains(ax)){
        error="Invalid BeltPath geometry (validate the scene against the Java contract first)";
        return false;
    }
    const QVector3D run=(end-start).normalized();
    QVector3D side=QVector3D::crossProduct(axis,run);
    if(side.lengthSquared()<1e-4){
        error="Belt pulley shaft axis is parallel to pulley separation";
        return false;
    }
    side.normalize();
    const float fraction=float((firstR-endR)/D);
    const float tangent=float(std::sqrt(std::max(0.0,1.0-double(fraction)*fraction)));
    const QVector3D top=(run*fraction+side*tangent).normalized();
    const QVector3D bottom=(run*fraction-side*tangent).normalized();
    std::vector<Segment> segments;
    segments.reserve(80);
    double traveled=0;
    append(segments,start+top*float(firstR),end+top*float(endR),axis,top,traveled);
    arc(segments,end,top,bottom,run,run,side,axis,endR,traveled);
    append(segments,end+bottom*float(endR),start+bottom*float(firstR),axis,bottom,traveled);
    arc(segments,start,bottom,top,-run,run,side,axis,firstR,traveled);
    const auto count=double(segments.size())+traveled/repeat+1.0;
    if(segments.empty()||count>192.0){
        error="Open belt could not produce a valid bounded Java-compatible loop";
        return false;
    }
    const double hw=belt.value("half_width").toDouble();
    const double ht=belt.value("half_thickness").toDouble();
    out.triangles.reserve(segments.size()*24);
    for(const auto& s:segments)renderSegment(out,s,hw,ht,repeat);
    out.draws.push_back({0,out.triangles.size(),
        belt.value("texture").toString().toStdString(),"belt",true});
    out.visibleObjects=1;
    error.clear();return true;
}
} // namespace studio
