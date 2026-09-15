#include "main_window.h"

#include "lev_reader.h"
#include "lev_writer.h"
#include "level_canvas.h"
#include "palette_widget.h"

#include <QAction>
#include <QButtonGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
#include <QPolygon>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <cstdint>
#include <string>
#include <utility>

namespace {

std::filesystem::path toPath(const QString& path)
{
    return std::filesystem::path(path.toStdU16String());
}

QString toQString(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

QString materialDescription(const int index)
{
    if ((index >= 2 && index <= 15) || index == 31 || index == 38 ||
        index == 47) {
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
    if (index >= 248 && index <= 255) {
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

QIcon toolIcon(const DrawTool tool)
{
    QPixmap pixmap(28, 28);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(pixmap.rect().adjusted(1, 1, -1, -1),
                     QColor(245, 245, 245));
    painter.setPen(QPen(QColor(150, 150, 150), 1));
    painter.drawRect(pixmap.rect().adjusted(1, 1, -2, -2));
    painter.setPen(QPen(Qt::black, 2));
    painter.setBrush(Qt::NoBrush);

    switch (tool) {
    case DrawTool::Pencil:
        painter.drawLine(6, 22, 21, 7);
        painter.drawLine(8, 24, 23, 9);
        painter.drawLine(6, 22, 8, 24);
        break;
    case DrawTool::Eraser: {
        QPolygon polygon;
        polygon << QPoint(5, 18) << QPoint(14, 7) << QPoint(23, 14)
                << QPoint(14, 23);
        painter.setBrush(QColor(245, 170, 180));
        painter.drawPolygon(polygon);
        break;
    }
    case DrawTool::Line:
        painter.drawLine(5, 23, 23, 5);
        break;
    case DrawTool::Rectangle:
        painter.drawRect(5, 6, 18, 16);
        break;
    case DrawTool::FilledRectangle:
        painter.setBrush(QColor(80, 80, 80));
        painter.drawRect(5, 6, 18, 16);
        break;
    case DrawTool::FloodFill: {
        QPolygon bucket;
        bucket << QPoint(6, 9) << QPoint(16, 7) << QPoint(21, 17)
               << QPoint(11, 21);
        painter.drawPolygon(bucket);
        painter.setBrush(QColor(70, 130, 220));
        painter.drawEllipse(20, 20, 4, 4);
        break;
    }
    case DrawTool::Eyedropper:
        painter.drawLine(7, 22, 21, 8);
        painter.drawEllipse(18, 5, 6, 6);
        painter.drawLine(5, 24, 9, 20);
        break;
    case DrawTool::Ellipse:
        painter.drawEllipse(4, 6, 20, 16);
        break;
    case DrawTool::FilledEllipse:
        painter.setBrush(QColor(80, 80, 80));
        painter.drawEllipse(4, 6, 20, 16);
        break;
    }
    return QIcon(pixmap);
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), level_(std::make_unique<Level>())
{
    level_->name = "UNTITLED";
    for (std::size_t index = 0; index < level_->palette.size(); ++index) {
        const auto value = static_cast<std::uint8_t>(index);
        level_->palette[index] = RGB{value, value, value};
    }

    canvas_ = new LevelCanvas(this);
    canvas_->setLevel(level_.get());
    setCentralWidget(canvas_);

    createActions();
    createMaterialDock();
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
            [this](const bool clean) { setModified(!clean); });

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

    saveAction_ = fileMenu->addAction(tr("&Save"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveLevel(); });

    QAction* saveAsAction = fileMenu->addAction(tr("Save &As..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this] { saveLevelAs(); });

    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction =
        canvas_->undoStack()->createUndoAction(this, tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAction);
    QAction* redoAction =
        canvas_->undoStack()->createRedoAction(this, tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(redoAction);
}

void MainWindow::createMaterialDock()
{
    auto* dock = new QDockWidget(tr("Toolbox"), this);
    auto* contents = new QWidget(dock);
    auto* layout = new QVBoxLayout(contents);
    layout->addWidget(new QLabel(tr("Tools"), contents));

    auto* toolGrid = new QWidget(contents);
    auto* grid = new QGridLayout(toolGrid);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(2);
    auto* toolGroup = new QButtonGroup(toolGrid);
    toolGroup->setExclusive(true);
    const std::array<std::pair<const char*, DrawTool>, 9> tools{{
        {"Pencil", DrawTool::Pencil},
        {"Eraser (index 0)", DrawTool::Eraser},
        {"Line", DrawTool::Line},
        {"Rectangle", DrawTool::Rectangle},
        {"Filled rectangle", DrawTool::FilledRectangle},
        {"Flood fill", DrawTool::FloodFill},
        {"Eyedropper", DrawTool::Eyedropper},
        {"Ellipse", DrawTool::Ellipse},
        {"Filled ellipse", DrawTool::FilledEllipse},
    }};
    for (std::size_t index = 0; index < tools.size(); ++index) {
        const auto& [label, tool] = tools[index];
        auto* button = new QToolButton(toolGrid);
        button->setCheckable(true);
        button->setIcon(toolIcon(tool));
        button->setIconSize(QSize(28, 28));
        button->setFixedSize(42, 42);
        button->setToolTip(tr(label));
        button->setAccessibleName(tr(label));
        toolGroup->addButton(button, static_cast<int>(tool));
        grid->addWidget(button, static_cast<int>(index / 2),
                        static_cast<int>(index % 2));
        if (tool == DrawTool::Pencil) {
            button->setChecked(true);
        }
    }
    grid->setColumnStretch(2, 1);
    layout->addWidget(toolGrid);
    auto* activeToolLabel = new QLabel(tr("Pencil"), contents);
    layout->addWidget(activeToolLabel);
    connect(toolGroup, &QButtonGroup::idClicked, this,
            [this, activeToolLabel, tools](const int id) {
                canvas_->setDrawTool(static_cast<DrawTool>(id));
                for (const auto& [label, tool] : tools) {
                    if (static_cast<int>(tool) == id) {
                        activeToolLabel->setText(tr(label));
                        break;
                    }
                }
            });

    layout->addSpacing(6);
    layout->addWidget(new QLabel(tr("Palette"), contents));
    paletteWidget_ = new PaletteWidget(contents);
    paletteWidget_->setLevel(level_.get());
    layout->addWidget(paletteWidget_);

    auto* indexLayout = new QHBoxLayout();
    indexLayout->addWidget(new QLabel(tr("Index:"), contents));
    materialIndexSpinBox_ = new QSpinBox(contents);
    materialIndexSpinBox_->setRange(0, 255);
    materialIndexSpinBox_->setValue(57);
    indexLayout->addWidget(materialIndexSpinBox_);
    layout->addLayout(indexLayout);
    materialDetailsLabel_ = new QLabel(contents);
    layout->addWidget(materialDetailsLabel_);
    layout->addStretch();
    connect(materialIndexSpinBox_, &QSpinBox::valueChanged, this,
            [this](const int value) {
                canvas_->setSelectedIndex(static_cast<std::uint8_t>(value));
                paletteWidget_->setSelectedIndex(
                    static_cast<std::uint8_t>(value));
                updateMaterialDetails(value);
            });
    connect(canvas_, &LevelCanvas::selectedIndexChanged, materialIndexSpinBox_,
            &QSpinBox::setValue);
    connect(paletteWidget_, &PaletteWidget::indexSelected,
            materialIndexSpinBox_, &QSpinBox::setValue);
    updateMaterialDetails(materialIndexSpinBox_->value());

    dock->setWidget(contents);
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
    toolBar->addWidget(zoomCombo);
}

void MainWindow::openLevel()
{
    if (!maybeSave()) {
        return;
    }

    const QString filename =
        QFileDialog::getOpenFileName(this, tr("Open V-Wing level"), QString(),
                                     tr("V-Wing levels (*.LEV *.lev)"));
    if (filename.isEmpty()) {
        return;
    }

    auto loaded = std::make_unique<Level>();
    std::string error;
    const std::filesystem::path path = toPath(filename);
    if (!loadLev(path, *loaded, error)) {
        QMessageBox::critical(this, tr("Open failed"),
                              QString::fromStdString(error));
        return;
    }

    // Drop commands while their old Level target is still alive.
    canvas_->undoStack()->clear();
    level_ = std::move(loaded);
    currentPath_ = path;
    canvas_->setLevel(level_.get());
    paletteWidget_->setLevel(level_.get());
    updateMaterialDetails(materialIndexSpinBox_->value());
    canvas_->undoStack()->setClean();
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
    materialDetailsLabel_->setText(tr("RGB: %1, %2, %3   %4\n%5")
                                       .arg(static_cast<int>(color.r))
                                       .arg(static_cast<int>(color.g))
                                       .arg(static_cast<int>(color.b))
                                       .arg(hex)
                                       .arg(materialDescription(index)));
}

bool MainWindow::saveLevel()
{
    return currentPath_.empty() ? saveLevelAs() : writeLevel(currentPath_);
}

bool MainWindow::saveLevelAs()
{
    const QString filename = QFileDialog::getSaveFileName(
        this, tr("Save V-Wing level"), toQString(currentPath_),
        tr("V-Wing levels (*.LEV)"));
    if (filename.isEmpty()) {
        return false;
    }
    return writeLevel(toPath(filename));
}

bool MainWindow::maybeSave()
{
    if (!modified_) {
        return true;
    }

    const QMessageBox::StandardButton choice = QMessageBox::warning(
        this, tr("Unsaved changes"), tr("Save changes to the current level?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
        QMessageBox::Save);
    if (choice == QMessageBox::Save) {
        return saveLevel();
    }
    return choice == QMessageBox::Discard;
}

bool MainWindow::writeLevel(const std::filesystem::path& path)
{
    std::string error;
    if (!saveLev(path, *level_, error)) {
        QMessageBox::critical(this, tr("Save failed"),
                              QString::fromStdString(error));
        return false;
    }
    currentPath_ = path;
    canvas_->undoStack()->setClean();
    setModified(false);
    statusBar()->showMessage(tr("Saved %1").arg(toQString(path)), 3000);
    return true;
}

void MainWindow::setModified(const bool modified)
{
    modified_ = modified;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    const QString name = currentPath_.empty()
                             ? tr("Untitled")
                             : toQString(currentPath_.filename());
    setWindowTitle(tr("%1%2 — V-Wing Level Editor")
                       .arg(modified_ ? QStringLiteral("*") : QString(), name));
}
