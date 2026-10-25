#pragma once
#include "core/Project.hpp"
#include <QWidget>

namespace studio {

class LayerCanvas : public QWidget {
    Q_OBJECT
public:
    explicit LayerCanvas(QWidget* parent=nullptr);
    void setProject(const Project* project);
    void setLayer(int layer);
    void setRole(QString role);
    QSize sizeHint() const override { return {640,240}; }
signals:
    void paintRequested(int x,int y,int z,QString role);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
private:
    void paintAt(const QPoint& at);
    QRectF gridRect() const;
    const Project* project_=nullptr;
    int layer_=0;
    QString role_="Frame";
    Cell lastPaint_{-1,-1,-1};
};

} // namespace studio
