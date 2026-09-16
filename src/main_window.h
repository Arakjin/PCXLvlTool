#pragma once

#include "level.h"
#include "game_profile.h"

#include <QMainWindow>

#include <filesystem>
#include <memory>

class QAction;
class QCloseEvent;
class QLabel;
class QLineEdit;
class QListWidget;
class QSpinBox;
class QTabBar;
class LevelCanvas;
class PaletteWidget;
class PaletteIndexSpinBox;
class QComboBox;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    void promptForInitialLevel();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createToolBars();
    void createMaterialDock();
    void createLayerDock();
    void createZoomToolBar();
    void newLevel();
    bool createNewLevel(bool checkUnsavedChanges);
    bool promptForWingsSettings(LevelCreationSettings& settings);
    void editLevelSettings();
    void switchWingsDocument(int index);
    Level* activeLevel() const;
    void configureWingsDocuments();
    void configurePaletteForGame();
    void openLevel();
    void importAutsBmp();
    bool saveProject();
    bool saveProjectAs();
    bool publishLevel();
    bool maybeSave();
    bool writeProject(const std::filesystem::path& path);
    bool writePublishedLevel(const std::filesystem::path& path);
    void uppercaseLevelName();
    void refreshLayerList();
    void editSelectedPaletteColor();
    void loadPalette();
    void savePalette();
    void setModified(bool modified);
    void updateMaterialDetails(int index);
    void updateWindowTitle();

    std::unique_ptr<Level> level_;
    std::unique_ptr<Level> backgroundLevel_;
    LevelCanvas* canvas_ = nullptr;
    PaletteWidget* paletteWidget_ = nullptr;
    PaletteIndexSpinBox* materialIndexSpinBox_ = nullptr;
    PaletteIndexSpinBox* secondaryIndexSpinBox_ = nullptr;
    QComboBox* paletteGroupCombo_ = nullptr;
    QLabel* materialDetailsLabel_ = nullptr;
    QLabel* positionLabel_ = nullptr;
    QLineEdit* levelNameEdit_ = nullptr;
    QListWidget* layerListWidget_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* levelSettingsAction_ = nullptr;
    QAction* importAutsBmpAction_ = nullptr;
    QTabBar* documentTabs_ = nullptr;
    LevelCreationSettings creationSettings_;
    std::filesystem::path projectPath_;
    std::filesystem::path publishPath_;
    bool modified_ = false;
    bool nonUndoModified_ = false;
};
