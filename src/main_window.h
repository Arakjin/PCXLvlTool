#pragma once

#include "level.h"

#include <QMainWindow>

#include <filesystem>
#include <memory>

class QAction;
class QCloseEvent;
class QLabel;
class LevelCanvas;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createMaterialDock();
    void createZoomToolBar();
    void openLevel();
    bool saveLevel();
    bool saveLevelAs();
    bool maybeSave();
    bool writeLevel(const std::filesystem::path& path);
    void setModified(bool modified);
    void updateWindowTitle();

    std::unique_ptr<Level> level_;
    LevelCanvas* canvas_ = nullptr;
    QLabel* positionLabel_ = nullptr;
    QAction* saveAction_ = nullptr;
    std::filesystem::path currentPath_;
    bool modified_ = false;
};
