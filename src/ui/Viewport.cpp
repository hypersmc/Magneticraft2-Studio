#include "Viewport.hpp"
#include "core/Modeling.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QFont>
#include <QOpenGLContext>
#include <QVector2D>
#include <QVector4D>
#include <QWheelEvent>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>

namespace studio {

Viewport::Viewport(QWidget* parent):QOpenGLWidget(parent),mailbox_(std::make_shared<MeshMailbox>()) {
    setMinimumSize(350,280);
    setFocusPolicy(Qt::StrongFocus);
    pool_.setMaxThreadCount(1);
    setMouseTracking(true);
    connect(&poll_,&QTimer::timeout,this,&Viewport::pollResults);
    poll_.start(32);
    setTool('v');
}
Viewport::~Viewport(){
    poll_.stop();
    mailbox_->generation.fetch_add(1);
    pool_.waitForDone();
    if(glReady_ && context()) {
        makeCurrent();
        solid_.destroy();lines_.destroy();vao_.destroy();
        shader_.removeAllShaders();
        doneCurrent();
    }
}
void Viewport::setScene(const Project& document,BuildOptions options){
    document_=document;options_=options;
    rebuildAsync();
}
void Viewport::setSelection(int i){
    options_.selectedPart=i;
    rebuildAsync();
}
void Viewport::setTool(char tool){
    if(tool=='v'||tool=='b'||tool=='w'||tool=='e'||tool=='r')activeTool_=tool;
    update();
}
void Viewport::setAxis(char axis){if(axis=='x'||axis=='y'||axis=='z')activeAxis_=axis;update();}
void Viewport::setWorkPlane(double y){workPlane_=y;options_.gridY=y;rebuildAsync();update();}
void Viewport::setDrawHeight(double height){drawHeight_=std::max(1.0/16.0,height);update();}
void Viewport::setGridStep(double step){gridStep_=std::clamp(step,1.0/16.0,1.0);update();}
void Viewport::setCameraPreset(int preset){
    cameraPreset_=std::clamp(preset,0,3);
    update();
}

void Viewport::resetCamera(){
    const auto& d=document_.dimensions;
    target_={d[0]*0.5f,d[1]*0.5f,d[2]*0.5f};
    const double diagonal=std::sqrt(double(d[0]*d[0]+d[1]*d[1]+d[2]*d[2]));
    distance_=std::max(5.0,diagonal*1.5);
    yaw_=0.7;pitch_=0.55;update();
}
void Viewport::rebuildAsync(){
    const auto generation=mailbox_->generation.fetch_add(1)+1;
    const Project snapshot=document_;
    const BuildOptions options=options_;
    auto mailbox=mailbox_;
    pool_.start([snapshot,options,generation,mailbox](){
        if(generation!=mailbox->generation.load())return;
        auto done=std::make_shared<Geometry>(buildGeometry(snapshot,options));
        if(generation!=mailbox->generation.load())return;
        std::lock_guard<std::mutex> lock(mailbox->mutex);
        mailbox->ready=std::move(done);
        mailbox->readyGeneration=generation;
    });
}
void Viewport::pollResults(){
    std::shared_ptr<Geometry> received;
    {
        std::lock_guard<std::mutex> lock(mailbox_->mutex);
        if(mailbox_->ready && mailbox_->readyGeneration==mailbox_->generation.load())
            received=std::move(mailbox_->ready);
        else mailbox_->ready.reset();
    }
    if(received){mesh_=std::move(*received);dirtyGpu_=true;update();}
}
void Viewport::initializeGL(){
    glReady_=initializeOpenGLFunctions();
    if(!glReady_)return;
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);
    constexpr const char* vertex=R"(#version 330 core
layout(location=0) in vec3 aPosition;
layout(location=1) in vec3 aColor;
layout(location=2) in vec3 aNormal;
uniform mat4 uMvp;
out vec3 vColor;
out vec3 vNormal;
void main(){gl_Position=uMvp*vec4(aPosition,1.0);vColor=aColor;vNormal=aNormal;}
)";
    constexpr const char* fragment=R"(#version 330 core
in vec3 vColor;
in vec3 vNormal;
out vec4 fragColor;
void main(){
    float l=length(vNormal)<0.1?1.0:(0.54+0.46*max(0.0,dot(normalize(vNormal),normalize(vec3(0.48,0.9,0.4)))));
    fragColor=vec4(vColor*l,1.0);
}
)";
    glReady_ = shader_.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex) &&
        shader_.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment) && shader_.link();
    if(!glReady_) { qWarning("Studio shader failed: %s", qPrintable(shader_.log())); return; }
    vao_.create();solid_.create();lines_.create();
    dirtyGpu_=true;
}
void Viewport::resizeGL(int w,int h){Q_UNUSED(w);Q_UNUSED(h);updateCameraMatrices();}
void Viewport::updateCameraMatrices(){
    const float cp=float(std::cos(pitch_)), sp=float(std::sin(pitch_));
    const float cy=float(std::cos(yaw_)),sy=float(std::sin(yaw_));
    QVector3D eye=target_+QVector3D(float(distance_)*cp*sy,float(distance_)*sp,float(distance_)*cp*cy);
    QVector3D up(0,1,0);
    if(cameraPreset_==1){ eye=target_+QVector3D(0,float(distance_),0);up=QVector3D(0,0,-1); }
    if(cameraPreset_==2){ eye=target_+QVector3D(0,0,float(distance_)); }
    if(cameraPreset_==3){ eye=target_+QVector3D(float(distance_),0,0); }
    view_.setToIdentity();view_.lookAt(eye,target_,up);
    projection_.setToIdentity();
    const float aspect=float(std::max(1,width()))/float(std::max(1,height()));
    if(cameraPreset_!=0){
        const float halfHeight=float(distance_)*0.55f;
        projection_.ortho(-halfHeight*aspect,halfHeight*aspect,-halfHeight,halfHeight,0.05f,2500.0f);
    }else projection_.perspective(46.0f,aspect,0.05f,2500.0f);
    mvp_=projection_*view_;
}
void Viewport::drawBuffer(QOpenGLBuffer& buffer,const std::vector<Vertex>& vertices,GLenum primitive){
    if(vertices.empty())return;
    buffer.bind();
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glEnableVertexAttribArray(2);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,x)));
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,r)));
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,nx)));
    glDrawArrays(primitive,0,GLsizei(vertices.size()));
    buffer.release();
}
void Viewport::uploadBuffers(){
    if(!dirtyGpu_)return;
    solid_.bind();solid_.allocate(mesh_.triangles.data(),int(mesh_.triangles.size()*sizeof(Vertex)));solid_.release();
    lines_.bind();lines_.allocate(mesh_.lines.data(),int(mesh_.lines.size()*sizeof(Vertex)));lines_.release();
    dirtyGpu_=false;
}
void Viewport::paintGL(){
    if(!glReady_) {glClearColor(0.075f,0.11f,0.14f,1);glClear(GL_COLOR_BUFFER_BIT);return;}
    glViewport(0,0,int(width()*devicePixelRatioF()),int(height()*devicePixelRatioF()));
    glClearColor(0.055f,0.089f,0.118f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    updateCameraMatrices();
    uploadBuffers();
    shader_.bind();shader_.setUniformValue("uMvp",mvp_);
    vao_.bind();
    drawBuffer(solid_,mesh_.triangles,GL_TRIANGLES);
    drawBuffer(lines_,mesh_.lines,GL_LINES);
    vao_.release();shader_.release();
    if(options_.showFormed)drawModelOverlay();
}
void Viewport::wheelEvent(QWheelEvent* e){
    distance_=std::clamp(distance_*std::exp(-e->angleDelta().y()*0.001),1.2,1800.0);
    update();e->accept();
}
static bool rayIntersect(QVector3D origin,QVector3D direction,Bounds b,float& t){
    double entry=0,exit=1e20;
    const double o[]={origin.x(),origin.y(),origin.z()};
    const double d[]={direction.x(),direction.y(),direction.z()};
    const double mn[]={b.from.x,b.from.y,b.from.z},mx[]={b.to.x,b.to.y,b.to.z};
    for(int i=0;i<3;i++){
        if(std::abs(d[i])<1e-10) {if(o[i]<mn[i]||o[i]>mx[i])return false;continue;}
        double a=(mn[i]-o[i])/d[i],z=(mx[i]-o[i])/d[i];if(a>z)std::swap(a,z);
        entry=std::max(entry,a);exit=std::min(exit,z);
        if(entry>exit)return false;
    }
    t=float(entry);return exit>=0;
}
int Viewport::pickPart(const QPoint& at) const {
    bool valid=false;const QMatrix4x4 inverse=mvp_.inverted(&valid);
    if(!valid)return -1;
    const float nx=2.0f*(float(at.x())/float(std::max(1,width())))-1.0f;
    const float ny=1.0f-2.0f*(float(at.y())/float(std::max(1,height())));
    QVector4D a=inverse*QVector4D(nx,ny,-1,1),b=inverse*QVector4D(nx,ny,1,1);
    if(std::abs(a.w())<1e-7||std::abs(b.w())<1e-7)return -1;
    QVector3D origin=a.toVector3DAffine();
    QVector3D direction=(b.toVector3DAffine()-origin).normalized();
    int index=-1;float best=std::numeric_limits<float>::infinity();
    for(int i=0;i<int(document_.parts.size());i++){
        const auto& part=document_.parts[std::size_t(i)];
        if(!part.visible)continue;
        float t=0;
        if(rayIntersect(origin,direction,transformedBounds(part),t)&&t<best){best=t;index=i;}
    }
    return index;
}
QPointF Viewport::projectPoint(Vec3 v) const {
    auto projected=mvp_*QVector4D(float(v.x),float(v.y),float(v.z),1);
    if(std::abs(projected.w())<1e-7)return {};
    return {(projected.x()/projected.w()*0.5+0.5)*width(),
        (0.5-projected.y()/projected.w()*0.5)*height()};
}
bool Viewport::workPlaneAt(const QPoint& screen,Vec3& result) const {
    bool invertible=false;
    const auto inverse=mvp_.inverted(&invertible);
    if(!invertible)return false;
    const float nx=2.0f*float(screen.x())/float(std::max(1,width()))-1.0f;
    const float ny=1.0f-2.0f*float(screen.y())/float(std::max(1,height()));
    auto a=inverse*QVector4D(nx,ny,-1.0f,1.0f);
    auto b=inverse*QVector4D(nx,ny,1.0f,1.0f);
    if(std::abs(a.w())<1e-7f||std::abs(b.w())<1e-7f)return false;
    const auto ray=a.toVector3DAffine();
    const auto direction=b.toVector3DAffine()-ray;
    if(std::abs(direction.y())<1e-8f)return false;
    const double t=(workPlane_-ray.y())/direction.y();
    if(t<0 || t>1.0)return false;
    const auto hit=ray+direction*float(t);
    if(!std::isfinite(hit.x())||!std::isfinite(hit.z()))return false;
    result={snapGrid(hit.x(),gridStep_),workPlane_,snapGrid(hit.z(),gridStep_)};
    return true;
}
char Viewport::gizmoAt(const QPoint& screen) const {
    if(options_.selectedPart<0||options_.selectedPart>=int(document_.parts.size()))return 0;
    const Part& part=document_.parts[std::size_t(options_.selectedPart)];
    const double length=std::max(0.7,std::min(2.5,(part.bounds.to.x-part.bounds.from.x+part.bounds.to.y-part.bounds.from.y+part.bounds.to.z-part.bounds.from.z)*0.28));
    const QPointF p=projectPoint(part.pivot);
    char best=0;double closest=12.0;
    for(const auto pair:std::array<std::pair<char,Vec3>,3>{{{'x',{length,0,0}},{'y',{0,length,0}},{'z',{0,0,length}}}}){
        const QPointF e=projectPoint(part.pivot+pair.second);
        const QPointF ab=e-p;const QPointF ap=QPointF(screen)-p;
        const double len=ab.x()*ab.x()+ab.y()*ab.y();
        if(len<100.0)continue;
        const double u=std::clamp((ap.x()*ab.x()+ap.y()*ab.y())/len,0.15,1.2);
        const QPointF nearest=p+ab*u;
        const double dist=std::hypot(screen.x()-nearest.x(),screen.y()-nearest.y());
        if(dist<closest){closest=dist;best=pair.first;}
    }
    return best;
}
void Viewport::drawModelOverlay(){
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing,true);
    painter.setFont(QFont("Sans Serif",10));
    const QString mode=activeTool_=='b'?"DRAW BOX":activeTool_=='v'?"SELECT":activeTool_=='w'?"MOVE":activeTool_=='e'?"ROTATE":"RESIZE";
    const QString preset=cameraPreset_==1?"TOP":cameraPreset_==2?"FRONT":cameraPreset_==3?"RIGHT":"PERSPECTIVE";
    painter.fillRect(QRect(12,12,400,38),QColor(18,29,39,210));
    painter.setPen(QColor(231,198,152));
    painter.drawText(QRect(24,12,380,38),Qt::AlignVCenter,
        QString("MODEL  /  %1  ·  %2  ·  SNAP %3").arg(mode,preset,QString::number(gridStep_,'g',3)));
    painter.setPen(QColor(153,183,198));
    painter.drawText(QRect(20,height()-36,width()-30,30),Qt::AlignVCenter,
        activeTool_=='b'?"Left-drag across the workplane to create a cube  ·  Scroll to zoom  ·  Right-drag to pan":
        "Click a part to select  ·  Drag XYZ handles with Move/Rotate/Resize  ·  Left-drag empty space to orbit");
    if(drawing_ && hasDrawPreview_){
        const Bounds b=drawnCuboid(drawStart_,drawEnd_,workPlane_,drawHeight_,gridStep_);
        const auto a=b.from,c=b.to;
        const Vec3 vertices[]={{a.x,a.y,a.z},{c.x,a.y,a.z},{c.x,c.y,a.z},{a.x,c.y,a.z},
            {a.x,a.y,c.z},{c.x,a.y,c.z},{c.x,c.y,c.z},{a.x,c.y,c.z}};
        const int edges[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        painter.setPen(QPen(QColor(253,183,83),2,Qt::DashLine));
        for(const auto &edge:edges)painter.drawLine(projectPoint(vertices[edge[0]]),projectPoint(vertices[edge[1]]));
    }
    if(options_.selectedPart>=0 && options_.selectedPart<int(document_.parts.size())) {
        const Part& part=document_.parts[std::size_t(options_.selectedPart)];
        const double length=std::max(0.7,std::min(2.5,(part.bounds.to.x-part.bounds.from.x+part.bounds.to.y-part.bounds.from.y+part.bounds.to.z-part.bounds.from.z)*0.28));
        const QPointF p=projectPoint(part.pivot);
        for(const auto pair:std::array<std::pair<char,Vec3>,3>{{{'x',{length,0,0}},{'y',{0,length,0}},{'z',{0,0,length}}}}){
            const QColor color=pair.first=='x'?QColor("#fb625b"):pair.first=='y'?QColor("#6cdb85"):QColor("#66a1fc");
            const QPointF e=projectPoint(part.pivot+pair.second);
            painter.setPen(QPen(color,pair.first==activeAxis_?4:2));painter.drawLine(p,e);
            painter.setBrush(color);painter.drawEllipse(e,5,5);
            painter.setPen(QColor("#f7fafc"));painter.drawText(e+QPointF(7,-8),QString(QChar(pair.first).toUpper()));
        }
    }
}
void Viewport::mousePressEvent(QMouseEvent* event){
    setFocus(Qt::MouseFocusReason);
    lastMouse_=event->pos();
    orbiting_=panning_=transforming_=drawing_=selecting_=false;
    if(event->button()==Qt::MiddleButton || event->button()==Qt::RightButton){panning_=true;return;}
    if(event->button()!=Qt::LeftButton)return;
    updateCameraMatrices();
    if(options_.showFormed){
        if(activeTool_=='b'){
            Vec3 point;
            if(workPlaneAt(event->pos(),point)){
                drawStart_=drawEnd_=point;drawing_=hasDrawPreview_=true;update();
            }
            return;
        }
        if(activeTool_=='w'||activeTool_=='e'||activeTool_=='r'){
            const char axis=gizmoAt(event->pos());
            if(axis){
                activeAxis_=axis;emit axisPicked(axis);
                transforming_=true;dragAccum_=emittedDrag_=0;emit beginEdit();return;
            }
        }
        const int hit=pickPart(event->pos());
        if(hit>=0){
            if(hit!=options_.selectedPart){options_.selectedPart=hit;emit partSelected(hit);rebuildAsync();}
            if(activeTool_!='v'){
                transforming_=true;dragAccum_=emittedDrag_=0;emit beginEdit();
            }else selecting_=true;
            return;
        }
        emit partSelected(-1);
    }
    orbiting_=true;
}
void Viewport::mouseMoveEvent(QMouseEvent* event){
    const QPoint delta=event->pos()-lastMouse_;
    lastMouse_=event->pos();
    if(drawing_){
        updateCameraMatrices();
        Vec3 point;
        if(workPlaneAt(event->pos(),point)){drawEnd_=point;hasDrawPreview_=true;update();}
        return;
    }
    if(orbiting_) {
        if(cameraPreset_!=0){cameraPreset_=0;emit cameraPresetChanged(0);}
        yaw_+=delta.x()*0.008;pitch_=std::clamp(pitch_+delta.y()*0.008,-1.40,1.40);
        update();return;
    }
    if(panning_){
        const float speed=float(distance_*0.0018);
        const QVector3D right(float(std::cos(yaw_)),0,float(-std::sin(yaw_)));
        target_-=right*(delta.x()*speed);target_+=QVector3D(0,1,0)*(delta.y()*speed);
        update();return;
    }
    if(!transforming_||options_.selectedPart<0||options_.selectedPart>=int(document_.parts.size()))return;
    updateCameraMatrices();
    const Part& part=document_.parts[std::size_t(options_.selectedPart)];
    Vec3 axis={activeAxis_=='x'?1.0:0.0,activeAxis_=='y'?1.0:0.0,activeAxis_=='z'?1.0:0.0};
    const QPointF before=projectPoint(part.pivot);
    const QPointF after=projectPoint(part.pivot+axis);
    const QVector2D vector(float(after.x()-before.x()),float(after.y()-before.y()));
    const QVector2D mouse(float(delta.x()),float(delta.y()));
    const double len2=QVector2D::dotProduct(vector,vector);
    double deltaUnits=len2>=40?QVector2D::dotProduct(mouse,vector)/len2:double(delta.x())/40.0;
    if(activeTool_=='e')deltaUnits*=45.0;
    dragAccum_+=deltaUnits;
    const double snap=activeTool_=='e'?5.0:gridStep_;
    const double candidate=std::round(dragAccum_/snap)*snap;
    const double motion=candidate-emittedDrag_;
    if(std::abs(motion)>1e-8){
        emittedDrag_=candidate;
        emit deltaEdit(motion,activeAxis_,activeTool_);
    }
}
void Viewport::mouseReleaseEvent(QMouseEvent* event){
    if(drawing_ && hasDrawPreview_ && event->button()==Qt::LeftButton){
        emit boxDrawn(drawStart_.x,drawStart_.z,drawEnd_.x,drawEnd_.z);
    }
    if(transforming_)emit endEdit();
    transforming_=orbiting_=panning_=drawing_=selecting_=hasDrawPreview_=false;
    update();
}

} // namespace studio
