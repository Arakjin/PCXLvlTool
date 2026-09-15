#include "main_window.h"

#include "lev_reader.h"
#include "lev_writer.h"
#include "level_canvas.h"

#include <QAction>
#include <QCloseEvent>
#include <QComboBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QLabel>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QSpinBox>
#include <QStatusBar>
#include <QToolBar>
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
    createDrawToolBar();
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
    connect(saveAsAction, &QAction::triggered, this,
            [this] { saveLevelAs(); });

    fileMenu->addSeparator();
    QAction* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &QWidget::close);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    QAction* undoAction = canvas_->undoStack()->createUndoAction(this, tr("&Undo"));
    undoAction->setShortcut(QKeySequence::Undo);
    editMenu->addAction(undoAction);
    QAction* redoAction = canvas_->undoStack()->createRedoAction(this, tr("&Redo"));
    redoAction->setShortcut(QKeySequence::Redo);
    editMenu->addAction(redoAction);
}

void MainWindow::createMaterialDock()
{
    auto* dock = new QDockWidget(tr("Materials"), this);
    auto* contents = new QWidget(dock);
    auto* layout = new QVBoxLayout(contents);
    layout->addWidget(new QLabel(tr("Palette index"), contents));

    auto* indexSpinBox = new QSpinBox(contents);
    indexSpinBox->setRange(0, 255);
    indexSpinBox->setValue(57);
    layout->addWidget(indexSpinBox);
    layout->addStretch();
    connect(indexSpinBox, &QSpinBox::valueChanged, this,
            [this](const int value) {
                canvas_->setSelectedIndex(static_cast<std::uint8_t>(value));
            });
    connect(canvas_, &LevelCanvas::selectedIndexChanged, indexSpinBox,
            &QSpinBox::setValue);

    dock->setWidget(contents);
    addDockWidget(Qt::LeftDockWidgetArea, dock);
}

void MainWindow::createDrawToolBar()
{
    QToolBar* toolBar = addToolBar(tr("Tools"));
    toolBar->addWidget(new QLabel(tr("Tool: "), toolBar));
    auto* toolCombo = new QComboBox(toolBar);
    const std::array<std::pair<const char*, DrawTool>, 7> tools{{
        {"Pencil", DrawTool::Pencil},
        {"Eraser", DrawTool::Eraser},
        {"Line", DrawTool::Line},
        {"Rectangle", DrawTool::Rectangle},
        {"Filled rectangle", DrawTool::FilledRectangle},
        {"Flood fill", DrawTool::FloodFill},
        {"Eyedropper", DrawTool::Eyedropper},
    }};
    for (const auto& [label, tool] : tools) {
        toolCombo->addItem(tr(label), static_cast<int>(tool));
    }
    connect(toolCombo, &QComboBox::currentIndexChanged, this,
            [this, toolCombo](const int index) {
                canvas_->setDrawTool(static_cast<DrawTool>(
                    toolCombo->itemData(index).toInt()));
            });
    toolBar->addWidget(toolCombo);
}

void MainWindow::createZoomToolBar()
{
    QToolBar* toolBar = addToolBar(tr("View"));
    toolBar->addWidget(new QLabel(tr("Zoom: "), toolBar));
    auto* zoomCombo = new QComboBox(toolBar);
    const std::array<std::pair<const char*, double>, 6> zoomLevels{{
        {"25%", 0.25}, {"50%", 0.5}, {"100%", 1.0},
        {"200%", 2.0}, {"400%", 4.0}, {"800%", 8.0},
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

    const QString filename = QFileDialog::getOpenFileName(
        this, tr("Open V-Wing level"), QString(), tr("V-Wing levels (*.LEV *.lev)"));
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
    canvas_->undoStack()->setClean();
    setModified(false);
    statusBar()->showMessage(tr("Opened %1").arg(filename), 3000);
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
