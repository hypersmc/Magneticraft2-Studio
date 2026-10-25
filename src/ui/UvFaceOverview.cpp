#include "UvFaceOverview.hpp"

#include <QKeyEvent>
#include <QFont>
#include <QStringList>
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <QtGlobal>
#include <algorithm>
#include <cmath>

namespace studio {
namespace {
struct Location { int column, row; };
constexpr std::array<Location,6> positions{{
    {3,1}, // South (+Z)
    {1,1}, // North (-Z)
    {0,1}, // West  (-X)
    {2,1}, // East  (+X)
    {1,0}, // Up    (+Y)
    {1,2}  // Down  (-Y)
}};
const QStringList titles={"SOUTH +Z","NORTH -Z","WEST -X","EAST +X","UP +Y","DOWN -Y"};
constexpr int thumbSize=80;
constexpr int inset=8;
constexpr int spacing=9;

// Nearest-neighbour sampling, including Minecraft's tiled UV coordinates.
// No data is modified; the face's texture and UVs remain independent.
QColor missingSample(int x,int y){
    return ((x/10+y/10)%2==0)?QColor("#f000dc"):QColor("#232029");
}
}

UvFaceOverview::UvFaceOverview(QWidget* parent):QWidget(parent){
    setMinimumSize(300,265);
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName("Unfolded six-face UV overview; select a face to edit its texture mapping");
}

QImage UvFaceOverview::preview(const Face& face){
    QImage result(thumbSize,thumbSize,QImage::Format_ARGB32_Premultiplied);
    const QImage image=face.image.convertToFormat(QImage::Format_ARGB32);
    for(int y=0;y<thumbSize;y++)for(int x=0;x<thumbSize;x++){
        if(image.isNull()){
            result.setPixelColor(x,y,missingSample(x,y));continue;
        }
        double s=(x+0.5)/double(thumbSize),t=(y+0.5)/double(thumbSize);
        switch((face.quarterTurns%4+4)%4){
            case 1:{const double old=s;s=t;t=1.0-old;break;}
            case 2:s=1.0-s;t=1.0-t;break;
            case 3:{const double old=s;s=1.0-t;t=old;break;}
            default:break;
        }
        const auto& r=face.uv;
        const double u=r[0]+(r[2]-r[0])*s,v=r[1]+(r[3]-r[1])*t;
        const double ru=u-std::floor(u),rv=v-std::floor(v);
        const int px=std::clamp(int(ru*image.width()),0,image.width()-1);
        const int py=std::clamp(int(rv*image.height()),0,image.height()-1);
        result.setPixelColor(x,y,image.pixelColor(px,py));
    }
    return result;
}

void UvFaceOverview::setFaces(const std::array<Face,6>& faces,int selectedFace){
    faces_=faces;
    selected_=(selectedFace>=0&&selectedFace<6)?selectedFace:-1;
    for(int i=0;i<6;i++)preview_[std::size_t(i)]=preview(faces_[std::size_t(i)]);
    update();
}
void UvFaceOverview::clear(){
    selected_=-1;
    for(auto& image:preview_)image=QImage();
    update();
}
QRectF UvFaceOverview::tile(int index) const{
    if(index<0||index>=6)return {};
    const double widthAvailable=std::max(40,width()-inset*2-3*spacing);
    const double heightAvailable=std::max(40,height()-inset*2-2*spacing-20);
    const double side=std::max(25.0,std::min(widthAvailable/4.0,heightAvailable/3.0));
    const double x0=(width()-(4*side+3*spacing))*0.5;
    const double y0=(height()-(3*side+2*spacing))*0.5+7;
    const Location location=positions[std::size_t(index)];
    return {x0+location.column*(side+spacing),y0+location.row*(side+spacing),side,side};
}
QPointF UvFaceOverview::faceCenter(int index) const{return tile(index).center();}
int UvFaceOverview::faceAt(QPointF point) const{
    if(selected_<0)return -1;
    for(int i=0;i<6;i++)if(tile(i).contains(point))return i;
    return -1;
}
void UvFaceOverview::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing,true);
    p.fillRect(rect(),QColor("#121f29"));
    p.setPen(QColor("#96adbd"));
    p.drawText(QRectF(9,4,width()-18,21),Qt::AlignLeft|Qt::AlignVCenter,
               "ALL SIX FACES  /  CLICK A FACE TO EDIT");
    if(selected_<0){
        p.setPen(QColor("#879eab"));
        p.drawText(rect(),Qt::AlignCenter,"Select a model cuboid to inspect its six UV faces");
        return;
    }
    for(int i=0;i<6;i++){
        const auto& f=faces_[std::size_t(i)];
        const QRectF cell=tile(i);
        const bool active=i==selected_;
        p.setPen(Qt::NoPen);p.setBrush(QColor(active?"#35414a":"#24323b"));
        p.drawRoundedRect(cell,5,5);
        const qreal titleHeight=std::min(26.0,cell.height()*0.29);
        const QRectF picture=cell.adjusted(5,titleHeight+1,-5,-17);
        if(picture.width()>5&&picture.height()>5){
            // Checkerboard behind PNG transparency.
            p.fillRect(picture,QColor("#5e6571"));
            p.setRenderHint(QPainter::SmoothPixmapTransform,false);
            p.drawImage(picture,preview_[std::size_t(i)]);
        }
        p.setPen(active?QColor("#ffc891"):QColor("#c4d1dc"));
        QFont label=p.font();label.setBold(active);label.setPointSizeF(std::max(7.0,std::min(10.0,cell.width()/13.0)));
        p.setFont(label);
        p.drawText(cell.adjusted(3,2,-3,-cell.height()+titleHeight+1),Qt::AlignCenter,titles[i]);
        p.setPen(QColor("#9db5c4"));
        QFont foot=p.font();foot.setPointSizeF(7.0);foot.setBold(false);p.setFont(foot);
        const QString footText=f.automatic?"AUTO UV":"FACE UV";
        p.drawText(QRectF(cell.left()+2,cell.bottom()-16,cell.width()-4,15),Qt::AlignCenter,footText);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(QColor(active?"#e5a46b":"#506473"),active?2.3:1.0));
        p.drawRoundedRect(cell.adjusted(1,1,-1,-1),5,5);
    }
}
void UvFaceOverview::mousePressEvent(QMouseEvent* event){
    if(event->button()!=Qt::LeftButton){QWidget::mousePressEvent(event);return;}
    const int face=faceAt(event->position());
    if(face<0){QWidget::mousePressEvent(event);return;}
    setFocus(Qt::MouseFocusReason);
    selected_=face;update();emit facePicked(face);
    event->accept();
}
void UvFaceOverview::mouseMoveEvent(QMouseEvent* event){
    const int face=faceAt(event->position());
    setCursor(face>=0?Qt::PointingHandCursor:Qt::ArrowCursor);
    if(face>=0){
        const Face& f=faces_[std::size_t(face)];
        setToolTip(QString("%1\n%2\nUV: %3, %4 to %5, %6  %7")
            .arg(titles[face],f.texture.isEmpty()?"No texture assigned":f.texture)
            .arg(f.uv[0],0,'f',3).arg(f.uv[1],0,'f',3)
            .arg(f.uv[2],0,'f',3).arg(f.uv[3],0,'f',3)
            .arg(f.automatic?"Automatic":"Manual"));
    }else setToolTip({});
    QWidget::mouseMoveEvent(event);
}
void UvFaceOverview::keyPressEvent(QKeyEvent* event){
    if(selected_>=0 && (event->key()==Qt::Key_Left||event->key()==Qt::Key_Right)){
        const int d=event->key()==Qt::Key_Right?1:-1;
        selected_=(selected_+d+6)%6;update();emit facePicked(selected_);event->accept();return;
    }
    QWidget::keyPressEvent(event);
}

} // namespace studio
