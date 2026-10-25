#pragma once
#include "core/Geometry.hpp"

#include <QMatrix4x4>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QPoint>
#include <QThreadPool>
#include <QTimer>

#include <atomic>
#include <memory>
#include <mutex>

namespace studio {

class Viewport final : public QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    Q_OBJECT
public:
    explicit Viewport(QWidget* parent=nullptr);
    ~Viewport() override;
    void setScene(const Project& p,BuildOptions options);
    void resetCamera();
    void setSelection(int index);
    void setAxis(char axis);
    void setTool(char tool); // W = translate, E = rotate, R = scale
signals:
    void partSelected(int index);
    void beginEdit();
    void deltaEdit(double amount,char axis,char tool);
    void endEdit();
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w,int h) override;
    void wheelEvent(QWheelEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
private:
    struct MeshMailbox {
        std::mutex mutex;
        std::atomic<std::uint64_t> generation{0};
        std::shared_ptr<Geometry> ready;
        std::uint64_t readyGeneration=0;
    };
    void rebuildAsync();
    void pollResults();
    void updateCameraMatrices();
    int pickPart(const QPoint& at) const;
    QPointF projectPoint(Vec3 pos) const;
    void uploadBuffers();
    void drawBuffer(QOpenGLBuffer& buffer, const std::vector<Vertex>& verts,GLenum primitive);

    Project document_;
    BuildOptions options_;
    std::shared_ptr<MeshMailbox> mailbox_;
    QThreadPool pool_;
    QTimer poll_;
    Geometry mesh_;
    bool dirtyGpu_=true;
    bool glReady_=false;
    QOpenGLShaderProgram shader_;
    QOpenGLVertexArrayObject vao_;
    QOpenGLBuffer solid_{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer lines_{QOpenGLBuffer::VertexBuffer};
    QMatrix4x4 mvp_;
    QMatrix4x4 view_;
    QMatrix4x4 projection_;
    QPoint lastMouse_;
    bool orbiting_=false;
    bool panning_=false;
    bool transforming_=false;
    double dragAccum_=0;
    double emittedDrag_=0;
    double distance_=12;
    double yaw_=0.8, pitch_=0.55;
    QVector3D target_{2.5f,1.0f,2.5f};
    char activeAxis_='x';
    char activeTool_='w';
};

} // namespace studio
