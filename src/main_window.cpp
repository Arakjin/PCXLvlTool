#include "main_window.h"

#include "default_palette.h"
#include "lev_reader.h"
#include "lev_writer.h"
#include "level_canvas.h"
#include "palette_io.h"
#include "palette_rules.h"
#include "palette_widget.h"
#include "project_io.h"

#include <QAction>
#include <QAbstractItemView>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
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
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QValidator>
#include <QWidget>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

class PaletteIndexSpinBox final : public QSpinBox {
public:
    using QSpinBox::QSpinBox;

protected:
    void stepBy(const int steps) override
    {
        const int direction = steps < 0 ? -1 : 1;
        int candidate = value();
        for (int step = 0; step < std::abs(steps); ++step) {
            do {
                candidate += direction;
            } while (candidate >= minimum() && candidate <= maximum() &&
                     isReservedColorChartNumber(candidate));
            candidate = std::clamp(candidate, minimum(), maximum());
        }
        setValue(candidate);
    }

    QValidator::State validate(QString& input, int& position) const override
    {
        const QValidator::State state = QSpinBox::validate(input, position);
        if (state == QValidator::Acceptable &&
            isReservedColorChartNumber(input.toInt())) {
            return QValidator::Intermediate;
        }
        return state;
    }

    void fixup(QString& input) const override
    {
        bool valid = false;
        int value = input.toInt(&valid);
        if (valid && isReservedColorChartNumber(value)) {
            while (value <= maximum() &&
                   isReservedColorChartNumber(value)) {
                ++value;
            }
            input = QString::number(std::min(value, maximum()));
            return;
        }
        QSpinBox::fixup(input);
    }
};

std::filesystem::path toPath(const QString& path)
{
    return std::filesystem::path(path.toStdU16String());
}

QString toQString(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

QString materialDescription(const int paletteIndex)
{
    const int index = colorChartNumber(paletteIndex);
    if (isReservedPaletteIndex(paletteIndex)) {
        return QStringLiteral("Reserved (do not use)");
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
    if (index >= 248 && index <= 256) {
        return QStringLiteral("Indestructible");
    }

    switch (index) {
    case 1:
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
        return QStringLiteral("Ice (not in water)");
    case 45:
        return QStringLiteral("Plastic explosive");
    case 46:
        return QStringLiteral("Birds");
    case 48:
        return QStringLiteral("Blood");
    case 49:
        return QStringLiteral("Clay");
    case 50:
        return QStringLiteral("Base");
    case 51:
        return QStringLiteral("Ash");
    case 52:
        return QStringLiteral("Snow");
    case 56:
        return QStringLiteral("Bubbles");
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
        return QStringLiteral("Ice");
    case 244:
        return QStringLiteral("Turret muzzle (firing)");
    case 245:
        return QStringLiteral("Turret barrel and body");
    case 246:
        return QStringLiteral("Turret muzzle");
    case 247:
        return QStringLiteral("Turret body (sides)");
    default:
        return QStringLiteral("Unknown / undocumented");
    }
}

enum class PaletteGroup {
    AllUsable,
    Background,
    Water,
    FlyThrough,
    Font,
    Special,
    NormalTerrain,
    Burnable,
    Underwater,
    Indestructible,
    Turrets,
};

void appendRange(std::vector<std::uint8_t>& indices, const int first,
                 const int last)
{
    for (int index = first; index <= last; ++index) {
        indices.push_back(static_cast<std::uint8_t>(index));
    }
}

std::vector<std::uint8_t> paletteIndices(const PaletteGroup group)
{
    std::vector<std::uint8_t> indices;
    indices.reserve(256);
    switch (group) {
    case PaletteGroup::AllUsable:
        indices.push_back(0);
        appendRange(indices, 15, 29);
        appendRange(indices, 31, 36);
        appendRange(indices, 38, 45);
        appendRange(indices, 47, 51);
        appendRange(indices, 55, 173);
        appendRange(indices, 175, 198);
        appendRange(indices, 200, 218);
        appendRange(indices, 220, 255);
        break;
    case PaletteGroup::Background:
        indices.push_back(0);
        break;
    case PaletteGroup::Water:
        appendRange(indices, 15, 18);
        break;
    case PaletteGroup::FlyThrough:
        appendRange(indices, 19, 29);
        break;
    case PaletteGroup::Font:
        appendRange(indices, 31, 36);
        break;
    case PaletteGroup::Special:
        appendRange(indices, 38, 45);
        appendRange(indices, 47, 51);
        indices.push_back(55);
        break;
    case PaletteGroup::NormalTerrain:
        appendRange(indices, 56, 148);
        break;
    case PaletteGroup::Burnable:
        appendRange(indices, 149, 173);
        appendRange(indices, 175, 198);
        break;
    case PaletteGroup::Underwater:
        appendRange(indices, 200, 218);
        break;
    case PaletteGroup::Indestructible:
        appendRange(indices, 220, 242);
        appendRange(indices, 247, 255);
        break;
    case PaletteGroup::Turrets:
        appendRange(indices, 243, 246);
        break;
    }
    return indices;
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

    canvas_ = new LevelCanvas(this);
    canvas_->setLevel(level_.get());
    setCentralWidget(canvas_);

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
    connect(canvas_->undoStack(), &QUndoStack::cleanChanged, this,
            [this](const bool clean) {
                setModified(nonUndoModified_ || !clean ||
                            canvas_->hasPendingSelectionEdit());
            });
    connect(canvas_, &LevelCanvas::pendingSelectionEditChanged, this,
            [this](const bool pending) {
                setModified(nonUndoModified_ || pending ||
                            !canvas_->undoStack()->isClean());
            });
    connect(canvas_, &LevelCanvas::layersChanged, this,
            &MainWindow::refreshLayerList);

    resize(1000, 800);
    updateWindowTitle();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeSave()) {
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::createActions()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    QAction* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::openLevel);

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
        } else if (tool == DrawTool::Eraser) {
            button->setToolTip(tr("Eraser: makes upper layers transparent; "
                                  "writes Color Chart 1 (file index 0) on "
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
         shapeModeCombo, fontCombo, textSizeSpinBox, tools](const int id) {
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
    auto* paletteGroupCombo = new QComboBox(contents);
    const std::array<std::pair<const char*, PaletteGroup>, 11> paletteGroups{{
        {"All documented usable", PaletteGroup::AllUsable},
        {"Background (1)", PaletteGroup::Background},
        {"Water (16-19)", PaletteGroup::Water},
        {"Fly through (20-30)", PaletteGroup::FlyThrough},
        {"Font (32-37)", PaletteGroup::Font},
        {"Special materials (39-56)", PaletteGroup::Special},
        {"Normal terrain (57-149)", PaletteGroup::NormalTerrain},
        {"Burnable (150-199)", PaletteGroup::Burnable},
        {"Underwater (201-219)", PaletteGroup::Underwater},
        {"Indestructible (221-243, 248-256)", PaletteGroup::Indestructible},
        {"Turrets (244-247)", PaletteGroup::Turrets},
    }};
    for (const auto& [label, group] : paletteGroups) {
        paletteGroupCombo->addItem(tr(label), static_cast<int>(group));
    }
    layout->addWidget(paletteGroupCombo);
    paletteWidget_ = new PaletteWidget(contents);
    paletteWidget_->setLevel(level_.get());
    paletteWidget_->setIndices(paletteIndices(PaletteGroup::AllUsable));
    layout->addWidget(paletteWidget_);

    auto* indexLayout = new QHBoxLayout();
    indexLayout->addWidget(new QLabel(tr("Left Color Chart:"), contents));
    materialIndexSpinBox_ = new PaletteIndexSpinBox(contents);
    materialIndexSpinBox_->setRange(1, 256);
    materialIndexSpinBox_->setValue(57);
    materialIndexSpinBox_->setToolTip(
        tr("Reserved Color Chart indices cannot be selected"));
    indexLayout->addWidget(materialIndexSpinBox_);
    layout->addLayout(indexLayout);
    auto* secondaryIndexLayout = new QHBoxLayout();
    secondaryIndexLayout->addWidget(
        new QLabel(tr("Right Color Chart:"), contents));
    secondaryIndexSpinBox_ = new PaletteIndexSpinBox(contents);
    secondaryIndexSpinBox_->setRange(1, 256);
    secondaryIndexSpinBox_->setValue(58);
    secondaryIndexSpinBox_->setToolTip(
        tr("Right-click drawing and filled-shape interior Color Chart number"));
    secondaryIndexLayout->addWidget(secondaryIndexSpinBox_);
    layout->addLayout(secondaryIndexLayout);
    materialDetailsLabel_ = new QLabel(contents);
    layout->addWidget(materialDetailsLabel_);
    auto* editColorButton =
        new QPushButton(tr("Edit selected color..."), contents);
    layout->addWidget(editColorButton);
    auto* paletteFileLayout = new QHBoxLayout();
    auto* loadPaletteButton = new QPushButton(tr("Load palette..."), contents);
    auto* savePaletteButton = new QPushButton(tr("Save palette..."), contents);
    paletteFileLayout->addWidget(loadPaletteButton);
    paletteFileLayout->addWidget(savePaletteButton);
    layout->addLayout(paletteFileLayout);
    layout->addStretch();
    connect(paletteGroupCombo, &QComboBox::currentIndexChanged, this,
            [this, paletteGroupCombo](const int index) {
                auto indices = paletteIndices(static_cast<PaletteGroup>(
                    paletteGroupCombo->itemData(index).toInt()));
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
    connect(editColorButton, &QPushButton::clicked, this,
            &MainWindow::editSelectedPaletteColor);
    connect(loadPaletteButton, &QPushButton::clicked, this,
            &MainWindow::loadPalette);
    connect(savePaletteButton, &QPushButton::clicked, this,
            &MainWindow::savePalette);
    connect(canvas_, &LevelCanvas::paletteColorChanged, this,
            [this](const int index) {
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
                if (index > 0 &&
                    level_->layers[static_cast<std::size_t>(index)].visible !=
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
            level_->layers[static_cast<std::size_t>(index)].name);
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
        const bool locked =
            level_->layers[static_cast<std::size_t>(index)].locked;
        canvas_->setLayerLocked(index, !locked);
        nonUndoModified_ = true;
        setModified(true);
    });
    refreshLayerList();
}

void MainWindow::refreshLayerList()
{
    if (layerListWidget_ == nullptr || level_ == nullptr) {
        return;
    }
    const QSignalBlocker blocker(layerListWidget_);
    layerListWidget_->clear();
    for (std::size_t index = level_->layers.size(); index-- > 0;) {
        const Level::Layer& layer = level_->layers[index];
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
        if (index == level_->activeLayer) {
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
        this, tr("Open project or V-Wing level"), QString(),
        tr("V-Wing projects and levels (*.vwp *.VWP *.lev *.LEV);;"
           "V-Wing projects (*.vwp *.VWP);;V-Wing levels (*.lev *.LEV)"));
    if (filename.isEmpty()) {
        return;
    }

    auto loaded = std::make_unique<Level>();
    std::string error;
    const std::filesystem::path path = toPath(filename);
    const bool isProject =
        QString::compare(QFileInfo(filename).suffix(), QStringLiteral("vwp"),
                         Qt::CaseInsensitive) == 0;
    const bool loadedSuccessfully = isProject ? loadProject(path, *loaded, error)
                                              : loadLev(path, *loaded, error);
    if (!loadedSuccessfully) {
        QMessageBox::critical(this, tr("Open failed"),
                              QString::fromStdString(error));
        return;
    }

    // Drop commands while their old Level target is still alive.
    canvas_->undoStack()->clear();
    level_ = std::move(loaded);
    projectPath_ = isProject ? path : std::filesystem::path{};
    publishPath_ = isProject ? std::filesystem::path{} : path;
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    updateMaterialDetails(
        paletteIndexFromColorChart(materialIndexSpinBox_->value()));
    refreshLayerList();
    canvas_->undoStack()->setClean();
    nonUndoModified_ = false;
    setModified(false);
    statusBar()->showMessage(tr("Opened %1").arg(filename), 3000);
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
    materialDetailsLabel_->setText(tr("Color Chart %1 / file index %2\n"
                                      "RGB: %3, %4, %5   %6\n%7")
                                       .arg(colorChartNumber(index))
                                       .arg(index)
                                       .arg(static_cast<int>(color.r))
                                       .arg(static_cast<int>(color.g))
                                       .arg(static_cast<int>(color.b))
                                       .arg(hex)
                                       .arg(materialDescription(index)));
}

bool MainWindow::saveProject()
{
    return projectPath_.empty() ? saveProjectAs() : writeProject(projectPath_);
}

bool MainWindow::saveProjectAs()
{
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Save editable V-Wing project"), toQString(projectPath_),
        tr("V-Wing projects (*.vwp)"));
    if (filename.isEmpty()) {
        return false;
    }
    if (!filename.endsWith(QStringLiteral(".vwp"), Qt::CaseInsensitive)) {
        filename += QStringLiteral(".vwp");
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
    if (!::saveProject(path, *level_, error)) {
        QMessageBox::critical(this, tr("Save failed"),
                              QString::fromStdString(error));
        return false;
    }
    projectPath_ = path;
    canvas_->undoStack()->setClean();
    nonUndoModified_ = false;
    setModified(false);
    statusBar()->showMessage(tr("Saved %1").arg(toQString(path)), 3000);
    return true;
}

bool MainWindow::publishLevel()
{
    std::filesystem::path suggested = publishPath_;
    if (suggested.empty() && !projectPath_.empty()) {
        suggested = projectPath_;
        suggested.replace_extension(".LEV");
    }
    QString filename = QFileDialog::getSaveFileName(
        this, tr("Publish game-compatible V-Wing level"), toQString(suggested),
        tr("V-Wing levels (*.LEV)"));
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

void MainWindow::editSelectedPaletteColor()
{
    const int index = paletteIndexFromColorChart(
        materialIndexSpinBox_->value());
    const RGB& current = level_->palette[static_cast<std::size_t>(index)];
    const QColor selected =
        QColorDialog::getColor(QColor(current.r, current.g, current.b), this,
                               tr("Color Chart %1 (file index %2)")
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
    const QString name = projectPath_.empty()
                             ? (publishPath_.empty()
                                    ? tr("Untitled")
                                    : toQString(publishPath_.filename()))
                             : toQString(projectPath_.filename());
    setWindowTitle(tr("%1%2 — V-Wing Level Editor")
                       .arg(modified_ ? QStringLiteral("*") : QString(), name));
}
