#pragma once

#include <QWidget>
#include <QStringList>

class QListWidget;

namespace studio {

// A real entry point, deliberately independent from the editing viewport.
// Example selection never changes a project until MainWindow has checked
// whether the current document contains unsaved edits.
class StartPage final : public QWidget {
    Q_OBJECT
public:
    explicit StartPage(QWidget* parent = nullptr);
    void setRecentProjects(const QStringList& paths);

signals:
    void createJavaBlockRequested();
    void createMultiblockRequested();
    void openProjectRequested();
    void exampleRequested(const QString& id);
    void recentProjectRequested(const QString& path);

private:
    QListWidget* recentList_ = nullptr;
};

} // namespace studio
