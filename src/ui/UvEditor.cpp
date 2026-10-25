#include "UvEditor.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QLineF>
#include <algorithm>
#include <cmath>

namespace studio {
UvEditor::UvEditor(QWidget* parent):QWidget(parent){
    setMinimumHeight(220);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);
    setMouseTracking(true);
}
void UvEditor::setImage(const QImage& image){
    image_=image;update();
}
void UvEditor::updateExtent(){
    const auto extent=[&](double a,double b){return std::clamp(int(std::ceil(std::max({1.0,a,b}))),1,8);};
    tilesU_=extent(uv_[0],uv_[2]);tilesV_=extent(uv_[1],uv_[3]);
}
void UvEditor::setFaceOverlays(const std::array<OverlayFace,6>& faces,int activeFace){
    overlays_=faces;
    activeFace_=activeFace;
    update();
}
void UvEditor::setRect(const std::array<double,4>& uv){
    uv_=uv;updateExtent();update();
}
QRectF UvEditor::canvas() const {
    const double available=std::max(30.0,std::min(double(width()-28),double(height()-28)));
    return {(width()-available)*0.5,(height()-available)*0.5,available,available};
}
QPointF UvEditor::toScreen(double u,double v) const {
    const QRectF c=canvas();
    return {c.left()+u/double(tilesU_)*c.width(),c.top()+v/double(tilesV_)*c.height()};
}
QPointF UvEditor::toUv(const QPointF& point) const {
    const QRectF c=canvas();
    return {(point.x()-c.left())/c.width()*tilesU_,(point.y()-c.top())/c.height()*tilesV_};
}
void UvEditor::paintEvent(QPaintEvent*){
    QPainter painter(this);
    painter.fillRect(rect(),QColor("#17232c"));
    painter.setRenderHint(QPainter::SmoothPixmapTransform,false);
    const QRectF canvasRect=canvas();
    painter.fillRect(canvasRect,QColor("#20313b"));
    for(int x=0;x<tilesU_;x++)for(int y=0;y<tilesV_;y++){
        const QRectF cell(toScreen(x,y),toScreen(x+1,y+1));
        if(!image_.isNull())painter.drawImage(cell,image_);
        else {
            painter.fillRect(cell,QColor((x+y)%2?"#59666c":"#8c7c6c"));
            const int divisions=8;
            for(int a=0;a<divisions;a++)for(int b=0;b<divisions;b++)if((a+b)%2==0){
                painter.fillRect(QRectF(cell.left()+cell.width()*a/divisions,cell.top()+cell.height()*b/divisions,
                    cell.width()/divisions,cell.height()/divisions),QColor(255,255,255,18));
            }
        }
    }
    painter.setPen(QPen(QColor("#a7b4bf"),1));
    for(int x=0;x<=tilesU_;x++)painter.drawLine(toScreen(x,0),toScreen(x,tilesV_));
    for(int y=0;y<=tilesV_;y++)painter.drawLine(toScreen(0,y),toScreen(tilesU_,y));
    // Other faces referencing this same texture are visible on the atlas,
    // rather than hiding five of the six cuboid UV rectangles in a dropdown.
    static const QColor accent[6]={
        QColor("#f4a66a"),QColor("#59c6d5"),QColor("#b99ef5"),
        QColor("#88c4fc"),QColor("#a5dc81"),QColor("#f5d07e")
    };
    static const char* initial[6]={"S","N","W","E","U","D"};
    for(int i=0;i<6;i++){
        if(i==activeFace_||!overlays_[std::size_t(i)].visible)continue;
        const auto& uv=overlays_[std::size_t(i)].uv;
        const QRectF other=QRectF(toScreen(uv[0],uv[1]),toScreen(uv[2],uv[3])).normalized();
        painter.setPen(QPen(accent[i],1.5,Qt::DashLine));
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(other);
        painter.drawText(other.topLeft()+QPointF(4,14),initial[i]);
    }
    const QRectF selection(toScreen(uv_[0],uv_[1]),toScreen(uv_[2],uv_[3]));
    painter.fillRect(selection.normalized(),QColor(243,165,70,48));
    painter.setPen(QPen(QColor("#ffbf6e"),2));
    painter.setBrush(Qt::NoBrush);painter.drawRect(selection.normalized());
    painter.setPen(QPen(QColor("#1b2a33"),1));painter.setBrush(QColor("#ffe0a0"));
    for(const QPointF corner:{selection.topLeft(),selection.topRight(),selection.bottomRight(),selection.bottomLeft()})
        painter.drawRect(QRectF(corner-QPointF(4,4),QSizeF(8,8)));
    painter.setPen(QColor("#fff1d5"));
    painter.drawText(canvasRect.adjusted(6,6,-6,-6),Qt::AlignTop|Qt::AlignLeft,
        QString("%1 x %2 tiles").arg(tilesU_).arg(tilesV_));
}
void UvEditor::mousePressEvent(QMouseEvent* event){
    if(event->button()!=Qt::LeftButton || !canvas().contains(event->pos()))return;
    const QRectF selection(toScreen(uv_[0],uv_[1]),toScreen(uv_[2],uv_[3]));
    const std::array<QPointF,4> corners{selection.topLeft(),selection.topRight(),selection.bottomRight(),selection.bottomLeft()};
    handle_=-1;
    for(int i=0;i<4;i++){
        if(QLineF(event->position(),corners[std::size_t(i)]).length()<12){handle_=i;break;}
    }
    if(handle_<0)handle_=selection.normalized().contains(event->position())?4:5;
    initial_=uv_;
    downUV_=toUv(event->position());
    emit editBegan();
    event->accept();
}
void UvEditor::mouseMoveEvent(QMouseEvent* event){
    if(handle_<0)return;
    const QPointF now=toUv(event->position());
    const double gridU=1.0/std::max(16,image_.width());
    const double gridV=1.0/std::max(16,image_.height());
    const auto snap=[](double a,double grid){return std::round(a/grid)*grid;};
    const double dx=snap(now.x()-downUV_.x(),gridU);
    const double dy=snap(now.y()-downUV_.y(),gridV);
    auto output=initial_;
    if(handle_==4){output[0]+=dx;output[2]+=dx;output[1]+=dy;output[3]+=dy;}
    else if(handle_==5){
        output={snap(downUV_.x(),gridU),snap(downUV_.y(),gridV),snap(now.x(),gridU),snap(now.y(),gridV)};
    }else{
        if(handle_==0||handle_==3)output[0]+=dx;else output[2]+=dx;
        if(handle_==0||handle_==1)output[1]+=dy;else output[3]+=dy;
    }
    for(double& value:output)value=std::clamp(value,-8.0,32.0);
    if(output!=uv_){uv_=output;updateExtent();update();emit rectChanged(uv_[0],uv_[1],uv_[2],uv_[3]);}
    event->accept();
}
void UvEditor::mouseReleaseEvent(QMouseEvent* event){
    if(handle_<0||event->button()!=Qt::LeftButton)return;
    handle_=-1;emit editEnded();event->accept();
}
} // namespace studio
