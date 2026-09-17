#include "main_window.h"

#include "auts_io.h"
#include "default_palette.h"
#include "export_settings.h"
#include "game_profile.h"
#include "lev_reader.h"
#include "lev_writer.h"
#include "layer_model.h"
#include "level_canvas.h"
#include "palette_io.h"
#include "palette_groups.h"
#include "palette_rules.h"
#include "palette_widget.h"
#include "pcx_reader.h"
#include "pcxl_project_io.h"
#include "wings_lev_writer.h"

#include <QAction>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDockWidget>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QInputDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QScrollArea>
#include <QSettings>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTextBrowser>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QValidator>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

class PaletteIndexSpinBox final : public QSpinBox {
public:
    using QSpinBox::QSpinBox;

    void setGame(const GameId game)
    {
        game_ = game;
        if (isReservedColorChartNumber(game_, value())) {
            setValue(game_ == GameId::Wings ? 128
                                            : game_ == GameId::Auts ? 7 : 56);
        }
    }

protected:
    void stepBy(const int steps) override
    {
        const int direction = steps < 0 ? -1 : 1;
        int candidate = value();
        for (int step = 0; step < std::abs(steps); ++step) {
            do {
                candidate += direction;
            } while (candidate >= minimum() && candidate <= maximum() &&
                     isReservedColorChartNumber(game_, candidate));
            candidate = std::clamp(candidate, minimum(), maximum());
        }
        setValue(candidate);
    }

    QValidator::State validate(QString& input, int& position) const override
    {
        const QValidator::State state = QSpinBox::validate(input, position);
        if (state == QValidator::Acceptable &&
            isReservedColorChartNumber(game_, input.toInt())) {
            return QValidator::Intermediate;
        }
        return state;
    }

    void fixup(QString& input) const override
    {
        bool valid = false;
        int value = input.toInt(&valid);
        if (valid && isReservedColorChartNumber(game_, value)) {
            while (value <= maximum() &&
                   isReservedColorChartNumber(game_, value)) {
                ++value;
            }
            input = QString::number(std::min(value, maximum()));
            return;
        }
        QSpinBox::fixup(input);
    }

private:
    GameId game_ = GameId::VWing;
};

namespace {

std::filesystem::path toPath(const QString& path)
{
    return std::filesystem::path(path.toStdU16String());
}

QString toQString(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

QString readResourceText(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QStringLiteral("Unable to load %1").arg(path);
    }
    return QString::fromUtf8(file.readAll());
}

QTextBrowser* markdownBrowser(const QString& resourcePath, QWidget* parent)
{
    auto* browser = new QTextBrowser(parent);
    browser->setMarkdown(readResourceText(resourcePath));
    browser->setOpenExternalLinks(true);
    return browser;
}

QTextBrowser* plainTextBrowser(const QString& resourcePath, QWidget* parent)
{
    auto* browser = new QTextBrowser(parent);
    browser->setPlainText(readResourceText(resourcePath));
    return browser;
}

QString materialDescription(const GameId game, const int paletteIndex)
{
    const int index = colorChartNumber(paletteIndex);
    if (isReservedPaletteIndex(game, paletteIndex)) {
        return QStringLiteral("Reserved (do not use)");
    }
    if (game == GameId::Auts) {
        if (index >= 92 && index <= 95) {
            return QStringLiteral("Docking plate");
        }
        switch (index) {
        case 0: return QStringLiteral("Space");
        case 7: return QStringLiteral("Indestructible");
        case 39:
            return QStringLiteral(
                "Water (maximum 7 short surfaces per level)");
        case 108:
        case 109:
        case 110:
        case 111:
            return QStringLiteral(
                "Gray terrain (do not mix with docking plate colors 92-95)");
        default: return QStringLiteral("Normal terrain / color");
        }
    }
    if (game == GameId::Wings) {
        if (index >= 32 && index <= 37) {
            return QStringLiteral("Base");
        }
        if (index >= 38 && index <= 39) {
            return QStringLiteral("Indestructible base");
        }
        if (index >= 40 && index <= 47) {
            return QStringLiteral("Team base");
        }
        if (index >= 64 && index <= 79) {
            return QStringLiteral("Fly-through background");
        }
        if (index >= 80 && index <= 95) {
            return QStringLiteral("Indestructible terrain");
        }
        if (index >= 96 && index <= 111) {
            return QStringLiteral("Soft terrain");
        }
        if (index >= 112 && index <= 127) {
            return QStringLiteral("Burning terrain");
        }
        if (index >= 128) {
            return QStringLiteral("Normal terrain");
        }
        switch (index) {
        case 0: return QStringLiteral("Background");
        case 16:
            return QStringLiteral(
                "Creates water (fixed palette color 0-47)");
        case 48: return QStringLiteral("Water");
        case 49: return QStringLiteral("Water flow down");
        case 50: return QStringLiteral("Water flow left");
        case 51: return QStringLiteral("Water flow right");
        case 52:
            return QStringLiteral("Bubbles (editable bubble effect color)");
        case 53:
            return QStringLiteral("Snow (editable snow color)");
        case 54: return QStringLiteral("Damaging fire background");
        case 55:
        case 56: return QStringLiteral("Explosive terrain");
        default: return QStringLiteral("Unknown / undocumented");
        }
    }
    if (index >= 20 && index <= 30) {
        return QStringLiteral("Fly through");
    }
    if (index >= 32 && index <= 37) {
        return QStringLiteral("Font");
    }
    if (index >= 40 && index <= 42) {
        return QStringLiteral("Explodes into grenade-launcher shells");
    }
    if (index >= 43 && index <= 44) {
        return QStringLiteral("Explodes like plastic explosive");
    }
    if (index >= 53 && index <= 55) {
        return QStringLiteral("Reserved for fire (do not use)");
    }
    if (index >= 57 && index <= 149) {
        return QStringLiteral("Normal terrain");
    }
    if (index >= 151 && index <= 174) {
        return QStringLiteral("Burns away");
    }
    if (index >= 176 && index <= 199) {
        return QStringLiteral("Burns to ash");
    }
    if (index >= 204 && index <= 219) {
        return QStringLiteral("Turns into water when destroyed");
    }
    if (index >= 221 && index <= 243) {
        return QStringLiteral("Indestructible");
    }
    if (index >= 248 && index <= 255) {
        return QStringLiteral("Indestructible");
    }

    switch (index) {
    case 0:
        return QStringLiteral("Background (always black)");
    case 16:
        return QStringLiteral("Water");
    case 17:
        return QStringLiteral("Waterfall (flows down)");
    case 18:
        return QStringLiteral("Water (flows right)");
    case 19:
        return QStringLiteral("Water (flows left)");
    case 39:
        return QStringLiteral("Ice (not in water; shared game color)");
    case 45:
        return QStringLiteral("Plastic explosive");
    case 46:
        return QStringLiteral("Bird color (changes flying birds globally)");
    case 48:
        return QStringLiteral("Blood color (also changes pilot blood)");
    case 49:
        return QStringLiteral("Clay");
    case 50:
        return QStringLiteral("Base");
    case 51:
        return QStringLiteral("Ash color (shared game color)");
    case 52:
        return QStringLiteral("Snow color (shared game color)");
    case 56:
        return QStringLiteral("Bubble color (shared game color)");
    case 150:
        return QStringLiteral("Does not burn");
    case 175:
        return QStringLiteral("Burns forever (do not use)");
    case 200:
    case 220:
        return QStringLiteral(
            "Indestructible, does not damage Ghostship (do not use)");
    case 201:
        return QStringLiteral("Underwater ash");
    case 202:
        return QStringLiteral("Underwater clay");
    case 203:
        return QStringLiteral("Underwater ice (shared game color)");
    case 244:
        return QStringLiteral(
            "Turret firing-muzzle color (does not place a turret)");
    case 245:
        return QStringLiteral(
            "Turret barrel/body color (does not place a turret)");
    case 246:
        return QStringLiteral(
            "Turret muzzle color (does not place a turret)");
    case 247:
        return QStringLiteral(
            "Turret side color (does not place a turret)");
    default:
        return QStringLiteral("Unknown / undocumented");
    }
}

QIcon toolIcon(const DrawTool tool)
{
    switch (tool) {
    case DrawTool::Pencil:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_pen.png"));
    case DrawTool::Eraser:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_eraser.png"));
    case DrawTool::Line:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_line.png"));
    case DrawTool::Rectangle:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_rectangle.png"));
    case DrawTool::FloodFill:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_flood_fill.png"));
    case DrawTool::Eyedropper:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_color_picker.png"));
    case DrawTool::Ellipse:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_ellipse.png"));
    case DrawTool::Spray:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_spraycan.png"));
    case DrawTool::Text:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_text.png"));
    case DrawTool::Polygon:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_polygon.png"));
    case DrawTool::SelectRectangle:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_rect_selection.png"));
    case DrawTool::SelectEllipse:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_elliptical_selection.png"));
    case DrawTool::SelectFreehand:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_free_form_selection.png"));
    case DrawTool::MoveSelection:
        return QIcon(QStringLiteral(":/icons/icons/hand-move.svg"));
    case DrawTool::BezierCurve:
        return QIcon(QStringLiteral(":/icons/icons/kolourpaint/tool_curve.png"));
    }
    return {};
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), level_(std::make_unique<Level>())
{
    level_->name = "UNTITLED";
    level_->palette = defaultVWingPalette();
    level_->pixels.fill(0);

    auto* central = new QWidget(this);
    auto* centralLayout = new QVBoxLayout(central);
    centralLayout->setContentsMargins(0, 0, 0, 0);
    centralLayout->setSpacing(0);
    documentTabs_ = new QTabBar(central);
    documentTabs_->addTab(tr("Level"));
    documentTabs_->addTab(tr("Background"));
    documentTabs_->setExpanding(false);
    documentTabs_->setVisible(false);
    centralLayout->addWidget(documentTabs_);
    canvas_ = new LevelCanvas(central);
    canvas_->setLevel(level_.get());
    centralLayout->addWidget(canvas_, 1);
    setCentralWidget(central);

    createActions();
    createToolBars();
    createMaterialDock();
    createLayerDock();
    createZoomToolBar();

    positionLabel_ = new QLabel(tr("Ready"), this);
    statusBar()->addPermanentWidget(positionLabel_);
    connect(canvas_, &LevelCanvas::cursorPositionChanged, this,
            [this](const int x, const int y, const int index) {
                positionLabel_->setText(
                    tr("x=%1  y=%2  index=%3").arg(x).arg(y).arg(index));
            });
    connect(canvas_, &LevelCanvas::cursorLeftCanvas, this,
            [this] { positionLabel_->setText(tr("Ready")); });
    connect(canvas_, &LevelCanvas::undoCleanChanged, this,
            [this](const bool clean) {
                Q_UNUSED(clean);
                setModified(nonUndoModified_ || canvas_->hasDirtyUndoStack() ||
                            canvas_->hasPendingSelectionEdit());
            });
    connect(canvas_, &LevelCanvas::pendingSelectionEditChanged, this,
            [this](const bool pending) {
                setModified(nonUndoModified_ || pending ||
                            !canvas_->undoStack()->isClean());
            });
    connect(canvas_, &LevelCanvas::layersChanged, this,
            &MainWindow::refreshLayerList);
    connect(documentTabs_, &QTabBar::currentChanged, this,
            &MainWindow::switchWingsDocument);

    resize(1000, 800);
    updateWindowTitle();
}

MainWindow::~MainWindow()
{
    // Child widgets are destroyed by QMainWindow after this class's members.
    // A dirty undo stack emits cleanChanged while LevelCanvas is torn down;
    // disconnect it before level_ and the other title data cease to exist.
    if (canvas_ != nullptr) {
        QObject::disconnect(canvas_, nullptr, this, nullptr);
    }
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::promptForInitialLevel()
{
    createNewLevel(false);
}

void MainWindow::createActions()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* newAction = fileMenu->addAction(tr("&New level"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::newLevel);

    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openLevel);

    QAction* importAction = fileMenu->addAction(tr("&Import..."));
    connect(importAction, &QAction::triggered, this,
            &MainWindow::importImage);

    saveAction_ = fileMenu->addAction(tr("&Save project"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this,
            [this] { saveProject(); });

    QAction* saveAsAction = fileMenu->addAction(tr("Save project &as..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this,
            [this] { saveProjectAs(); });

    QAction* publishAction = fileMenu->addAction(tr("&Publish LEV..."));
    connect(publishAction, &QAction::triggered, this,
            [this] { publishLevel(); });

    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = editMenu->addAction(tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, [this] {
        canvas_->commitSelection();
        canvas_->undoStack()->undo();
    });
    QAction* redoAction = editMenu->addAction(tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, [this] {
        canvas_->commitSelection();
        canvas_->undoStack()->redo();
    });
    editMenu->addSeparator();
    QAction* selectAllAction = editMenu->addAction(tr("Select &all"));
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(selectAllAction, &QAction::triggered, canvas_,
            &LevelCanvas::selectAll);
    QAction* copyAction = editMenu->addAction(tr("&Copy selection"));
    copyAction->setShortcut(QKeySequence::Copy);
    connect(copyAction, &QAction::triggered, canvas_,
            &LevelCanvas::copySelection);
    QAction* pasteAction = editMenu->addAction(tr("&Paste selection"));
    pasteAction->setShortcut(QKeySequence::Paste);
    connect(pasteAction, &QAction::triggered, canvas_,
            &LevelCanvas::pasteSelection);
    QAction* deleteAction = editMenu->addAction(tr("&Delete selection"));
    deleteAction->setShortcut(QKeySequence::Delete);
    connect(deleteAction, &QAction::triggered, canvas_,
            &LevelCanvas::deleteSelection);

    QMenu* settingsMenu = menuBar()->addMenu(tr("&Settings"));
    QAction* applicationSettingsAction =
        settingsMenu->addAction(tr("&Preferences..."));
    connect(applicationSettingsAction, &QAction::triggered, this,
            &MainWindow::editApplicationSettings);

    QMenu* levelMenu = menuBar()->addMenu(tr("&Level"));
    levelSettingsAction_ =
        levelMenu->addAction(tr("Wings level &settings..."));
    levelSettingsAction_->setEnabled(false);
    connect(levelSettingsAction_, &QAction::triggered, this,
            &MainWindow::editLevelSettings);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    QAction* userGuideAction = helpMenu->addAction(tr("&User guide..."));
    userGuideAction->setShortcut(QKeySequence::HelpContents);
    connect(userGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("PCX Level Tool user guide"),
                             QStringLiteral(":/help/user-guide-fi.md"));
    });

    QMenu* gameGuidesMenu = helpMenu->addMenu(tr("&Game guides"));
    QAction* vwingGuideAction = gameGuidesMenu->addAction(tr("&V-Wing..."));
    connect(vwingGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("V-Wing level guide"),
                             QStringLiteral(":/help/game-guide-vwing-fi.md"));
    });
    QAction* wingsGuideAction = gameGuidesMenu->addAction(tr("&Wings..."));
    connect(wingsGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("Wings level guide"),
                             QStringLiteral(":/help/game-guide-wings-fi.md"));
    });
    QAction* autsGuideAction = gameGuidesMenu->addAction(tr("&AUTS..."));
    connect(autsGuideAction, &QAction::triggered, this, [this] {
        showMarkdownDocument(tr("AUTS level guide"),
                             QStringLiteral(":/help/game-guide-auts-fi.md"));
    });

    helpMenu->addSeparator();
    QAction* licensesAction = helpMenu->addAction(tr("&Licenses..."));
    connect(licensesAction, &QAction::triggered, this,
            &MainWindow::showLicenses);
}

void MainWindow::showMarkdownDocument(const QString& title,
                                      const QString& resourcePath)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    auto* layout = new QVBoxLayout(&dialog);
    layout->addWidget(markdownBrowser(resourcePath, &dialog), 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(850, 700);
    dialog.exec();
}

void MainWindow::showLicenses()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("PCX Level Tool licenses"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* tabs = new QTabWidget(&dialog);
    tabs->addTab(markdownBrowser(QStringLiteral(":/help/licenses-fi.md"), tabs),
                 tr("Summary"));
    tabs->addTab(plainTextBrowser(QStringLiteral(":/help/LICENSE"), tabs),
                 tr("PCX Level Tool"));
    tabs->addTab(markdownBrowser(
                     QStringLiteral(":/help/THIRD_PARTY_NOTICES.md"), tabs),
                 tr("Third-party"));
    tabs->addTab(markdownBrowser(
                     QStringLiteral(":/help/QT-LGPL-NOTICE.md"), tabs),
                 tr("Qt"));
    tabs->addTab(plainTextBrowser(QStringLiteral(":/help/CONVERT.TXT"), tabs),
                 tr("V-Wing converter"));
    layout->addWidget(tabs, 1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(850, 700);
    dialog.exec();
}

void MainWindow::editApplicationSettings()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("PCX Level Tool preferences"));

    auto* layout = new QVBoxLayout(&dialog);
    auto* description = new QLabel(
        tr("Choose the default project folder and the folder used when "
           "publishing a LEV file for each game. You can still choose a "
           "different location in every save dialog."),
        &dialog);
    description->setWordWrap(true);
    layout->addWidget(description);

    auto* group = new QGroupBox(tr("Default folders"), &dialog);
    auto* form = new QFormLayout(group);
    struct DirectoryEditor {
        GameId game;
        QLineEdit* edit;
    };
    std::vector<DirectoryEditor> editors;
    QSettings settings;

    auto* projectRow = new QWidget(group);
    auto* projectRowLayout = new QHBoxLayout(projectRow);
    projectRowLayout->setContentsMargins(0, 0, 0, 0);
    auto* projectEdit = new QLineEdit(projectRow);
    projectEdit->setText(QDir::toNativeSeparators(
        effectiveProjectDirectorySetting(settings)));
    auto* projectBrowse = new QPushButton(tr("Browse..."), projectRow);
    auto* projectDefault = new QPushButton(tr("Default"), projectRow);
    projectRowLayout->addWidget(projectEdit, 1);
    projectRowLayout->addWidget(projectBrowse);
    projectRowLayout->addWidget(projectDefault);
    form->addRow(tr("Projects (.pxlp):"), projectRow);
    connect(projectBrowse, &QPushButton::clicked, &dialog,
            [&dialog, projectEdit] {
                const QString directory = QFileDialog::getExistingDirectory(
                    &dialog, tr("Select PCX Level Tool project folder"),
                    projectEdit->text());
                if (!directory.isEmpty()) {
                    projectEdit->setText(QDir::toNativeSeparators(directory));
                }
            });
    connect(projectDefault, &QPushButton::clicked, &dialog,
            [projectEdit] {
                projectEdit->setText(QDir::toNativeSeparators(
                    defaultProjectDirectory()));
            });

    for (const GameProfile& profile : availableGameProfiles()) {
        auto* row = new QWidget(group);
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* edit = new QLineEdit(row);
        edit->setText(QDir::toNativeSeparators(
            exportDirectorySetting(settings, profile.id)));
        edit->setPlaceholderText(tr("Not set"));
        auto* browse = new QPushButton(tr("Browse..."), row);
        auto* clear = new QPushButton(tr("Clear"), row);
        rowLayout->addWidget(edit, 1);
        rowLayout->addWidget(browse);
        rowLayout->addWidget(clear);

        const QString gameName =
            QString::fromUtf8(profile.displayName.data(),
                              static_cast<qsizetype>(profile.displayName.size()));
        form->addRow(tr("%1:").arg(gameName), row);
        connect(browse, &QPushButton::clicked, &dialog,
                [&dialog, edit, gameName] {
                    const QString directory = QFileDialog::getExistingDirectory(
                        &dialog,
                        tr("Select %1 LEV export folder").arg(gameName),
                        edit->text());
                    if (!directory.isEmpty()) {
                        edit->setText(QDir::toNativeSeparators(directory));
                    }
                });
        connect(clear, &QPushButton::clicked, edit, &QLineEdit::clear);
        editors.push_back({profile.id, edit});
    }
    layout->addWidget(group);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }
    setProjectDirectorySetting(settings, projectEdit->text());
    for (const DirectoryEditor& editor : editors) {
        setExportDirectorySetting(settings, editor.game, editor.edit->text());
    }
    statusBar()->showMessage(tr("Preferences saved"), 3000);
}

void MainWindow::createToolBars()
{
    auto* toolBar = new QToolBar(tr("Tools"), this);
    toolBar->setMovable(false);
    toolBar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, toolBar);
    auto* toolGroup = new QButtonGroup(toolBar);
    toolGroup->setExclusive(true);
    const std::array<std::pair<const char*, DrawTool>, 15> tools{{
        {"Pencil", DrawTool::Pencil},
        {"Eraser", DrawTool::Eraser},
        {"Spray", DrawTool::Spray},
        {"Line", DrawTool::Line},
        {"Bezier curve", DrawTool::BezierCurve},
        {"Rectangle", DrawTool::Rectangle},
        {"Ellipse", DrawTool::Ellipse},
        {"Polygon", DrawTool::Polygon},
        {"Flood fill", DrawTool::FloodFill},
        {"Eyedropper", DrawTool::Eyedropper},
        {"Text", DrawTool::Text},
        {"Rectangle select", DrawTool::SelectRectangle},
        {"Ellipse select", DrawTool::SelectEllipse},
        {"Freehand select", DrawTool::SelectFreehand},
        {"Move selection", DrawTool::MoveSelection},
    }};
    for (const auto& [label, tool] : tools) {
        auto* button = new QToolButton(toolBar);
        button->setCheckable(true);
        button->setIcon(toolIcon(tool));
        button->setIconSize(QSize(28, 28));
        button->setFixedSize(38, 38);
        button->setStyleSheet(QStringLiteral(
            "QToolButton { background: #f5f5f5; border: 1px solid #808080; }"
            "QToolButton:hover { background: #e8f2ff; }"
            "QToolButton:checked { background: #b9d9ff; border: 2px inset "
            "#5078a0; }"));
        button->setToolTip(tr(label));
        if (tool == DrawTool::Text) {
            button->setToolTip(tr("Text: drag an area, type directly, drag "
                                  "the box to move it; Ctrl+Enter or clicking "
                                  "outside accepts"));
        } else if (tool == DrawTool::Polygon) {
            button->setToolTip(tr("Polygon: click corners, double-click or "
                                  "press Enter to finish"));
        } else if (tool == DrawTool::Line) {
            button->setToolTip(
                tr("Line: hold Shift for horizontal, vertical, or 45-degree "
                   "directions"));
        } else if (tool == DrawTool::BezierCurve) {
            button->setToolTip(
                tr("Bezier curve: hold Shift to constrain only the initial "
                   "line; control-point adjustments remain free"));
        } else if (tool == DrawTool::SelectRectangle ||
                   tool == DrawTool::SelectEllipse ||
                   tool == DrawTool::SelectFreehand) {
            button->setToolTip(
                tr("Selection: Shift adds, Ctrl subtracts, Shift+Ctrl "
                   "intersects with the current selection"));
        } else if (tool == DrawTool::Eraser) {
            button->setToolTip(tr("Eraser: makes upper layers transparent; "
                                  "writes file index 0 on "
                                  "Background"));
        }
        button->setAccessibleName(tr(label));
        toolGroup->addButton(button, static_cast<int>(tool));
        toolBar->addWidget(button);
        if (tool == DrawTool::Pencil) {
            button->setChecked(true);
        }
    }
    addToolBarBreak(Qt::TopToolBarArea);
    auto* optionsBar = new QToolBar(tr("Tool options"), this);
    optionsBar->setMovable(false);
    optionsBar->setFloatable(false);
    addToolBar(Qt::TopToolBarArea, optionsBar);
    auto* activeToolLabel = new QLabel(tr("Pencil"), optionsBar);
    activeToolLabel->setMinimumWidth(90);
    optionsBar->addWidget(activeToolLabel);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Thickness: "), optionsBar));
    auto* thicknessSpinBox = new QSpinBox(optionsBar);
    thicknessSpinBox->setRange(1, 32);
    thicknessSpinBox->setSuffix(tr(" px"));
    thicknessSpinBox->setValue(canvas_->toolThickness(DrawTool::Pencil));
    optionsBar->addWidget(thicknessSpinBox);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Tip: "), optionsBar));
    auto* brushShapeCombo = new QComboBox(optionsBar);
    brushShapeCombo->addItem(tr("Square"),
                             static_cast<int>(BrushShape::Square));
    brushShapeCombo->addItem(tr("Circle"),
                             static_cast<int>(BrushShape::Circle));
    brushShapeCombo->setToolTip(
        tr("Pencil and eraser footprint shown under the pointer"));
    optionsBar->addWidget(brushShapeCombo);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Shape: "), optionsBar));
    auto* shapeModeCombo = new QComboBox(optionsBar);
    shapeModeCombo->addItem(tr("Outline"), static_cast<int>(ShapeMode::Outline));
    shapeModeCombo->addItem(tr("Outline + fill"),
                            static_cast<int>(ShapeMode::OutlineAndFill));
    shapeModeCombo->addItem(tr("Fill only"),
                            static_cast<int>(ShapeMode::FillOnly));
    shapeModeCombo->setEnabled(false);
    optionsBar->addWidget(shapeModeCombo);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Corners: "), optionsBar));
    auto* cornerRadiusCombo = new QComboBox(optionsBar);
    cornerRadiusCombo->addItem(tr("Sharp"), 0);
    cornerRadiusCombo->addItem(tr("2 px"), 2);
    cornerRadiusCombo->addItem(tr("4 px"), 4);
    cornerRadiusCombo->addItem(tr("8 px"), 8);
    cornerRadiusCombo->addItem(tr("16 px"), 16);
    cornerRadiusCombo->addItem(tr("32 px"), 32);
    cornerRadiusCombo->setEnabled(false);
    optionsBar->addWidget(cornerRadiusCombo);
    optionsBar->addSeparator();
    optionsBar->addWidget(new QLabel(tr("Font: "), optionsBar));
    auto* fontCombo = new QFontComboBox(optionsBar);
    fontCombo->setEnabled(false);
    fontCombo->setMaximumWidth(180);
    optionsBar->addWidget(fontCombo);
    auto* textSizeSpinBox = new QSpinBox(optionsBar);
    textSizeSpinBox->setRange(6, 64);
    textSizeSpinBox->setValue(12);
    textSizeSpinBox->setSuffix(tr(" px"));
    textSizeSpinBox->setEnabled(false);
    optionsBar->addWidget(textSizeSpinBox);
    connect(
        toolGroup, &QButtonGroup::idClicked, this,
        [this, activeToolLabel, thicknessSpinBox, cornerRadiusCombo,
         brushShapeCombo, shapeModeCombo, fontCombo, textSizeSpinBox,
         tools](const int id) {
            const auto selectedTool = static_cast<DrawTool>(id);
            canvas_->setDrawTool(selectedTool);
            for (const auto& [label, tool] : tools) {
                if (static_cast<int>(tool) == id) {
                    activeToolLabel->setText(tr(label));
                    break;
                }
            }
            const bool supportsThickness =
                selectedTool == DrawTool::Pencil ||
                selectedTool == DrawTool::Eraser ||
                selectedTool == DrawTool::Line ||
                selectedTool == DrawTool::Rectangle ||
                selectedTool == DrawTool::Ellipse ||
                selectedTool == DrawTool::Polygon ||
                selectedTool == DrawTool::Spray ||
                selectedTool == DrawTool::BezierCurve;
            const QSignalBlocker blocker(thicknessSpinBox);
            thicknessSpinBox->setEnabled(supportsThickness);
            thicknessSpinBox->setValue(canvas_->toolThickness(selectedTool));
            const bool supportsBrushShape = selectedTool == DrawTool::Pencil ||
                                            selectedTool == DrawTool::Eraser;
            brushShapeCombo->setEnabled(supportsBrushShape);
            if (supportsBrushShape) {
                const QSignalBlocker brushShapeBlocker(brushShapeCombo);
                brushShapeCombo->setCurrentIndex(brushShapeCombo->findData(
                    static_cast<int>(canvas_->brushShape(selectedTool))));
            }
            const bool supportsCorners = selectedTool == DrawTool::Rectangle;
            cornerRadiusCombo->setEnabled(supportsCorners);
            if (supportsCorners) {
                cornerRadiusCombo->setCurrentIndex(cornerRadiusCombo->findData(
                    canvas_->rectangleCornerRadius()));
            }
            const bool supportsFill = selectedTool == DrawTool::Rectangle ||
                                      selectedTool == DrawTool::Ellipse ||
                                      selectedTool == DrawTool::Polygon;
            shapeModeCombo->setEnabled(supportsFill);
            const bool supportsText = selectedTool == DrawTool::Text;
            fontCombo->setEnabled(supportsText);
            textSizeSpinBox->setEnabled(supportsText);
        });
    connect(thicknessSpinBox, &QSpinBox::valueChanged, this,
            [this, toolGroup](const int value) {
                canvas_->setToolThickness(
                    static_cast<DrawTool>(toolGroup->checkedId()), value);
            });
    connect(brushShapeCombo, &QComboBox::currentIndexChanged, this,
            [this, toolGroup, brushShapeCombo](const int index) {
                canvas_->setBrushShape(
                    static_cast<DrawTool>(toolGroup->checkedId()),
                    static_cast<BrushShape>(
                        brushShapeCombo->itemData(index).toInt()));
            });
    connect(cornerRadiusCombo, &QComboBox::currentIndexChanged, this,
            [this, cornerRadiusCombo](const int index) {
                canvas_->setRectangleCornerRadius(
                    cornerRadiusCombo->itemData(index).toInt());
            });
    connect(shapeModeCombo, &QComboBox::currentIndexChanged, this,
            [this, shapeModeCombo](const int index) {
                const auto mode = static_cast<ShapeMode>(
                    shapeModeCombo->itemData(index).toInt());
                canvas_->setShapeMode(mode);
            });
    connect(fontCombo, &QFontComboBox::currentFontChanged, this,
            [this](const QFont& font) {
                canvas_->setTextFontFamily(font.family());
            });
    canvas_->setTextFontFamily(fontCombo->currentFont().family());
    connect(textSizeSpinBox, &QSpinBox::valueChanged, canvas_,
            &LevelCanvas::setTextPixelSize);
}

void MainWindow::createMaterialDock()
{
    auto* dock = new QDockWidget(tr("Palette"), this);
    auto* contents = new QWidget(dock);
    auto* layout = new QVBoxLayout(contents);
    layout->addWidget(new QLabel(tr("Palette"), contents));
    paletteGroupCombo_ = new QComboBox(contents);
    const std::array<std::pair<const char*, PaletteGroup>, 11> paletteGroups{{
        {"All documented usable", PaletteGroup::AllUsable},
        {"Background (0)", PaletteGroup::Background},
        {"Water (16-19)", PaletteGroup::Water},
        {"Fly through (20-30)", PaletteGroup::FlyThrough},
        {"Font (32-37)", PaletteGroup::Font},
        {"Special materials and game colors (39-56)",
         PaletteGroup::Special},
        {"Normal terrain (57-149)", PaletteGroup::NormalTerrain},
        {"Burnable (150-199)", PaletteGroup::Burnable},
        {"Underwater (201-219)", PaletteGroup::Underwater},
        {"Indestructible (221-243, 248-255)", PaletteGroup::Indestructible},
        {"Turret colors (244-247; not placement)", PaletteGroup::Turrets},
    }};
    for (const auto& [label, group] : paletteGroups) {
        paletteGroupCombo_->addItem(tr(label), static_cast<int>(group));
    }
    layout->addWidget(paletteGroupCombo_);
    paletteWidget_ = new PaletteWidget(contents);
    paletteWidget_->setLevel(level_.get());
    paletteWidget_->setIndices(
        paletteIndices(PaletteGroup::AllUsable, GameId::VWing));
    layout->addWidget(paletteWidget_);

    auto* indexLayout = new QHBoxLayout();
    indexLayout->addWidget(new QLabel(tr("Left material index:"), contents));
    materialIndexSpinBox_ = new PaletteIndexSpinBox(contents);
    materialIndexSpinBox_->setRange(0, 255);
    materialIndexSpinBox_->setValue(57);
    materialIndexSpinBox_->setToolTip(
        tr("Reserved material indices cannot be selected"));
    indexLayout->addWidget(materialIndexSpinBox_);
    layout->addLayout(indexLayout);
    auto* secondaryIndexLayout = new QHBoxLayout();
    secondaryIndexLayout->addWidget(
        new QLabel(tr("Right material index:"), contents));
    secondaryIndexSpinBox_ = new PaletteIndexSpinBox(contents);
    secondaryIndexSpinBox_->setRange(0, 255);
    secondaryIndexSpinBox_->setValue(58);
    secondaryIndexSpinBox_->setToolTip(
        tr("Right-click drawing and filled-shape interior material index"));
    secondaryIndexLayout->addWidget(secondaryIndexSpinBox_);
    layout->addLayout(secondaryIndexLayout);
    materialDetailsLabel_ = new QLabel(contents);
    layout->addWidget(materialDetailsLabel_);
    editColorButton_ =
        new QPushButton(tr("Edit selected color..."), contents);
    layout->addWidget(editColorButton_);
    auto* paletteFileLayout = new QHBoxLayout();
    auto* loadPaletteButton = new QPushButton(tr("Load palette..."), contents);
    auto* savePaletteButton = new QPushButton(tr("Save palette..."), contents);
    paletteFileLayout->addWidget(loadPaletteButton);
    paletteFileLayout->addWidget(savePaletteButton);
    layout->addLayout(paletteFileLayout);
    layout->addStretch();
    connect(paletteGroupCombo_, &QComboBox::currentIndexChanged, this,
            [this](const int index) {
                auto indices = paletteIndices(static_cast<PaletteGroup>(
                    paletteGroupCombo_->itemData(index).toInt()),
                    creationSettings_.game);
                const auto selected = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(materialIndexSpinBox_->value()));
                const auto secondary = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(secondaryIndexSpinBox_->value()));
                const bool selectionVisible =
                    std::find(indices.begin(), indices.end(), selected) !=
                    indices.end();
                const bool secondaryVisible =
                    std::find(indices.begin(), indices.end(), secondary) !=
                    indices.end();
                const int firstIndex = indices.empty() ? 0 : indices.front();
                paletteWidget_->setIndices(std::move(indices));
                if (!selectionVisible) {
                    materialIndexSpinBox_->setValue(
                        colorChartNumber(firstIndex));
                }
                if (!secondaryVisible) {
                    secondaryIndexSpinBox_->setValue(
                        colorChartNumber(firstIndex));
                }
            });
    connect(materialIndexSpinBox_, &QSpinBox::valueChanged, this,
            [this](const int value) {
                const auto index = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(value));
                canvas_->setSelectedIndex(index);
                paletteWidget_->setSelectedIndex(index);
                updateMaterialDetails(index);
            });
    connect(canvas_, &LevelCanvas::selectedIndexChanged, this,
            [this](const int index) {
                materialIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(secondaryIndexSpinBox_, &QSpinBox::valueChanged, this,
            [this](const int value) {
                const auto index = static_cast<std::uint8_t>(
                    paletteIndexFromColorChart(value));
                canvas_->setSecondaryIndex(index);
                paletteWidget_->setSecondaryIndex(index);
            });
    connect(canvas_, &LevelCanvas::secondaryIndexChanged, this,
            [this](const int index) {
                secondaryIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(paletteWidget_, &PaletteWidget::indexSelected, this,
            [this](const int index) {
                materialIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(paletteWidget_, &PaletteWidget::secondaryIndexSelected, this,
            [this](const int index) {
                secondaryIndexSpinBox_->setValue(colorChartNumber(index));
            });
    connect(paletteWidget_, &PaletteWidget::indexEditRequested, this,
            [this](const int index) {
                materialIndexSpinBox_->setValue(colorChartNumber(index));
                editSelectedPaletteColor();
            });
    connect(editColorButton_, &QPushButton::clicked, this,
            &MainWindow::editSelectedPaletteColor);
    connect(loadPaletteButton, &QPushButton::clicked, this,
            &MainWindow::loadPalette);
    connect(savePaletteButton, &QPushButton::clicked, this,
            &MainWindow::savePalette);
    connect(canvas_, &LevelCanvas::paletteColorChanged, this,
            [this](const int index) {
                if (creationSettings_.game == GameId::Wings &&
                    backgroundLevel_) {
                    Level* source = activeLevel();
                    Level* target = source == level_.get()
                                        ? backgroundLevel_.get()
                                        : level_.get();
                    if (index < 0) {
                        target->palette = source->palette;
                    } else {
                        target->palette[static_cast<std::size_t>(index)] =
                            source->palette[static_cast<std::size_t>(index)];
                    }
                }
                paletteWidget_->update();
                const int selectedIndex = paletteIndexFromColorChart(
                    materialIndexSpinBox_->value());
                if (index < 0 || index == selectedIndex) {
                    updateMaterialDetails(selectedIndex);
                }
            });
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));

    auto* scrollArea = new QScrollArea(dock);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setWidget(contents);
    dock->setWidget(scrollArea);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::createZoomToolBar()
{
    QToolBar* toolBar = addToolBar(tr("View"));
    levelNameLabel_ = new QLabel(tr("Level name: "), toolBar);
    toolBar->addWidget(levelNameLabel_);
    levelNameEdit_ = new QLineEdit(
        QString::fromLatin1(level_->name.data(),
                            static_cast<int>(level_->name.size())),
        toolBar);
    levelNameEdit_->setMaxLength(20);
    levelNameEdit_->setMaximumWidth(190);
    levelNameEdit_->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[\\x20-\\x7E]{0,20}")),
        levelNameEdit_));
    levelNameEdit_->setToolTip(
        tr("Maximum 20 printable ASCII characters. Ä, Ö, Å and other "
           "non-ASCII characters are not supported by LEV files."));
    connect(levelNameEdit_, &QLineEdit::textEdited, this,
            [this](const QString& text) {
                const std::string name = text.toLatin1().toStdString();
                if (name == level_->name) {
                    return;
                }
                level_->name = name;
                creationSettings_.name = name;
                nonUndoModified_ = true;
                setModified(true);
            });
    toolBar->addWidget(levelNameEdit_);
    levelNameSeparatorAction_ = toolBar->addSeparator();
    toolBar->addWidget(new QLabel(tr("Zoom: "), toolBar));
    auto* zoomCombo = new QComboBox(toolBar);
    const std::array<std::pair<const char*, double>, 6> zoomLevels{{
        {"25%", 0.25},
        {"50%", 0.5},
        {"100%", 1.0},
        {"200%", 2.0},
        {"400%", 4.0},
        {"800%", 8.0},
    }};
    for (const auto& [label, value] : zoomLevels) {
        zoomCombo->addItem(QString::fromLatin1(label), value);
    }
    zoomCombo->setCurrentIndex(2);
    connect(zoomCombo, &QComboBox::currentIndexChanged, this,
            [this, zoomCombo](const int index) {
                canvas_->setZoom(zoomCombo->itemData(index).toDouble());
            });
    connect(canvas_, &LevelCanvas::zoomChanged, zoomCombo,
            [zoomCombo](const double zoom) {
                const QSignalBlocker blocker(zoomCombo);
                zoomCombo->setCurrentIndex(zoomCombo->findData(zoom));
            });
    toolBar->addWidget(zoomCombo);
    toolBar->addSeparator();
    auto* publishButton = new QPushButton(tr("Publish LEV..."), toolBar);
    publishButton->setToolTip(
        tr("Flatten visible layers and write a game-compatible LEV file"));
    connect(publishButton, &QPushButton::clicked, this,
            [this] { publishLevel(); });
    toolBar->addWidget(publishButton);
}

void MainWindow::createLayerDock()
{
    auto* dock = new QDockWidget(tr("Layers"), this);
    auto* contents = new QWidget(dock);
    auto* layout = new QVBoxLayout(contents);
    layout->addWidget(new QLabel(
        tr("Top layer is drawn first. Unchecked layers are hidden."), contents));

    layerListWidget_ = new QListWidget(contents);
    layerListWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(layerListWidget_);

    auto* firstRow = new QHBoxLayout();
    auto* addButton = new QPushButton(tr("Add"), contents);
    auto* duplicateButton = new QPushButton(tr("Duplicate"), contents);
    auto* deleteButton = new QPushButton(tr("Delete"), contents);
    firstRow->addWidget(addButton);
    firstRow->addWidget(duplicateButton);
    firstRow->addWidget(deleteButton);
    layout->addLayout(firstRow);

    auto* secondRow = new QHBoxLayout();
    auto* upButton = new QPushButton(tr("Up"), contents);
    auto* downButton = new QPushButton(tr("Down"), contents);
    auto* renameButton = new QPushButton(tr("Rename"), contents);
    auto* lockButton = new QPushButton(tr("Lock / unlock"), contents);
    secondRow->addWidget(upButton);
    secondRow->addWidget(downButton);
    secondRow->addWidget(renameButton);
    secondRow->addWidget(lockButton);
    layout->addLayout(secondRow);

    dock->setWidget(contents);
    addDockWidget(Qt::RightDockWidgetArea, dock);

    connect(layerListWidget_, &QListWidget::currentItemChanged, this,
            [this](QListWidgetItem* current, QListWidgetItem*) {
                if (current != nullptr) {
                    canvas_->setActiveLayer(
                        current->data(Qt::UserRole).toInt());
                }
            });
    connect(layerListWidget_, &QListWidget::itemChanged, this,
            [this](QListWidgetItem* item) {
                const int index = item->data(Qt::UserRole).toInt();
                const bool visible = item->checkState() == Qt::Checked;
                Level* document = activeLevel();
                if (index > 0 &&
                    document->layers[static_cast<std::size_t>(index)].visible !=
                        visible) {
                    canvas_->setLayerVisible(index, visible);
                    nonUndoModified_ = true;
                    setModified(true);
                }
            });
    connect(addButton, &QPushButton::clicked, this, [this] {
        if (canvas_->addLayer()) {
            nonUndoModified_ = true;
            setModified(true);
        } else {
            statusBar()->showMessage(tr("A project can contain at most 5 layers"),
                                     3000);
        }
    });
    connect(duplicateButton, &QPushButton::clicked, this, [this] {
        if (canvas_->duplicateActiveLayer()) {
            nonUndoModified_ = true;
            setModified(true);
        } else {
            statusBar()->showMessage(tr("The layer could not be duplicated"),
                                     3000);
        }
    });
    connect(deleteButton, &QPushButton::clicked, this, [this] {
        if (canvas_->deleteActiveLayer()) {
            nonUndoModified_ = true;
            setModified(true);
        } else {
            statusBar()->showMessage(tr("Background cannot be deleted"), 3000);
        }
    });
    connect(upButton, &QPushButton::clicked, this, [this] {
        if (canvas_->moveActiveLayer(1)) {
            nonUndoModified_ = true;
            setModified(true);
        }
    });
    connect(downButton, &QPushButton::clicked, this, [this] {
        if (canvas_->moveActiveLayer(-1)) {
            nonUndoModified_ = true;
            setModified(true);
        }
    });
    connect(renameButton, &QPushButton::clicked, this, [this] {
        const int index = canvas_->activeLayerIndex();
        if (index <= 0) {
            statusBar()->showMessage(tr("Background cannot be renamed"), 3000);
            return;
        }
        bool accepted = false;
        const QString current = QString::fromStdString(
            activeLevel()->layers[static_cast<std::size_t>(index)].name);
        const QString name = QInputDialog::getText(
            this, tr("Rename layer"), tr("Layer name:"), QLineEdit::Normal,
            current, &accepted);
        if (accepted && !name.trimmed().isEmpty() && name != current) {
            canvas_->renameLayer(index, name.trimmed());
            nonUndoModified_ = true;
            setModified(true);
        }
    });
    connect(lockButton, &QPushButton::clicked, this, [this] {
        const int index = canvas_->activeLayerIndex();
        if (index < 0) {
            return;
        }
        const bool locked = activeLevel()
                                ->layers[static_cast<std::size_t>(index)]
                                .locked;
        canvas_->setLayerLocked(index, !locked);
        nonUndoModified_ = true;
        setModified(true);
    });
    refreshLayerList();
}

void MainWindow::refreshLayerList()
{
    Level* document = activeLevel();
    if (layerListWidget_ == nullptr || document == nullptr) {
        return;
    }
    const QSignalBlocker blocker(layerListWidget_);
    layerListWidget_->clear();
    for (std::size_t index = document->layers.size(); index-- > 0;) {
        const Level::Layer& layer = document->layers[index];
        QString label = QString::fromStdString(layer.name);
        if (layer.locked) {
            label += tr("  [locked]");
        }
        auto* item = new QListWidgetItem(label, layerListWidget_);
        item->setData(Qt::UserRole, static_cast<int>(index));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(layer.visible ? Qt::Checked : Qt::Unchecked);
        if (index == 0) {
            item->setFlags(item->flags() & ~Qt::ItemIsUserCheckable);
            item->setToolTip(tr("Background is always visible and always bottom"));
        }
        if (index == document->activeLayer) {
            layerListWidget_->setCurrentItem(item);
        }
    }
}

void MainWindow::openLevel()
{
    if (!maybeSave()) {
        return;
    }

    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Open project or level"), QString(),
        tr("PCX Level Tool projects and levels (*.pxlp *.PXLP *.lev *.LEV);;"
           "PCX Level Tool projects (*.pxlp *.PXLP);;"
           "Supported game levels (*.lev *.LEV)"));
    if (filename.isEmpty()) {
        return;
    }

    auto loaded = std::make_unique<Level>();
    std::string error;
    const std::filesystem::path path = toPath(filename);
    const QString suffix = QFileInfo(filename).suffix();
    const bool isPxlProject =
        QString::compare(suffix, QStringLiteral("pxlp"),
                         Qt::CaseInsensitive) == 0;
    const bool isProject = isPxlProject;
    std::unique_ptr<Level> loadedBackground;
    LevelCreationSettings loadedSettings;
    bool loadedSuccessfully = false;
    if (isPxlProject) {
        loadedSuccessfully = loadPxlProject(
            path, *loaded, loadedBackground, loadedSettings, error);
    } else {
        std::string vwingError;
        loadedSuccessfully = loadLev(path, *loaded, vwingError);
        if (!loadedSuccessfully) {
            std::string autsError;
            loadedSuccessfully = loadAutsLev(path, *loaded, autsError);
            if (loadedSuccessfully) {
                loadedSettings.game = GameId::Auts;
            } else {
                error = "not a supported V-Wing or AUTS LEV file\nV-Wing: " +
                        vwingError + "\nAUTS: " + autsError;
            }
        }
    }
    if (!loadedSuccessfully) {
        QMessageBox::critical(this, tr("Open failed"),
                              QString::fromStdString(error));
        return;
    }

    // Drop commands while their old Level target is still alive.
    canvas_->forgetLevel(backgroundLevel_.get());
    canvas_->forgetLevel(level_.get());
    level_ = std::move(loaded);
    backgroundLevel_ = std::move(loadedBackground);
    creationSettings_ = loadedSettings;
    if (!isPxlProject) {
        if (creationSettings_.game != GameId::Auts) {
            creationSettings_.game = GameId::VWing;
        }
        creationSettings_.name = level_->name;
        creationSettings_.width = static_cast<int>(level_->width);
        creationSettings_.height = static_cast<int>(level_->height);
    }
    levelSettingsAction_->setEnabled(creationSettings_.game == GameId::Wings);
    documentTabs_->setCurrentIndex(0);
    documentTabs_->setVisible(creationSettings_.game == GameId::Wings);
    configurePaletteForGame();
    if (creationSettings_.game == GameId::Wings) {
        configureWingsDocuments();
    }
    projectPath_ = isProject ? path : std::filesystem::path{};
    publishPath_ = isProject ? std::filesystem::path{} : path;
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(
            QString::fromLatin1(level_->name.data(),
                                static_cast<int>(level_->name.size())));
    }
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));
    refreshLayerList();
    canvas_->undoStack()->setClean();
    nonUndoModified_ = false;
    setModified(false);
    statusBar()->showMessage(tr("Opened %1").arg(filename), 3000);
}

void MainWindow::importImage()
{
    QStringList gameNames;
    for (const GameProfile& profile : availableGameProfiles()) {
        gameNames.push_back(QString::fromUtf8(profile.displayName.data(),
                                              profile.displayName.size()));
    }
    bool selected = false;
    const QString gameName = QInputDialog::getItem(
        this, tr("Import image"), tr("Target game:"), gameNames, 0, false,
        &selected);
    if (!selected) {
        return;
    }
    const int selectedIndex = gameNames.indexOf(gameName);
    if (selectedIndex < 0 ||
        selectedIndex >= static_cast<int>(availableGameProfiles().size())) {
        return;
    }
    const GameProfile& profile =
        availableGameProfiles()[static_cast<std::size_t>(selectedIndex)];

    const bool auts = profile.id == GameId::Auts;
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Import image for %1").arg(gameName), QString(),
        auts ? tr("Indexed images (*.pcx *.PCX *.bmp *.BMP)")
             : tr("8-bit indexed PCX images (*.pcx *.PCX)"));
    if (filename.isEmpty()) {
        return;
    }

    auto imported = std::make_unique<Level>();
    bool paletteMatches = true;
    std::string error;
    const bool bmp = QFileInfo(filename).suffix().compare(
                         QStringLiteral("bmp"), Qt::CaseInsensitive) == 0;
    const bool loaded =
        bmp ? auts && loadAutsBmp(toPath(filename), *imported, paletteMatches,
                                  error)
            : loadPcx(toPath(filename), *imported, error);
    if (!loaded) {
        if (bmp && !auts) {
            error = "BMP import is currently supported only for AUTS";
        }
        QMessageBox::critical(this, tr("Image import failed"),
                              QString::fromStdString(error));
        return;
    }

    if (imported->width < static_cast<std::size_t>(profile.minimumWidth) ||
        imported->width > static_cast<std::size_t>(profile.maximumWidth) ||
        imported->height < static_cast<std::size_t>(profile.minimumHeight) ||
        imported->height > static_cast<std::size_t>(profile.maximumHeight)) {
        QMessageBox::critical(
            this, tr("Wrong image dimensions"),
            tr("%1 requires an image between %2 x %3 and %4 x %5 pixels. "
               "The selected image is %6 x %7.")
                .arg(gameName)
                .arg(profile.minimumWidth)
                .arg(profile.minimumHeight)
                .arg(profile.maximumWidth)
                .arg(profile.maximumHeight)
                .arg(imported->width)
                .arg(imported->height));
        return;
    }

    LevelCreationSettings settings;
    settings.game = profile.id;
    settings.name = imported->name;
    settings.width = static_cast<int>(imported->width);
    settings.height = static_cast<int>(imported->height);
    bool paletteAdjusted = false;
    if (profile.id == GameId::Auts) {
        paletteAdjusted = !paletteMatches ||
                          imported->palette != defaultAutsPalette();
        applyAutsGameRules(*imported);
    } else if (profile.id == GameId::Wings) {
        const auto fixed = defaultWingsPalette();
        for (std::size_t index = 0; index < imported->palette.size(); ++index) {
            if (index < 48 ||
                isReservedPaletteIndex(GameId::Wings,
                                       static_cast<int>(index))) {
                paletteAdjusted = paletteAdjusted ||
                                  !(imported->palette[index] == fixed[index]);
                imported->palette[index] = fixed[index];
            }
        }
        if (!promptForWingsSettings(settings, false)) {
            return;
        }
        imported->name = settings.name;
    }
    if (!maybeSave()) {
        return;
    }

    installImportedLevel(
        std::move(imported), settings,
        tr("Imported %1 for %2").arg(filename, gameName));
    if (profile.id == GameId::Auts || profile.id == GameId::VWing) {
        uppercaseLevelName();
    }
    if (paletteAdjusted) {
        QMessageBox::information(
            this, tr("Game palette rules applied"),
            profile.id == GameId::Auts
                ? tr("Pixel indices were preserved and the fixed AUTS palette "
                     "and boundary were applied.")
                : tr("Pixel indices were preserved. Wings' fixed and reserved "
                     "palette entries were restored to their game values."));
    }
}

void MainWindow::installImportedLevel(
    std::unique_ptr<Level> imported, const LevelCreationSettings& settings,
    const QString& statusMessage)
{
    canvas_->forgetLevel(backgroundLevel_.get());
    canvas_->forgetLevel(level_.get());
    level_ = std::move(imported);
    backgroundLevel_.reset();
    creationSettings_ = settings;
    projectPath_.clear();
    publishPath_.clear();
    documentTabs_->setCurrentIndex(0);
    configurePaletteForGame();
    configureWingsDocuments();
    levelSettingsAction_->setEnabled(settings.game == GameId::Wings);
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(QString::fromStdString(level_->name));
    }
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));
    refreshLayerList();
    canvas_->undoStack()->setClean();
    nonUndoModified_ = true;
    setModified(true);
    statusBar()->showMessage(statusMessage, 4000);
}

void MainWindow::newLevel()
{
    createNewLevel(true);
}

bool MainWindow::createNewLevel(const bool checkUnsavedChanges)
{
    if (checkUnsavedChanges && !maybeSave()) {
        return false;
    }

    QStringList gameNames;
    for (const GameProfile& profile : availableGameProfiles()) {
        gameNames.push_back(QString::fromUtf8(profile.displayName.data(),
                                              profile.displayName.size()));
    }
    bool selected = false;
    const QString gameName = QInputDialog::getItem(
        this, tr("New level"), tr("Game:"), gameNames, 0, false, &selected);
    if (!selected) {
        return false;
    }
    const int selectedIndex = gameNames.indexOf(gameName);
    if (selectedIndex < 0 ||
        selectedIndex >= static_cast<int>(availableGameProfiles().size())) {
        return false;
    }

    LevelCreationSettings settings;
    const GameProfile& profile =
        availableGameProfiles()[static_cast<std::size_t>(selectedIndex)];
    settings.game = profile.id;
    settings.width = profile.defaultWidth;
    settings.height = profile.defaultHeight;
    if (profile.id == GameId::Wings && !promptForWingsSettings(settings)) {
        return false;
    }

    auto fresh = std::make_unique<Level>();
    fresh->name = settings.name;
    if (settings.game == GameId::Wings) {
        fresh->palette = defaultWingsPalette();
    } else if (settings.game == GameId::Auts) {
        initializeBlankAutsLevel(*fresh);
        fresh->name = settings.name;
    } else {
        fresh->palette = defaultVWingPalette();
    }
    if (settings.game == GameId::Wings) {
        fresh->resize(static_cast<std::size_t>(settings.width),
                      static_cast<std::size_t>(settings.height));
    } else if (settings.game == GameId::VWing) {
        fresh->pixels.fill(0);
    }

    // Drop commands while their old Level target is still alive.
    canvas_->forgetLevel(backgroundLevel_.get());
    canvas_->forgetLevel(level_.get());
    level_ = std::move(fresh);
    creationSettings_ = settings;
    documentTabs_->setCurrentIndex(0);
    configurePaletteForGame();
    configureWingsDocuments();
    levelSettingsAction_->setEnabled(settings.game == GameId::Wings);
    projectPath_.clear();
    publishPath_.clear();
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(QString::fromLatin1(
            level_->name.data(), static_cast<int>(level_->name.size())));
    }
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));
    refreshLayerList();
    canvas_->undoStack()->setClean();
    nonUndoModified_ = false;
    setModified(false);
    if (settings.game == GameId::Wings) {
        const auto [backgroundWidth, backgroundHeight] =
            wingsParallaxSize(settings.width, settings.height);
        statusBar()->showMessage(
            settings.backgroundMode == BackgroundMode::Parallax
                ? tr("Created Wings level settings: %1 x %2, parallax %3 x %4")
                      .arg(settings.width)
                      .arg(settings.height)
                      .arg(backgroundWidth)
                      .arg(backgroundHeight)
                : tr("Created Wings level settings: %1 x %2")
                      .arg(settings.width)
                      .arg(settings.height),
            5000);
    } else if (settings.game == GameId::Auts) {
        statusBar()->showMessage(
            tr("Created a new AUTS level: 320 x 400 with a protected border"),
            4000);
    } else {
        statusBar()->showMessage(tr("Created a new V-Wing level"), 3000);
    }
    updateWindowTitle();
    return true;
}

bool MainWindow::promptForWingsSettings(LevelCreationSettings& settings,
                                        const bool allowDimensionEditing)
{
    const GameProfile& profile = gameProfile(GameId::Wings);
    QDialog dialog(this);
    dialog.setWindowTitle(tr("New Wings level"));
    auto* layout = new QVBoxLayout(&dialog);
    auto* form = new QFormLayout();

    auto* nameEdit = new QLineEdit(QString::fromStdString(settings.name),
                                   &dialog);
    nameEdit->setMaxLength(64);
    nameEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("[A-Za-z0-9 _.-]{0,64}")),
        nameEdit));
    form->addRow(tr("Level / file name:"), nameEdit);

    auto* width = new QSpinBox(&dialog);
    width->setRange(profile.minimumWidth, profile.maximumWidth);
    width->setValue(settings.width);
    width->setSuffix(tr(" px"));
    width->setEnabled(allowDimensionEditing);
    form->addRow(tr("Width:"), width);

    auto* height = new QSpinBox(&dialog);
    height->setRange(profile.minimumHeight, profile.maximumHeight);
    height->setValue(settings.height);
    height->setSuffix(tr(" px"));
    height->setEnabled(allowDimensionEditing);
    form->addRow(tr("Height:"), height);

    auto* parallax = new QCheckBox(tr("Use parallax background"), &dialog);
    parallax->setChecked(settings.backgroundMode == BackgroundMode::Parallax);
    form->addRow(QString(), parallax);
    auto* parallaxSize = new QLabel(&dialog);
    parallaxSize->setWordWrap(true);
    form->addRow(tr("Required background size:"), parallaxSize);

    auto* stars = new QCheckBox(tr("Show stars"), &dialog);
    stars->setChecked(settings.stars);
    form->addRow(QString(), stars);

    auto percentageSpin = [&dialog]() {
        auto* spin = new QSpinBox(&dialog);
        spin->setRange(0, 100);
        spin->setSuffix(QStringLiteral(" %"));
        return spin;
    };
    auto* rain = percentageSpin();
    rain->setValue(settings.rainProbability);
    form->addRow(tr("Rain probability:"), rain);
    auto* snow = percentageSpin();
    snow->setValue(settings.snowProbability);
    form->addRow(tr("Snow probability:"), snow);
    auto* bombing = percentageSpin();
    bombing->setValue(settings.bombingProbability);
    form->addRow(tr("Bombing probability:"), bombing);

    auto* civilians = new QSpinBox(&dialog);
    civilians->setRange(0, 1000);
    civilians->setValue(settings.civilians);
    form->addRow(tr("Civilians:"), civilians);
    auto* armed = percentageSpin();
    armed->setValue(settings.armedCiviliansProbability);
    form->addRow(tr("Armed civilians:"), armed);
    layout->addLayout(form);

    auto updateParallaxSize = [=] {
        const auto [requiredWidth, requiredHeight] =
            wingsParallaxSize(width->value(), height->value());
        if (parallax->isChecked()) {
            parallaxSize->setText(
                tr("%1 x %2 px (exact size used by Wings)")
                    .arg(requiredWidth)
                    .arg(requiredHeight));
        } else {
            parallaxSize->setText(tr("Not used"));
        }
        parallaxSize->setEnabled(parallax->isChecked());
    };
    connect(width, &QSpinBox::valueChanged, &dialog,
            [=](int) { updateParallaxSize(); });
    connect(height, &QSpinBox::valueChanged, &dialog,
            [=](int) { updateParallaxSize(); });
    connect(parallax, &QCheckBox::toggled, &dialog,
            [=](bool) { updateParallaxSize(); });
    updateParallaxSize();

    auto* note = new QLabel(
        tr("Wings requires at least 157 x 90 pixels. The 1000 x 1000 "
           "editor limit is provisional because MAKELEV does not document "
           "a maximum size."),
        &dialog);
    note->setWordWrap(true);
    layout->addWidget(note);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);

    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }
    settings.name = nameEdit->text().trimmed().toStdString();
    if (settings.name.empty()) {
        settings.name = "UNTITLED";
    }
    settings.width = width->value();
    settings.height = height->value();
    settings.backgroundMode = parallax->isChecked()
                                  ? BackgroundMode::Parallax
                                  : BackgroundMode::None;
    settings.stars = stars->isChecked();
    settings.rainProbability = rain->value();
    settings.snowProbability = snow->value();
    settings.bombingProbability = bombing->value();
    settings.civilians = civilians->value();
    settings.armedCiviliansProbability = armed->value();
    return true;
}

void MainWindow::editLevelSettings()
{
    if (creationSettings_.game != GameId::Wings) {
        return;
    }
    LevelCreationSettings edited = creationSettings_;
    edited.name = level_->name;
    if (!promptForWingsSettings(edited)) {
        return;
    }
    creationSettings_ = edited;
    level_->name = edited.name;
    configureWingsDocuments();
    {
        const QSignalBlocker blocker(levelNameEdit_);
        levelNameEdit_->setText(QString::fromStdString(level_->name));
    }
    nonUndoModified_ = true;
    setModified(true);
    updateWindowTitle();
}

Level* MainWindow::activeLevel() const
{
    if (creationSettings_.game == GameId::Wings && backgroundLevel_ &&
        documentTabs_ != nullptr && documentTabs_->currentIndex() == 1) {
        return backgroundLevel_.get();
    }
    return level_.get();
}

void MainWindow::configureWingsDocuments()
{
    const bool wings = creationSettings_.game == GameId::Wings;
    documentTabs_->setVisible(wings);
    if (!wings) {
        backgroundLevel_.reset();
        return;
    }

    if (level_->width != static_cast<std::size_t>(creationSettings_.width) ||
        level_->height != static_cast<std::size_t>(creationSettings_.height)) {
        canvas_->forgetLevel(level_.get());
        level_->resizePreservingContent(
            static_cast<std::size_t>(creationSettings_.width),
            static_cast<std::size_t>(creationSettings_.height));
    }

    const bool hasBackground =
        creationSettings_.backgroundMode != BackgroundMode::None;
    documentTabs_->setTabEnabled(1, hasBackground);
    if (!hasBackground) {
        canvas_->forgetLevel(backgroundLevel_.get());
        backgroundLevel_.reset();
        documentTabs_->setCurrentIndex(0);
    } else {
        int width = creationSettings_.width;
        int height = creationSettings_.height;
        if (creationSettings_.backgroundMode == BackgroundMode::Parallax) {
            const auto size = wingsParallaxSize(width, height);
            width = size.first;
            height = size.second;
        }
        if (!backgroundLevel_) {
            backgroundLevel_ = std::make_unique<Level>();
            backgroundLevel_->name = "Background";
        }
        backgroundLevel_->palette = level_->palette;
        if (backgroundLevel_->width != static_cast<std::size_t>(width) ||
            backgroundLevel_->height != static_cast<std::size_t>(height)) {
            canvas_->forgetLevel(backgroundLevel_.get());
            backgroundLevel_->resizePreservingContent(
                static_cast<std::size_t>(width),
                static_cast<std::size_t>(height));
        }
    }
    switchWingsDocument(documentTabs_->currentIndex());
}

void MainWindow::configurePaletteForGame()
{
    if (paletteGroupCombo_ == nullptr || materialIndexSpinBox_ == nullptr ||
        secondaryIndexSpinBox_ == nullptr) {
        return;
    }
    canvas_->setGame(creationSettings_.game);
    const bool hasInternalLevelName =
        creationSettings_.game == GameId::VWing;
    levelNameLabel_->setVisible(hasInternalLevelName);
    levelNameEdit_->setVisible(hasInternalLevelName);
    levelNameSeparatorAction_->setVisible(hasInternalLevelName);
    if (hasInternalLevelName) {
        levelNameEdit_->setMaxLength(20);
        levelNameEdit_->setValidator(new QRegularExpressionValidator(
            QRegularExpression(QStringLiteral("[\\x20-\\x7E]{0,20}")),
            levelNameEdit_));
        levelNameEdit_->setToolTip(
            tr("Maximum 20 printable ASCII characters. Ä, Ö, Å and other "
               "non-ASCII characters are not supported by LEV files."));
    }
    materialIndexSpinBox_->setGame(creationSettings_.game);
    secondaryIndexSpinBox_->setGame(creationSettings_.game);

    const QSignalBlocker blocker(paletteGroupCombo_);
    paletteGroupCombo_->clear();
    const auto add = [this](const char* label, const PaletteGroup group) {
        paletteGroupCombo_->addItem(tr(label), static_cast<int>(group));
    };
    if (creationSettings_.game == GameId::Wings) {
        add("All documented usable", PaletteGroup::AllUsable);
        add("Background (0)", PaletteGroup::Background);
        add("Water, bubbles and snow (16, 48-53)", PaletteGroup::Water);
        add("Bases (32-47)", PaletteGroup::Bases);
        add("Fire and explosives (54-56)", PaletteGroup::Special);
        add("Fly-through background (64-79)", PaletteGroup::FlyThrough);
        add("Indestructible (80-95)", PaletteGroup::Indestructible);
        add("Soft terrain (96-111)", PaletteGroup::Soft);
        add("Burning terrain (112-127)", PaletteGroup::BurningWings);
        add("Normal terrain (128-255)", PaletteGroup::NormalTerrain);
    } else if (creationSettings_.game == GameId::Auts) {
        add("All palette indices (0-255)", PaletteGroup::AllUsable);
        add("Space (0)", PaletteGroup::Background);
        add("Indestructible (7)", PaletteGroup::Indestructible);
        add("Water (39)", PaletteGroup::Water);
        add("Docking plate (92-95)", PaletteGroup::Docking);
        add("Other colors", PaletteGroup::Other);
    } else {
        add("All documented usable", PaletteGroup::AllUsable);
        add("Background (0)", PaletteGroup::Background);
        add("Water (16-19)", PaletteGroup::Water);
        add("Fly through (20-30)", PaletteGroup::FlyThrough);
        add("Font (32-37)", PaletteGroup::Font);
        add("Special materials and game colors (39-56)",
            PaletteGroup::Special);
        add("Normal terrain (57-149)", PaletteGroup::NormalTerrain);
        add("Burnable (150-199)", PaletteGroup::Burnable);
        add("Underwater (201-219)", PaletteGroup::Underwater);
        add("Indestructible (221-243, 248-255)",
            PaletteGroup::Indestructible);
        add("Turret colors (244-247; not placement)",
            PaletteGroup::Turrets);
    }
    paletteWidget_->setIndices(
        paletteIndices(PaletteGroup::AllUsable, creationSettings_.game));
    const int defaultIndex = creationSettings_.game == GameId::Wings
                                 ? 128
                                 : creationSettings_.game == GameId::Auts ? 7
                                                                          : 56;
    const int secondaryIndex = creationSettings_.game == GameId::Auts
                                   ? 39
                                   : defaultIndex + 1;
    materialIndexSpinBox_->setValue(defaultIndex);
    secondaryIndexSpinBox_->setValue(secondaryIndex);
    updateMaterialDetails(defaultIndex);
}

void MainWindow::switchWingsDocument(const int index)
{
    if (creationSettings_.game != GameId::Wings || level_ == nullptr) {
        return;
    }
    canvas_->commitSelection();
    Level* document = index == 1 && backgroundLevel_ ? backgroundLevel_.get()
                                                     : level_.get();
    canvas_->setLevel(document);
    paletteWidget_->setLevel(document);
    configurePaletteForGame();
    refreshLayerList();
    setModified(nonUndoModified_ || canvas_->hasDirtyUndoStack());
    statusBar()->showMessage(
        index == 1
            ? tr("Editing Wings background: %1 x %2")
                  .arg(document->width)
                  .arg(document->height)
            : tr("Editing Wings level: %1 x %2")
                  .arg(document->width)
                  .arg(document->height),
        3000);
}

void MainWindow::updateMaterialDetails(const int index)
{
    const RGB& color = level_->palette[static_cast<std::size_t>(index)];
    const QString hex =
        QStringLiteral("#%1%2%3")
            .arg(static_cast<int>(color.r), 2, 16, QLatin1Char('0'))
            .arg(static_cast<int>(color.g), 2, 16, QLatin1Char('0'))
            .arg(static_cast<int>(color.b), 2, 16, QLatin1Char('0'))
            .toUpper();
    materialDetailsLabel_->setText(tr("Material index %1 / file index %2\n"
                                      "RGB: %3, %4, %5   %6\n%7")
                                       .arg(colorChartNumber(index))
                                       .arg(index)
                                       .arg(static_cast<int>(color.r))
                                       .arg(static_cast<int>(color.g))
                                       .arg(static_cast<int>(color.b))
                                       .arg(hex)
                                       .arg(materialDescription(
                                           creationSettings_.game, index)));
    const bool editable =
        isPaletteColorEditable(creationSettings_.game, index);
    editColorButton_->setEnabled(editable);
    if (editable) {
        editColorButton_->setToolTip(tr("Change this palette color globally"));
    } else if (creationSettings_.game == GameId::Auts) {
        editColorButton_->setToolTip(tr("AUTS uses a fixed game palette"));
    } else {
        editColorButton_->setToolTip(
            tr("This palette color is fixed or reserved by the game"));
    }
}

bool MainWindow::saveProject()
{
    return projectPath_.empty() ? saveProjectAs() : writeProject(projectPath_);
}

bool MainWindow::saveProjectAs()
{
    QString suggested = toQString(projectPath_);
    if (suggested.isEmpty()) {
        QSettings settings;
        const QString directory =
            effectiveProjectDirectorySetting(settings);
        if (QDir(directory).exists() || QDir().mkpath(directory)) {
            QString projectName = QString::fromLatin1(
                creationSettings_.name.data(),
                static_cast<qsizetype>(creationSettings_.name.size()));
            projectName.replace(
                QRegularExpression(QStringLiteral(R"([\\/:*?"<>|])")),
                QStringLiteral("_"));
            if (projectName.isEmpty()) {
                projectName = tr("Untitled");
            }
            suggested = QDir(directory).filePath(
                projectName + QStringLiteral(".pxlp"));
        }
    }
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Save editable PCX Level Tool project"),
        suggested, tr("PCX Level Tool projects (*.pxlp)"));
    if (filename.isEmpty()) {
        return false;
    }
    if (!filename.endsWith(QStringLiteral(".pxlp"), Qt::CaseInsensitive)) {
        filename += QStringLiteral(".pxlp");
    }
    return writeProject(toPath(filename));
}

bool MainWindow::maybeSave()
{
    if (!modified_) {
        return true;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this, tr("Unsaved changes"), tr("Save changes to the editable project?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Save) {
        return saveProject();
    }
    return choice == QMessageBox::Discard;
}

bool MainWindow::writeProject(const std::filesystem::path& path)
{
    canvas_->commitSelection();
    std::string error;
    bool saved = false;
    if (creationSettings_.game == GameId::VWing ||
        creationSettings_.game == GameId::Auts) {
        uppercaseLevelName();
    }
    flattenLayers(*level_);
    if (backgroundLevel_) {
        flattenLayers(*backgroundLevel_);
    }
    saved = savePxlProject(path, *level_, backgroundLevel_.get(),
                           creationSettings_, error);
    if (!saved) {
        QMessageBox::critical(this, tr("Save failed"),
                              QString::fromStdString(error));
        return false;
    }
    projectPath_ = path;
    canvas_->markAllUndoStacksClean();
    nonUndoModified_ = false;
    setModified(false);
    statusBar()->showMessage(tr("Saved %1").arg(toQString(path)), 3000);
    return true;
}

bool MainWindow::publishLevel()
{
    std::filesystem::path suggested = publishPath_;
    if ((creationSettings_.game == GameId::Wings ||
         creationSettings_.game == GameId::Auts) &&
        suggested.empty()) {
        suggested = std::filesystem::path(creationSettings_.name + ".LEV");
    }
    if (suggested.empty() && !projectPath_.empty()) {
        suggested = projectPath_;
        suggested.replace_extension(".LEV");
    }
    if (suggested.empty() && !creationSettings_.name.empty()) {
        suggested = std::filesystem::path(creationSettings_.name + ".LEV");
    }
    QSettings settings;
    const QString configuredDirectory =
        exportDirectorySetting(settings, creationSettings_.game);
    if (!configuredDirectory.isEmpty() &&
        QDir(configuredDirectory).exists()) {
        const std::filesystem::path directory = toPath(configuredDirectory);
        suggested = suggested.empty()
                        ? directory
                        : directory / suggested.filename();
    }
    QString gameName;
    if (creationSettings_.game == GameId::Wings) {
        gameName = QStringLiteral("Wings");
    } else if (creationSettings_.game == GameId::Auts) {
        gameName = QStringLiteral("AUTS");
    } else {
        gameName = QStringLiteral("V-Wing");
    }
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Publish game-compatible %1 level").arg(gameName),
        toQString(suggested), tr("%1 levels (*.LEV)").arg(gameName));
    if (filename.isEmpty()) {
        return false;
    }
    if (!filename.endsWith(QStringLiteral(".lev"), Qt::CaseInsensitive)) {
        filename += QStringLiteral(".LEV");
    }
    return writePublishedLevel(toPath(filename));
}

bool MainWindow::writePublishedLevel(const std::filesystem::path& path)
{
    canvas_->commitSelection();
    if (creationSettings_.game == GameId::Wings) {
        flattenLayers(*level_);
        if (backgroundLevel_) {
            flattenLayers(*backgroundLevel_);
        }
        std::string error;
        if (!saveWingsLev(path, *level_, backgroundLevel_.get(),
                          creationSettings_, error)) {
            QMessageBox::critical(this, tr("Publish failed"),
                                  QString::fromStdString(error));
            return false;
        }
        publishPath_ = path;
        statusBar()->showMessage(tr("Published %1").arg(toQString(path)),
                                 3000);
        return true;
    }
    if (creationSettings_.game == GameId::Auts) {
        uppercaseLevelName();
        flattenLayers(*level_);
        std::string error;
        if (!saveAutsLev(path, *level_, error)) {
            QMessageBox::critical(this, tr("Publish failed"),
                                  QString::fromStdString(error));
            return false;
        }
        publishPath_ = path;
        statusBar()->showMessage(tr("Published %1").arg(toQString(path)),
                                 3000);
        return true;
    }
    uppercaseLevelName();
    canvas_->refreshImage();
    std::string error;
    if (!saveLev(path, *level_, error)) {
        QMessageBox::critical(this, tr("Publish failed"),
                              QString::fromStdString(error));
        return false;
    }
    publishPath_ = path;
    statusBar()->showMessage(tr("Published %1").arg(toQString(path)), 3000);
    return true;
}

void MainWindow::uppercaseLevelName()
{
    if (creationSettings_.game == GameId::Auts) {
        level_->name.erase(
            std::remove_if(level_->name.begin(), level_->name.end(),
                           [](const char character) {
                               const unsigned char value =
                                   static_cast<unsigned char>(character);
                               return !(std::isalnum(value) || character == '_' ||
                                        character == '-');
                           }),
            level_->name.end());
        if (level_->name.size() > 8) {
            level_->name.resize(8);
        }
        if (level_->name.empty()) {
            level_->name = "UNTITLED";
        }
    } else if (creationSettings_.game == GameId::VWing &&
               level_->name.size() > 20) {
        level_->name.resize(20);
    }
    std::transform(level_->name.begin(), level_->name.end(),
                   level_->name.begin(), [](const char character) {
                       return character >= 'a' && character <= 'z'
                                  ? static_cast<char>(character - 'a' + 'A')
                                  : character;
                   });
    creationSettings_.name = level_->name;
    const QSignalBlocker blocker(levelNameEdit_);
    levelNameEdit_->setText(
        QString::fromLatin1(level_->name.data(),
                            static_cast<int>(level_->name.size())));
    updateWindowTitle();
}

void MainWindow::editSelectedPaletteColor()
{
    const int index = paletteIndexFromColorChart(
        materialIndexSpinBox_->value());
    if (!isPaletteColorEditable(creationSettings_.game, index)) {
        return;
    }
    const RGB& current = level_->palette[static_cast<std::size_t>(index)];
    const QColor selected =
        QColorDialog::getColor(QColor(current.r, current.g, current.b), this,
                               tr("Material index %1 (file index %2)")
                                   .arg(colorChartNumber(index))
                                   .arg(index));
    if (!selected.isValid()) {
        return;
    }
    canvas_->setPaletteColor(static_cast<std::uint8_t>(index),
                             RGB{static_cast<std::uint8_t>(selected.red()),
                                 static_cast<std::uint8_t>(selected.green()),
                                 static_cast<std::uint8_t>(selected.blue())});
}

void MainWindow::loadPalette()
{
    if (creationSettings_.game == GameId::Auts) {
        QMessageBox::information(
            this, tr("Fixed AUTS palette"),
            tr("AUTS uses the fixed palette supplied with its original "
               "converter. Custom palettes cannot be loaded for AUTS levels."));
        return;
    }
    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Load palette"), QString(), tr("JASC palettes (*.pal)"));
    if (filename.isEmpty()) {
        return;
    }

    std::array<RGB, 256> palette{};
    std::string error;
    if (!loadJascPalette(toPath(filename), palette, error)) {
        QMessageBox::critical(this, tr("Palette load failed"),
                              QString::fromStdString(error));
        return;
    }
    if (creationSettings_.game == GameId::Wings) {
        const auto fixed = defaultWingsPalette();
        std::copy_n(fixed.begin(), 48, palette.begin());
        const auto& current = activeLevel()->palette;
        for (int index = 48; index < 256; ++index) {
            if (isReservedPaletteIndex(GameId::Wings, index)) {
                palette[static_cast<std::size_t>(index)] =
                    current[static_cast<std::size_t>(index)];
            }
        }
    }
    canvas_->setPalette(palette);
    statusBar()->showMessage(tr("Loaded palette %1").arg(filename), 3000);
}

void MainWindow::savePalette()
{
    const QString filename = QFileDialog::getSaveFileName(
        this, tr("Save palette"), QString(), tr("JASC palettes (*.pal)"));
    if (filename.isEmpty()) {
        return;
    }

    std::string error;
    if (!saveJascPalette(toPath(filename), level_->palette, error)) {
        QMessageBox::critical(this, tr("Palette save failed"),
                              QString::fromStdString(error));
        return;
    }
    statusBar()->showMessage(tr("Saved palette %1").arg(filename), 3000);
}

void MainWindow::setModified(const bool modified)
{
    modified_ = modified;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    const QString filename = projectPath_.empty()
                                 ? (publishPath_.empty()
                                        ? tr("Untitled")
                                        : toQString(publishPath_.filename()))
                                 : toQString(projectPath_.filename());
    const QString levelName =
        QString::fromLatin1(level_->name.data(),
                            static_cast<int>(level_->name.size()));
    const QString document = levelName.isEmpty()
                                 ? filename
                                 : tr("%1 [%2]").arg(filename, levelName);
    const GameProfile& profile = gameProfile(creationSettings_.game);
    const QString game = QString::fromUtf8(profile.displayName.data(),
                                           profile.displayName.size());
    setWindowTitle(tr("%1%2 — %3 — PCX Level Tool")
                       .arg(modified_ ? QStringLiteral("*") : QString(),
                            document, game));
}
