#pragma once

#include "level.h"
#include "game_profile.h"
#include "overthrust_model.h"

#include <QMainWindow>

#include <filesystem>
#include <memory>

class QAction;
class QCloseEvent;
class QCheckBox;
class QDockWidget;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QPolygon;
class QSpinBox;
class QTabBar;
class LevelCanvas;
class PaletteWidget;
class PaletteIndexSpinBox;
class QComboBox;
class QString;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;
    void promptForInitialLevel();

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void createActions();
    void createToolBars();
    void createMaterialDock();
    void createLayerDock();
    void createObjectDock();
    void createZoomToolBar();
    void editApplicationSettings();
    void showMarkdownDocument(const QString& title,
                              const QString& resourcePath);
    void showLicenses();
    void newLevel();
    bool createNewLevel(bool checkUnsavedChanges);
    bool promptForWingsSettings(LevelCreationSettings& settings,
                                bool allowDimensionEditing = true);
    bool promptForOverThrustSettings(LevelCreationSettings& settings,
                                     bool allowDimensionEditing = true);
    void editLevelSettings();
    void switchProfileDocument(int index);
    Level* activeLevel() const;
    void configureProfileDocuments();
    void configurePaletteForGame();
    void updateOverThrustBackgroundPreview();
    void openLevel();
    void importImage();
    void exportPcx();
    void installImportedLevel(std::unique_ptr<Level> imported,
                              const LevelCreationSettings& settings,
                              const QString& statusMessage);
    bool saveProject();
    bool saveProjectAs();
    bool publishLevel();
    bool maybeSave();
    bool writeProject(const std::filesystem::path& path);
    bool writePublishedLevel(const std::filesystem::path& path);
    void uppercaseLevelName();
    void refreshLayerList();
    void refreshObjectList();
    void addOverThrustObject();
    void addOverThrustObject(const std::string& type);
    void editSelectedOverThrustObject();
    void duplicateSelectedOverThrustObject();
    void deleteSelectedOverThrustObject();
    void redrawSelectedTeamBase();
    void applyTeamBasePolygon(const QPolygon& polygon, int objectIndex);
    bool editOverThrustObject(OverThrustObject& object, bool creating);
    void applyObjectListChange(const QString& text,
                               std::vector<OverThrustObject> before,
                               std::vector<OverThrustObject> after);
    void editSelectedPaletteColor();
    void loadPalette();
    void savePalette();
    void setModified(bool modified);
    void updateMaterialDetails(int index);
    void updateWindowTitle();

    std::unique_ptr<Level> level_;
    std::unique_ptr<Level> backgroundLevel_;
    std::unique_ptr<OverThrustProjectData> overthrustData_;
    LevelCanvas* canvas_ = nullptr;
    PaletteWidget* paletteWidget_ = nullptr;
    PaletteIndexSpinBox* materialIndexSpinBox_ = nullptr;
    PaletteIndexSpinBox* secondaryIndexSpinBox_ = nullptr;
    QComboBox* paletteGroupCombo_ = nullptr;
    QLabel* materialDetailsLabel_ = nullptr;
    QPushButton* editColorButton_ = nullptr;
    QLabel* positionLabel_ = nullptr;
    QLabel* levelNameLabel_ = nullptr;
    QLineEdit* levelNameEdit_ = nullptr;
    QListWidget* layerListWidget_ = nullptr;
    QListWidget* objectListWidget_ = nullptr;
    QDockWidget* objectDock_ = nullptr;
    QCheckBox* objectModeCheck_ = nullptr;
    QCheckBox* backgroundPreviewCheck_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* publishAction_ = nullptr;
    QAction* levelSettingsAction_ = nullptr;
    QAction* levelNameSeparatorAction_ = nullptr;
    QTabBar* documentTabs_ = nullptr;
    LevelCreationSettings creationSettings_;
    std::filesystem::path projectPath_;
    std::filesystem::path publishPath_;
    bool modified_ = false;
    bool nonUndoModified_ = false;
};
