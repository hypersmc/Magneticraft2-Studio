#include "Viewport.hpp"
#include "core/Modeling.hpp"

#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QPainterPath>
#include <QPolygonF>
#include <QFont>
#include <QLabel>
#include <QByteArray>
#include <QImage>
#include <QResizeEvent>
#include <QOpenGLContext>
#include <QVector2D>
#include <QVector4D>
#include <QWheelEvent>

#include <algorithm>
#include <cstddef>
#include <cmath>
#include <limits>
#include <tuple>
#include <utility>
#include <functional>

namespace studio {

Viewport::Viewport(QWidget* parent):QOpenGLWidget(parent),mailbox_(std::make_shared<MeshMailbox>()) {
    setMinimumSize(350,280);
    setFocusPolicy(Qt::StrongFocus);
    pool_.setMaxThreadCount(1);
    setMouseTracking(true);
    connect(&poll_,&QTimer::timeout,this,&Viewport::pollResults);
    poll_.start(32);
    hud_=new QLabel(this);
    hud_->setObjectName("ModelHud");
    hud_->setAttribute(Qt::WA_TransparentForMouseEvents);
    hud_->setStyleSheet("QLabel { background: rgba(20,32,43,220); color: #f1c68f; padding: 8px 12px; border-radius: 4px; font-weight: 600; }");
    hud_->setFixedHeight(35);
    help_=new QLabel(this);
    help_->setObjectName("ModelHelp");
    help_->setAttribute(Qt::WA_TransparentForMouseEvents);
    help_->setStyleSheet("QLabel { background: rgba(20,32,43,190); color: #b4c7d5; padding: 5px 12px; border-radius: 4px; }");
    help_->setFixedHeight(29);
    setTool('v');
    updateHud();
}
Viewport::~Viewport(){
    poll_.stop();
    mailbox_->generation.fetch_add(1);
    pool_.waitForDone();
    if(glReady_ && context()) {
        makeCurrent();
        textureCache_.clear();
        solid_.destroy();lines_.destroy();vao_.destroy();
        shader_.removeAllShaders();
        doneCurrent();
    }
}
void Viewport::setScene(const Project& document,BuildOptions options){
    document_=document;options_=options;
    updateHud();
    rebuildAsync();
}
void Viewport::setSelection(int i){
    options_.selectedPart=i;
    rebuildAsync();
}
void Viewport::setTool(char tool){
    if(tool=='v'||tool=='b'||tool=='w'||tool=='e'||tool=='r')activeTool_=tool;
    dragHandle_=0;
    updateHud();update();
}
void Viewport::setAxis(char axis){if(axis=='x'||axis=='y'||axis=='z')activeAxis_=axis;update();}
void Viewport::setWorkPlane(double y){workPlane_=y;options_.gridY=y;updateHud();rebuildAsync();update();}
void Viewport::setDrawHeight(double height){drawHeight_=std::max(1.0/16.0,height);update();}
void Viewport::setGridStep(double step){gridStep_=std::clamp(step,1.0/16.0,1.0);updateHud();update();}
void Viewport::setCameraPreset(int preset){
    cameraPreset_=std::clamp(preset,0,3);
    updateHud();update();
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
layout(location=3) in vec2 aUv;
uniform mat4 uMvp;
out vec3 vColor;
out vec3 vNormal;
out vec2 vUv;
void main(){gl_Position=uMvp*vec4(aPosition,1.0);vColor=aColor;vNormal=aNormal;vUv=aUv;}
)";
    constexpr const char* fragment=R"(#version 330 core
in vec3 vColor;
in vec3 vNormal;
in vec2 vUv;
uniform sampler2D uTexture;
uniform bool uTextured;
out vec4 fragColor;
void main(){
    float l=length(vNormal)<0.1?1.0:(0.54+0.46*max(0.0,dot(normalize(vNormal),normalize(vec3(0.48,0.9,0.4)))));
    vec4 diffuse=uTextured?texture(uTexture,vec2(vUv.x,1.0-vUv.y)):vec4(1.0);
    fragColor=vec4(vColor*diffuse.rgb*l,diffuse.a);
}
)";
    glReady_ = shader_.addShaderFromSourceCode(QOpenGLShader::Vertex,vertex) &&
        shader_.addShaderFromSourceCode(QOpenGLShader::Fragment,fragment) && shader_.link();
    if(!glReady_) { qWarning("Studio shader failed: %s", qPrintable(shader_.log())); return; }
    vao_.create();solid_.create();lines_.create();
    dirtyGpu_=true;
}
void Viewport::resizeGL(int w,int h){Q_UNUSED(w);Q_UNUSED(h);updateCameraMatrices();}
void Viewport::resizeEvent(QResizeEvent* event){
    QOpenGLWidget::resizeEvent(event);
    if(hud_){hud_->move(12,12);hud_->raise();}
    if(help_){help_->setGeometry(12,std::max(10,height()-42),std::max(50,width()-24),29);help_->raise();}
}
void Viewport::updateHud(){
    if(!hud_||!help_)return;
    const bool visible=options_.showFormed;
    hud_->setVisible(visible);help_->setVisible(visible);
    const QString tool=activeTool_=='b'?"DRAW CUBE":activeTool_=='v'?"SELECT":activeTool_=='w'?"MOVE":activeTool_=='e'?"ROTATE":"RESIZE";
    const QString camera=cameraPreset_==1?"TOP":cameraPreset_==2?"FRONT":cameraPreset_==3?"RIGHT":"PERSPECTIVE";
    hud_->setText(QString("MODEL  /  %1    ·    %2    ·    SNAP %3    ·    Y %4")
        .arg(tool,camera,QString::number(gridStep_,'g',3),QString::number(workPlane_,'f',2)));
    hud_->adjustSize();
    help_->setText(activeTool_=='b'?"Click-drag grid: draw cuboid   ·   Scroll: zoom   ·   Right-drag: pan":
        "Click: select   ·   RGB: axis   ·   Color squares: plane   ·   White center: free/uniform   ·   Scroll: zoom");
}

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
    glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);glEnableVertexAttribArray(2);glEnableVertexAttribArray(3);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,x)));
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,r)));
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,nx)));
    glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,u)));
    glDrawArrays(primitive,0,GLsizei(vertices.size()));
    buffer.release();
}
void Viewport::uploadBuffers(){
    if(!dirtyGpu_)return;
    solid_.bind();solid_.allocate(mesh_.triangles.data(),int(mesh_.triangles.size()*sizeof(Vertex)));solid_.release();
    lines_.bind();lines_.allocate(mesh_.lines.data(),int(mesh_.lines.size()*sizeof(Vertex)));lines_.release();
    dirtyGpu_=false;
}
// Generate a small Minecraft-like fallback so a fresh cuboid is not a flat
// colored prototype. User-imported PNGs always override this preview texture.
static QImage fallbackPixelArt(const std::string& material){
    QImage image(16,16,QImage::Format_RGBA8888);
    int baseR=148,baseG=107,baseB=73;
    if(material=="copper"){baseR=181;baseG=104;baseB=70;}
    else if(material=="stone"){baseR=116;baseG=124;baseB=132;}
    else if(material=="iron"){baseR=168;baseG=181;baseB=187;}
    else if(material=="dark"){baseR=76;baseG=90;baseB=100;}
    else if(material=="glass"){baseR=79;baseG=160;baseB=157;}
    else if(material=="water"){baseR=42;baseG=100;baseB=165;}
    for(int y=0;y<16;y++)for(int x=0;x<16;x++){
        // Deterministic and pixel-aligned; no blurry interpolated noise.
        const unsigned h=unsigned(x*16777619u)^unsigned(y*2166136261u)^unsigned((x+y*13)*131u);
        const int variation=int(h%19)-9;
        const int grain=material=="wood"?((y%4==0)?-19:((x+2*y)%13==0?-14:0)):0;
        auto channel=[&](int v){return std::clamp(v+variation+grain,0,255);};
        image.setPixelColor(x,y,QColor(channel(baseR),channel(baseG),channel(baseB)));
    }
    return image;
}
QOpenGLTexture* Viewport::getTexture(const std::string& id,const std::string& material){
    const auto source=document_.textures.find(id);
    const bool embedded=!id.empty()&&source!=document_.textures.end();
    const std::string key=embedded?id:"__fallback/"+material;
    const std::size_t signature=embedded?std::hash<std::string>{}(source->second):0;
    auto found=textureCache_.find(key);
    if(found!=textureCache_.end()&&found->second.signature==signature)return found->second.gpu.get();
    QImage image;
    if(embedded){
        const QByteArray bytes=QByteArray::fromBase64(QByteArray::fromStdString(source->second));
        image=QImage::fromData(bytes,"PNG");
        if(image.width()>4096||image.height()>4096)image=QImage();
    }
    if(image.isNull())image=fallbackPixelArt(material);
    // OpenGL UV origin is bottom-left; the editor's UV coordinates are top-left.
    image=image.convertToFormat(QImage::Format_RGBA8888).mirrored(false,true);
    auto gpu=std::make_unique<QOpenGLTexture>(image,QOpenGLTexture::DontGenerateMipMaps);
    if(!gpu->isCreated())return nullptr;
    gpu->setWrapMode(QOpenGLTexture::Repeat);
    gpu->setMinificationFilter(QOpenGLTexture::Nearest);
    gpu->setMagnificationFilter(QOpenGLTexture::Nearest);
    CachedTexture& cache=textureCache_[key];
    cache.signature=signature;
    cache.gpu=std::move(gpu);
    return cache.gpu.get();
}
void Viewport::paintGL(){
    if(!glReady_) {glClearColor(0.075f,0.11f,0.14f,1);glClear(GL_COLOR_BUFFER_BIT);return;}
    QPainter painter(this);
    painter.beginNativePainting();
    glViewport(0,0,int(width()*devicePixelRatioF()),int(height()*devicePixelRatioF()));
    glClearColor(0.055f,0.089f,0.118f,1);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    updateCameraMatrices();
    uploadBuffers();
    shader_.bind();shader_.setUniformValue("uMvp",mvp_);
    shader_.setUniformValue("uTexture",0);
    vao_.bind();
    if(!mesh_.triangles.empty()) {
        solid_.bind();
        glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);glEnableVertexAttribArray(3);
        glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,x)));
        glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,r)));
        glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,nx)));
        glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<const void*>(offsetof(Vertex,u)));
        for(const auto& draw:mesh_.draws){
            auto* tex=draw.useTexture?getTexture(draw.texture,draw.material):nullptr;
            shader_.setUniformValue("uTextured",int(tex!=nullptr));
            if(tex)tex->bind(0);
            glDrawArrays(GL_TRIANGLES,GLint(draw.first),GLsizei(draw.count));
        }
        shader_.setUniformValue("uTextured",0);
        solid_.release();
    }
    drawBuffer(lines_,mesh_.lines,GL_LINES);
    vao_.release();shader_.release();
    glDisable(GL_DEPTH_TEST); // 2D QPainter overlays must never fail the depth test.
    painter.endNativePainting();
    if(options_.showFormed)drawModelOverlay(painter);
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
QPointF Viewport::gizmoCenterOnScreen() const {
    if(options_.selectedPart<0||options_.selectedPart>=int(document_.parts.size()))return {};
    return projectPoint(document_.parts[std::size_t(options_.selectedPart)].pivot);
}
double Viewport::gizmoLength() const {
    if(options_.selectedPart<0||options_.selectedPart>=int(document_.parts.size()))return 1;
    const auto center=document_.parts[std::size_t(options_.selectedPart)].pivot;
    const QPointF start=projectPoint(center);
    double scale=0;int count=0;
    for(const Vec3 axis:{Vec3{1,0,0},Vec3{0,1,0},Vec3{0,0,1}}){
        const QPointF delta=projectPoint(center+axis)-start;
        const double pixels=std::hypot(delta.x(),delta.y());
        if(pixels>2){scale+=pixels;count++;}
    }
    const double pixels=count?scale/count:70;
    return std::clamp(88.0/std::max(pixels,0.1),0.12,32.0);
}
static QPointF screenVec(const QPointF& from,const QPointF& to){return to-from;}
static double pointSegmentDistance(QPointF point,QPointF a,QPointF b){
    const QPointF span=screenVec(a,b),relative=screenVec(a,point);
    const double sq=span.x()*span.x()+span.y()*span.y();
    const double t=sq>1e-10?std::clamp((relative.x()*span.x()+relative.y()*span.y())/sq,0.0,1.0):0.0;
    const QPointF nearest=a+span*t;
    return std::hypot(point.x()-nearest.x(),point.y()-nearest.y());
}
Vec3 Viewport::handlePlane(char handle) const {
    if(handle=='1')return {0,0,1}; // XY
    if(handle=='2')return {0,1,0}; // XZ
    if(handle=='3')return {1,0,0}; // YZ
    const auto inverted=view_.inverted();
    const auto vector=inverted.mapVector(QVector3D(0,0,-1)).normalized();
    return {vector.x(),vector.y(),vector.z()}; // camera-facing free movement plane
}
bool Viewport::screenOnPlane(const QPoint& screen,Vec3 center,Vec3 normal,Vec3& result) const {
    bool invertible=false;
    const auto inverse=mvp_.inverted(&invertible);
    if(!invertible)return false;
    const float nx=2.0f*float(screen.x())/float(std::max(1,width()))-1.0f;
    const float ny=1.0f-2.0f*float(screen.y())/float(std::max(1,height()));
    const QVector4D near=inverse*QVector4D(nx,ny,-1.0f,1),far=inverse*QVector4D(nx,ny,1.0f,1);
    if(std::abs(near.w())<1e-7||std::abs(far.w())<1e-7)return false;
    const QVector3D origin=near.toVector3DAffine();
    const QVector3D ray=far.toVector3DAffine()-origin;
    const QVector3D n(float(normal.x),float(normal.y),float(normal.z));
    const float dot=QVector3D::dotProduct(n,ray);
    if(std::abs(dot)<1e-7)return false;
    const float t=QVector3D::dotProduct(n,QVector3D(float(center.x),float(center.y),float(center.z))-origin)/dot;
    if(t<0||t>1)return false;
    const QVector3D hit=origin+ray*t;
    result={hit.x(),hit.y(),hit.z()};
    return std::isfinite(result.x)&&std::isfinite(result.y)&&std::isfinite(result.z);
}
char Viewport::gizmoAt(const QPoint& screen) const {
    if(options_.selectedPart<0||options_.selectedPart>=int(document_.parts.size()))return 0;
    const auto pivot=document_.parts[std::size_t(options_.selectedPart)].pivot;
    const QPointF origin=projectPoint(pivot),mouse(screen);
    const double len=gizmoLength();
    if(activeTool_=='w'||activeTool_=='r'){
        if(std::hypot(mouse.x()-origin.x(),mouse.y()-origin.y())<10)
            return activeTool_=='r'?'u':'f';
        if(activeTool_=='w'){
            struct Plane {char id;Vec3 a,b;};
            constexpr Plane planes[]={{'1',{1,0,0},{0,1,0}},
                {'2',{1,0,0},{0,0,1}},{'3',{0,1,0},{0,0,1}}};
            for(const auto& plane:planes){
                QPolygonF poly;
                for(const auto [a,b]:{std::pair{0.20,0.20},std::pair{0.40,0.20},
                        std::pair{0.40,0.40},std::pair{0.20,0.40}})
                    poly<<projectPoint(pivot+(plane.a*a+plane.b*b)*len);
                if(poly.containsPoint(mouse,Qt::OddEvenFill))return plane.id;
            }
        }
        double closest=12;char best=0;
        for(const auto& axis:std::array<std::pair<char,Vec3>,3>{{{'x',{1,0,0}},
                  {'y',{0,1,0}},{'z',{0,0,1}}}}){
            const QPointF begin=projectPoint(pivot+axis.second*(len*0.44));
            const QPointF end=projectPoint(pivot+axis.second*(len*1.18));
            const double d=pointSegmentDistance(mouse,begin,end);
            if(d<closest){closest=d;best=axis.first;}
        }
        return best;
    }
    if(activeTool_=='e'){
        double closest=10.0;char best=0;
        constexpr int segments=96;
        for(const char axis:{'x','y','z'}){
            auto curve=[&](double radians){
                const double c=std::cos(radians),s=std::sin(radians);
                return projectPoint(pivot+(axis=='x'?Vec3{0,c,s}:axis=='y'?Vec3{c,0,s}:Vec3{c,s,0})*len*0.8);
            };
            for(int i=0;i<segments;i++){
                const double a=6.28318530718*double(i)/segments;
                const double b=6.28318530718*double(i+1)/segments;
                const double d=pointSegmentDistance(mouse,curve(a),curve(b));
                if(d<closest){closest=d;best=axis;}
            }
        }
        return best;
    }
    return 0;
}
void Viewport::beginHandleDrag(char handle,const QPoint& mouse){
    dragHandle_=handle;dragMouseStart_=mouse;
    dragAccum_=emittedDrag_=0;
    dragSent_={};
    dragHasAnchor_=false;
    dragPivot_=document_.parts[std::size_t(options_.selectedPart)].pivot;
    dragPlaneNormal_=handlePlane(handle);
    if(handle=='f'||handle=='1'||handle=='2'||handle=='3')
        dragHasAnchor_=screenOnPlane(mouse,dragPivot_,dragPlaneNormal_,dragAnchor_);
    transforming_=true;
    emit beginEdit();
    update();
}
void Viewport::drawModelOverlay(QPainter& painter){
    painter.setRenderHint(QPainter::Antialiasing,true);
    if(drawing_&&hasDrawPreview_){
        const Bounds b=drawnCuboid(drawStart_,drawEnd_,workPlane_,drawHeight_,gridStep_);
        const auto a=b.from,c=b.to;
        const Vec3 vertices[]={{a.x,a.y,a.z},{c.x,a.y,a.z},{c.x,c.y,a.z},{a.x,c.y,a.z},
            {a.x,a.y,c.z},{c.x,a.y,c.z},{c.x,c.y,c.z},{a.x,c.y,c.z}};
        const int edges[][2]={{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}};
        painter.setPen(QPen(QColor(253,183,83),2,Qt::DashLine));
        for(const auto& edge:edges)painter.drawLine(projectPoint(vertices[edge[0]]),projectPoint(vertices[edge[1]]));
    }
    if(options_.selectedPart<0||options_.selectedPart>=int(document_.parts.size())||activeTool_=='v'||activeTool_=='b')return;
    const auto pivot=document_.parts[std::size_t(options_.selectedPart)].pivot;
    const double length=gizmoLength();
    const QPointF center=projectPoint(pivot);
    struct Axis {char id;Vec3 vec;QColor color;};
    const std::array<Axis,3> axes{{{'x',{1,0,0},QColor("#ff5b5b")},
        {'y',{0,1,0},QColor("#5eee72")},{'z',{0,0,1},QColor("#578dff")}}};
    if(activeTool_=='w'){
        // Plane handles are actual draggable constraints, not decorative squares.
        struct Plane{char id;Vec3 a,b;QColor color;};
        const std::array<Plane,3> planes{{{'1',{1,0,0},{0,1,0},QColor(255,209,91,100)},
            {'2',{1,0,0},{0,0,1},QColor(106,206,251,100)},
            {'3',{0,1,0},{0,0,1},QColor(177,124,250,100)}}};
        for(const auto& plane:planes){
            QPolygonF polygon;
            for(const auto [a,b]:{std::pair{0.20,0.20},std::pair{0.40,0.20},
                    std::pair{0.40,0.40},std::pair{0.20,0.40}})
                polygon<<projectPoint(pivot+(plane.a*a+plane.b*b)*length);
            painter.setPen(QPen(plane.color.lighter(130),1));
            painter.setBrush(plane.color);painter.drawPolygon(polygon);
        }
    }
    if(activeTool_=='e'){
        constexpr int segments=96;
        for(const auto& axis:axes){
            QPainterPath path;
            for(int i=0;i<=segments;i++){
                const double a=6.28318530718*double(i)/segments,c=std::cos(a),s=std::sin(a);
                const Vec3 local=axis.id=='x'?Vec3{0,c,s}:axis.id=='y'?Vec3{c,0,s}:Vec3{c,s,0};
                const QPointF point=projectPoint(pivot+local*length*0.8);
                if(i==0)path.moveTo(point);else path.lineTo(point);
            }
            painter.setPen(QPen(axis.color,axis.id==dragHandle_?4:2.5));
            painter.setBrush(Qt::NoBrush);painter.drawPath(path);
        }
        return;
    }
    for(const auto& axis:axes){
        const QPointF end=projectPoint(pivot+axis.vec*length);
        painter.setPen(QPen(axis.color,axis.id==dragHandle_?4:2.5));
        painter.drawLine(center,end);
        const QPointF delta=end-center;
        const double pixels=std::hypot(delta.x(),delta.y());
        if(pixels<3)continue;
        painter.setBrush(axis.color);
        if(activeTool_=='w'){
            const QPointF unit=delta/pixels;
            QPolygonF arrow;
            arrow<<end+unit*7.0<<end-unit*8.0+QPointF(-unit.y()*5,unit.x()*5)
                <<end-unit*8.0-QPointF(-unit.y()*5,unit.x()*5);
            painter.drawPolygon(arrow);
        }else painter.drawRect(QRectF(end-QPointF(5,5),QSizeF(10,10)));
    }
    painter.setPen(QPen(QColor("#273a46"),1.5));
    painter.setBrush(QColor("#f9f2d6"));
    if(activeTool_=='w')painter.drawEllipse(center,8,8);
    else painter.drawRect(QRectF(center-QPointF(7,7),QSizeF(14,14)));
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
                if(axis=='x'||axis=='y'||axis=='z'){
                    activeAxis_=axis;emit axisPicked(axis);
                }
                beginHandleDrag(axis,event->pos());return;
            }
        }
        const int hit=pickPart(event->pos());
        if(hit>=0){
            if(hit!=options_.selectedPart){options_.selectedPart=hit;emit partSelected(hit);rebuildAsync();}
            // Clicking a body selects it; dragging its visible handles edits it.
            // This prevents accidentally moving a large machine while orbiting.
            selecting_=true;
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
    if(dragHandle_=='f'||dragHandle_=='1'||dragHandle_=='2'||dragHandle_=='3'){
        if(activeTool_!='w'||!dragHasAnchor_)return;
        Vec3 current;
        if(!screenOnPlane(event->pos(),dragPivot_,dragPlaneNormal_,current))return;
        Vec3 delta=current-dragAnchor_;
        if(dragHandle_=='1')delta.z=0;
        if(dragHandle_=='2')delta.y=0;
        if(dragHandle_=='3')delta.x=0;
        const Vec3 snapped{snapGrid(delta.x,gridStep_),snapGrid(delta.y,gridStep_),snapGrid(delta.z,gridStep_)};
        for(const auto [axis,value,prev]:std::array<std::tuple<char,double,double>,3>{{
            {'x',snapped.x,dragSent_.x},{'y',snapped.y,dragSent_.y},{'z',snapped.z,dragSent_.z}}}){
            if(std::abs(value-prev)>1e-8)emit deltaEdit(value-prev,axis,'w');
        }
        dragSent_=snapped;
        return;
    }
    if(dragHandle_=='u'&&activeTool_=='r'){
        const double absolute=-(event->pos().y()-dragMouseStart_.y())*gridStep_*0.4;
        const double target=snapGrid(absolute,gridStep_);
        const double difference=target-emittedDrag_;
        if(std::abs(difference)>1e-8){
            emittedDrag_=target;
            for(const char axis:{'x','y','z'})emit deltaEdit(difference,axis,'r');
        }
        return;
    }
    const char axis=dragHandle_;
    const Vec3 unit{axis=='x'?1.0:0.0,axis=='y'?1.0:0.0,axis=='z'?1.0:0.0};
    const QPointF origin=projectPoint(dragPivot_);
    double total=0;
    if(activeTool_=='e'){
        const QPointF start=QPointF(dragMouseStart_)-origin;
        const QPointF now=QPointF(event->pos())-origin;
        if(std::hypot(now.x(),now.y())<6||std::hypot(start.x(),start.y())<6)return;
        const double before=std::atan2(start.y(),start.x());
        const double after=std::atan2(now.y(),now.x());
        double angle=(after-before)*180.0/3.14159265358979323846;
        if(angle>180)angle-=360;if(angle< -180)angle+=360;
        total=angle;
    }else {
        const QPointF offset=projectPoint(dragPivot_+unit)-origin;
        const QPointF mouse=QPointF(event->pos()-dragMouseStart_);
        const double length2=offset.x()*offset.x()+offset.y()*offset.y();
        if(length2>1e-6)total=(mouse.x()*offset.x()+mouse.y()*offset.y())/length2;
    }
    const double snap=activeTool_=='e'?5.0:gridStep_;
    const double target=snapGrid(total,snap);
    const double difference=target-emittedDrag_;
    if(std::abs(difference)>1e-8){emittedDrag_=target;emit deltaEdit(difference,axis,activeTool_);}

}
void Viewport::mouseReleaseEvent(QMouseEvent* event){
    if(drawing_ && hasDrawPreview_ && event->button()==Qt::LeftButton){
        emit boxDrawn(drawStart_.x,drawStart_.z,drawEnd_.x,drawEnd_.z);
    }
    if(transforming_)emit endEdit();
    transforming_=orbiting_=panning_=drawing_=selecting_=hasDrawPreview_=false;
    dragHandle_=0;dragHasAnchor_=false;
    update();
}

} // namespace studio
