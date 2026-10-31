#include "LayerCanvas.hpp"
#include <QMouseEvent>
#include <QPainter>
#include <QPen>
#include <algorithm>
#include <cmath>

namespace studio {

static QColor roleColor(const std::string& role) {
    if(role=="Controller")return QColor("#e3a94b");
    if(role.find("Item")!=std::string::npos)return QColor("#6bb986");
    if(role.find("Fluid")!=std::string::npos)return QColor("#58a4c8");
    if(role.find("Mechanical")!=std::string::npos)return QColor("#d57b5b");
    return QColor("#8d6e50");
}
LayerCanvas::LayerCanvas(QWidget* parent):QWidget(parent) {
    setMinimumHeight(160);
    setMouseTracking(true);
    setCursor(Qt::CrossCursor);
}
void LayerCanvas::setProject(const Project* p) {project_=p;update();}
void LayerCanvas::setLayer(int layer){layer_=layer;lastPaint_={-1,-1,-1};update();}
void LayerCanvas::setRole(QString role){role_=std::move(role);lastPaint_={-1,-1,-1};update();}
QRectF LayerCanvas::gridRect() const {
    if(!project_)return {};
    const double side=std::max(1.0,std::min((width()-44.0)/project_->dimensions[0],
                                                 (height()-32.0)/project_->dimensions[2]));
    const double w=side*project_->dimensions[0],h=side*project_->dimensions[2];
    return {(width()-w)*0.5,(height()-h)*0.5,w,h};
}
void LayerCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(),QColor("#111b23"));
    if(!project_)return;
    p.setRenderHint(QPainter::Antialiasing,false);
    const auto area=gridRect();
    const double side=area.width()/project_->dimensions[0];
    p.setPen(QPen(QColor("#34434e"),1));
    p.setBrush(QColor("#1a2934"));
    p.drawRect(area);
    for(int z=0;z<project_->dimensions[2];z++) for(int x=0;x<project_->dimensions[0];x++) {
        const QRectF cell(area.x()+x*side,area.y()+z*side,side,side);
        auto it=project_->cells.find({x,layer_,z});
        if(it!=project_->cells.end()) {
            QColor color=roleColor(it->second);
            p.fillRect(cell.adjusted(1,1,-1,-1),color);
        }
    }
    if(side>=4) {
        p.setPen(QColor("#41515a"));
        for(int x=0;x<=project_->dimensions[0];x++) {
            const double px=area.x()+x*side;
            p.drawLine(QPointF(px,area.y()),QPointF(px,area.bottom()));
        }
        for(int z=0;z<=project_->dimensions[2];z++) {
            const double py=area.y()+z*side;
            p.drawLine(QPointF(area.x(),py),QPointF(area.right(),py));
        }
    }
    p.setPen(QColor("#9eb0bf"));
    p.drawText(QRectF(8,4,width()-16,20),Qt::AlignLeft,"LAYER Y="+QString::number(layer_)+"   |   X EAST  /  Z SOUTH");
}
void LayerCanvas::paintAt(const QPoint& pt){
    if(!project_)return;
    const auto bounds=gridRect();
    if(!bounds.contains(pt))return;
    const double side=bounds.width()/project_->dimensions[0];
    const int x=int(std::floor((pt.x()-bounds.x())/side));
    const int z=int(std::floor((pt.y()-bounds.y())/side));
    Cell cell{x,layer_,z};
    if(!contains(project_->dimensions,cell) || cell==lastPaint_)return;
    lastPaint_=cell;
    emit paintRequested(x,layer_,z,role_);
}
void LayerCanvas::mousePressEvent(QMouseEvent* e) {
    if(e->button()==Qt::LeftButton){lastPaint_={-1,-1,-1};paintAt(e->pos());}
}
void LayerCanvas::mouseMoveEvent(QMouseEvent* e) {
    if(e->buttons() & Qt::LeftButton)paintAt(e->pos());
    else lastPaint_={-1,-1,-1};
}
} // namespace studio
