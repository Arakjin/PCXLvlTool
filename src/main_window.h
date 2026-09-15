#pragma once

#include "level.h"

#include <QMainWindow>

#include <filesystem>
#include <memory>

class QAction;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class LevelCanvas;
class PaletteWidget;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createToolBars();
    void createMaterialDock();
    void createLayerDock();
    void createZoomToolBar();
    void openLevel();
    bool saveProject();
    bool saveProjectAs();
    bool publishLevel();
    bool maybeSave();
    bool writeProject(const std::filesystem::path& path);
    bool writePublishedLevel(const std::filesystem::path& path);
    void refreshLayerList();
    void editSelectedPaletteColor();
    void loadPalette();
    void savePalette();
    void setModified(bool modified);
    void updateMaterialDetails(int index);
    void updateWindowTitle();

    std::unique_ptr<Level> level_;
    LevelCanvas* canvas_ = nullptr;
    PaletteWidget* paletteWidget_ = nullptr;
    QSpinBox* materialIndexSpinBox_ = nullptr;
    QSpinBox* secondaryIndexSpinBox_ = nullptr;
    QLabel* materialDetailsLabel_ = nullptr;
    QLabel* positionLabel_ = nullptr;
    QLineEdit* levelNameEdit_ = nullptr;
    QListWidget* layerListWidget_ = nullptr;
    QAction* saveAction_ = nullptr;
    std::filesystem::path projectPath_;
    std::filesystem::path publishPath_;
    bool modified_ = false;
    bool nonUndoModified_ = false;
};
