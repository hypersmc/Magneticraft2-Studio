#pragma once

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QWidget>
#include <array>

namespace studio {

// A selectable, unfolded six-face cuboid overview. Each face owns its own
// texture and UV rectangle: this is a visualization, not a destructive box-UV
// conversion. The atlas editor below edits only the selected face.
class UvFaceOverview final : public QWidget {
    Q_OBJECT
public:
    struct Face {
        QImage image;
        QString texture;
        std::array<double,4> uv{0.0,0.0,1.0,1.0};
        bool automatic=true;
        int quarterTurns=0;
    };

    explicit UvFaceOverview(QWidget* parent=nullptr);
    void setFaces(const std::array<Face,6>& faces,int selectedFace);
    void clear();
    int selectedFace() const {return selected_;}
    QPointF faceCenter(int index) const; // Also useful for real pointer UI tests.
signals:
    void facePicked(int face);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
private:
    QRectF tile(int index) const;
    int faceAt(QPointF screenPoint) const;
    static QImage preview(const Face& face);
    std::array<Face,6> faces_{};
    std::array<QImage,6> preview_{};
    int selected_=-1;
};

} // namespace studio
