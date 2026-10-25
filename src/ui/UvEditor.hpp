#pragma once
#include <QImage>
#include <QPoint>
#include <QRectF>
#include <QWidget>
#include <array>

namespace studio {
// A pixel-accurate tiled UV canvas. All coordinates are in texture tiles
// (1.0 means the full source PNG, 2.0 repeats it twice).
class UvEditor final : public QWidget {
    Q_OBJECT
public:
    explicit UvEditor(QWidget* parent=nullptr);
    void setImage(const QImage& image);
    void setRect(const std::array<double,4>& uv);
    std::array<double,4> uv() const {return uv_;}
signals:
    void editBegan();
    void rectChanged(double u0,double v0,double u1,double v1);
    void editEnded();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    QRectF canvas() const;
    QPointF toScreen(double u,double v) const;
    QPointF toUv(const QPointF& p) const;
    void updateExtent();
    QImage image_;
    std::array<double,4> uv_{0,0,1,1};
    std::array<double,4> initial_{};
    QPointF downUV_{};
    int handle_=-1; // four corners, 4 = move, 5 = draw new UV rectangle
    int tilesU_=1,tilesV_=1;
};
}
