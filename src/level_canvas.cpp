#include "level_canvas.h"

#include "palette_rules.h"

#include <QApplication>
#include <QClipboard>
#include <QDataStream>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QScrollBar>
#include <QUndoCommand>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr auto kSelectionMimeType = "application/x-vwing-level-selection";

class PixelEditCommand final : public QUndoCommand {
public:
    PixelEditCommand(Level* level, std::vector<PixelChange> changes,
                     LevelCanvas* canvas, QString commandText)
        : level_(level), changes_(std::move(changes)), canvas_(canvas)
    {
        setText(commandText);
    }

    void undo() override
    {
        for (const PixelChange& change : changes_) {
            level_->pixels[change.offset] = change.oldValue;
        }
        canvas_->refreshImage();
    }

    void redo() override
    {
        for (const PixelChange& change : changes_) {
            level_->pixels[change.offset] = change.newValue;
        }
        canvas_->refreshImage();
    }

private:
    Level* level_;
    std::vector<PixelChange> changes_;
    LevelCanvas* canvas_;
};

class PaletteEditCommand final : public QUndoCommand {
public:
    PaletteEditCommand(LevelCanvas* canvas, const std::uint8_t index,
                       const RGB oldColor, const RGB newColor)
        : canvas_(canvas), index_(index), oldColor_(oldColor),
          newColor_(newColor)
    {
        setText(QObject::tr("Change palette color"));
    }

    void undo() override { canvas_->applyPaletteColor(index_, oldColor_); }
    void redo() override { canvas_->applyPaletteColor(index_, newColor_); }

private:
    LevelCanvas* canvas_;
    std::uint8_t index_;
    RGB oldColor_;
    RGB newColor_;
};

class PaletteReplaceCommand final : public QUndoCommand {
public:
    PaletteReplaceCommand(LevelCanvas* canvas, std::array<RGB, 256> oldPalette,
                          std::array<RGB, 256> newPalette)
        : canvas_(canvas), oldPalette_(std::move(oldPalette)),
          newPalette_(std::move(newPalette))
    {
        setText(QObject::tr("Load palette"));
    }

    void undo() override { canvas_->applyPalette(oldPalette_); }
    void redo() override { canvas_->applyPalette(newPalette_); }

private:
    LevelCanvas* canvas_;
    std::array<RGB, 256> oldPalette_;
    std::array<RGB, 256> newPalette_;
};

} // namespace

LevelCanvas::LevelCanvas(QWidget* parent) : QAbstractScrollArea(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setBackgroundRole(QPalette::Dark);
    viewport()->setCursor(Qt::CrossCursor);
}

void LevelCanvas::setLevel(Level* level)
{
    clearSelection();
    curveStage_ = CurveStage::None;
    level_ = level;
    drawing_ = false;
    undoStack_.clear();
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
    updateScrollBars();
    viewport()->update();
}

void LevelCanvas::setZoom(const double zoom)
{
    const double newZoom = std::clamp(zoom, 0.25, 8.0);
    if (std::abs(newZoom - zoom_) < 0.0001) {
        return;
    }

    const double centerX =
        (horizontalScrollBar()->value() + viewport()->width() / 2.0) / zoom_;
    const double centerY =
        (verticalScrollBar()->value() + viewport()->height() / 2.0) / zoom_;
    zoom_ = newZoom;
    updateScrollBars();
    horizontalScrollBar()->setValue(static_cast<int>(
        std::round(centerX * zoom_ - viewport()->width() / 2.0)));
    verticalScrollBar()->setValue(static_cast<int>(
        std::round(centerY * zoom_ - viewport()->height() / 2.0)));
    viewport()->update();
}

void LevelCanvas::setSelectedIndex(const std::uint8_t index)
{
    if (selectedIndex_ == index || isReservedPaletteIndex(index)) {
        return;
    }
    selectedIndex_ = index;
    emit selectedIndexChanged(index);
}

void LevelCanvas::setDrawTool(const DrawTool tool)
{
    if (drawTool_ == DrawTool::BezierCurve && tool != DrawTool::BezierCurve) {
        cancelCurve();
    }
    drawTool_ = tool;
    updateToolCursor();
}

void LevelCanvas::setToolThickness(const DrawTool tool, const int thickness)
{
    const int value = std::clamp(thickness, 1, 32);
    if (tool == DrawTool::Pencil) {
        pencilThickness_ = value;
    } else if (tool == DrawTool::Eraser) {
        eraserThickness_ = value;
    } else if (tool == DrawTool::Line) {
        lineThickness_ = value;
    } else if (tool == DrawTool::Rectangle) {
        rectangleThickness_ = value;
    } else if (tool == DrawTool::Ellipse) {
        ellipseThickness_ = value;
    } else if (tool == DrawTool::Spray) {
        sprayThickness_ = value;
    } else if (tool == DrawTool::BezierCurve) {
        curveThickness_ = value;
    }
}

int LevelCanvas::toolThickness(const DrawTool tool) const
{
    if (tool == DrawTool::Pencil) {
        return pencilThickness_;
    }
    if (tool == DrawTool::Eraser) {
        return eraserThickness_;
    }
    if (tool == DrawTool::Line) {
        return lineThickness_;
    }
    if (tool == DrawTool::Rectangle) {
        return rectangleThickness_;
    }
    if (tool == DrawTool::Ellipse) {
        return ellipseThickness_;
    }
    if (tool == DrawTool::Spray) {
        return sprayThickness_;
    }
    if (tool == DrawTool::BezierCurve) {
        return curveThickness_;
    }
    return 1;
}

void LevelCanvas::setRectangleCornerRadius(const int radius)
{
    rectangleCornerRadius_ = std::clamp(radius, 0, 32);
}

int LevelCanvas::rectangleCornerRadius() const
{
    return rectangleCornerRadius_;
}

void LevelCanvas::setPaletteColor(const std::uint8_t index, const RGB color)
{
    if (level_ == nullptr || level_->palette[index] == color) {
        return;
    }
    undoStack_.push(
        new PaletteEditCommand(this, index, level_->palette[index], color));
}

void LevelCanvas::applyPaletteColor(const std::uint8_t index, const RGB color)
{
    if (level_ == nullptr) {
        return;
    }
    level_->palette[index] = color;
    viewport()->update();
    emit paletteColorChanged(index);
}

void LevelCanvas::setPalette(const std::array<RGB, 256>& palette)
{
    if (level_ == nullptr || level_->palette == palette) {
        return;
    }
    undoStack_.push(new PaletteReplaceCommand(this, level_->palette, palette));
}

void LevelCanvas::applyPalette(const std::array<RGB, 256>& palette)
{
    if (level_ == nullptr) {
        return;
    }
    level_->palette = palette;
    viewport()->update();
    emit paletteColorChanged(-1);
}

void LevelCanvas::commitSelection()
{
    if (curveStage_ != CurveStage::None) {
        commitCurve();
    }
    if (!selectionActive_ || level_ == nullptr) {
        return;
    }

    std::unordered_map<std::size_t, std::uint8_t> finalValues;
    const int width = selectionBounds_.width();
    const int height = selectionBounds_.height();
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t localOffset =
                static_cast<std::size_t>(y) * width + x;
            if (selectionMask_[localOffset] == 0) {
                continue;
            }
            if (selectionHasSource_) {
                const QPoint source = selectionSourcePosition_ + QPoint(x, y);
                finalValues[static_cast<std::size_t>(source.y()) *
                                Level::Width +
                            source.x()] = 0;
            }
            const QPoint destination = selectionPosition_ + QPoint(x, y);
            finalValues[static_cast<std::size_t>(destination.y()) *
                            Level::Width +
                        destination.x()] = selectionPixels_[localOffset];
        }
    }

    std::vector<PixelChange> changes;
    changes.reserve(finalValues.size());
    for (const auto& [offset, value] : finalValues) {
        if (level_->pixels[offset] != value) {
            changes.push_back({offset, level_->pixels[offset], value});
        }
    }
    std::sort(changes.begin(), changes.end(),
              [](const PixelChange& left, const PixelChange& right) {
                  return left.offset < right.offset;
              });
    clearSelection();
    if (!changes.empty()) {
        undoStack_.push(new PixelEditCommand(level_, std::move(changes), this,
                                             tr("Commit selection")));
    }
}

void LevelCanvas::deleteSelection()
{
    cancelCurve();
    if (!selectionActive_ || level_ == nullptr) {
        return;
    }

    std::vector<PixelChange> changes;
    if (selectionHasSource_) {
        const int width = selectionBounds_.width();
        const int height = selectionBounds_.height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t localOffset =
                    static_cast<std::size_t>(y) * width + x;
                if (selectionMask_[localOffset] == 0) {
                    continue;
                }
                const QPoint source = selectionSourcePosition_ + QPoint(x, y);
                const std::size_t offset =
                    static_cast<std::size_t>(source.y()) * Level::Width +
                    source.x();
                if (level_->pixels[offset] != 0) {
                    changes.push_back({offset, level_->pixels[offset], 0});
                }
            }
        }
    }
    clearSelection();
    if (!changes.empty()) {
        undoStack_.push(new PixelEditCommand(level_, std::move(changes), this,
                                             tr("Delete selection")));
    }
}

void LevelCanvas::copySelection()
{
    if (!selectionActive_) {
        return;
    }
    QByteArray payload;
    QDataStream stream(&payload, QIODevice::WriteOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    const quint32 width = static_cast<quint32>(selectionBounds_.width());
    const quint32 height = static_cast<quint32>(selectionBounds_.height());
    stream << width << height << static_cast<qint32>(selectionPosition_.x())
           << static_cast<qint32>(selectionPosition_.y());
    for (std::size_t index = 0; index < selectionMask_.size(); ++index) {
        stream << static_cast<quint8>(selectionMask_[index])
               << static_cast<quint8>(selectionPixels_[index]);
    }
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(kSelectionMimeType), payload);
    QApplication::clipboard()->setMimeData(mimeData);
}

void LevelCanvas::pasteSelection()
{
    if (level_ == nullptr) {
        return;
    }
    const QMimeData* mimeData = QApplication::clipboard()->mimeData();
    if (!mimeData->hasFormat(QString::fromLatin1(kSelectionMimeType))) {
        return;
    }
    QByteArray payload =
        mimeData->data(QString::fromLatin1(kSelectionMimeType));
    QDataStream stream(&payload, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    quint32 width = 0;
    quint32 height = 0;
    qint32 sourceX = 0;
    qint32 sourceY = 0;
    stream >> width >> height >> sourceX >> sourceY;
    if (stream.status() != QDataStream::Ok || width == 0 || height == 0 ||
        width > Level::Width || height > Level::Height ||
        static_cast<std::size_t>(width) * height > Level::PixelCount) {
        return;
    }
    const std::size_t size = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> mask(size);
    std::vector<std::uint8_t> pixels(size);
    bool containsPixels = false;
    for (std::size_t index = 0; index < size; ++index) {
        quint8 maskValue = 0;
        quint8 pixelValue = 0;
        stream >> maskValue >> pixelValue;
        mask[index] = maskValue == 0 ? 0 : 1;
        pixels[index] = pixelValue;
        containsPixels |= mask[index] != 0;
    }
    if (stream.status() != QDataStream::Ok || !containsPixels) {
        return;
    }

    commitSelection();
    selectionBounds_ =
        QRect(0, 0, static_cast<int>(width), static_cast<int>(height));
    selectionMask_ = std::move(mask);
    selectionPixels_ = std::move(pixels);
    selectionHasSource_ = false;
    selectionActive_ = true;
    selectionEditPending_ = true;
    emit pendingSelectionEditChanged(true);
    const qint64 maximumX = static_cast<qint64>(Level::Width) - width;
    const qint64 maximumY = static_cast<qint64>(Level::Height) - height;
    selectionPosition_ = {
        static_cast<int>(
            std::clamp(static_cast<qint64>(sourceX) + 1, qint64{0}, maximumX)),
        static_cast<int>(
            std::clamp(static_cast<qint64>(sourceY) + 1, qint64{0}, maximumY)),
    };
    viewport()->update();
}

void LevelCanvas::selectAll()
{
    if (level_ == nullptr) {
        return;
    }
    commitSelection();
    selectionBounds_ = QRect(0, 0, static_cast<int>(Level::Width),
                             static_cast<int>(Level::Height));
    selectionSourcePosition_ = {0, 0};
    selectionPosition_ = {0, 0};
    selectionMask_.assign(Level::PixelCount, 1);
    selectionPixels_.assign(level_->pixels.begin(), level_->pixels.end());
    selectionHasSource_ = true;
    selectionActive_ = true;
    selectionEditPending_ = false;
    viewport()->update();
}

bool LevelCanvas::hasSelection() const { return selectionActive_; }

bool LevelCanvas::hasPendingSelectionEdit() const
{
    return selectionEditPending_;
}

QUndoStack* LevelCanvas::undoStack() { return &undoStack_; }

void LevelCanvas::refreshImage() { viewport()->update(); }

void LevelCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Dark));

    if (level_ == nullptr) {
        painter.setPen(palette().color(QPalette::BrightText));
        painter.drawText(viewport()->rect(), Qt::AlignCenter,
                         tr("No level loaded"));
        return;
    }

    std::vector<std::uint8_t> composedPixels;
    const std::uint8_t* imagePixels = level_->pixels.data();
    if (selectionActive_) {
        composedPixels.assign(level_->pixels.begin(), level_->pixels.end());
        const int width = selectionBounds_.width();
        const int height = selectionBounds_.height();
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const std::size_t localOffset =
                    static_cast<std::size_t>(y) * width + x;
                if (selectionMask_[localOffset] == 0) {
                    continue;
                }
                if (selectionHasSource_) {
                    const QPoint source =
                        selectionSourcePosition_ + QPoint(x, y);
                    const std::size_t sourceOffset =
                        static_cast<std::size_t>(source.y()) * Level::Width +
                        source.x();
                    composedPixels[sourceOffset] = 0;
                }
                const QPoint destination = selectionPosition_ + QPoint(x, y);
                const std::size_t destinationOffset =
                    static_cast<std::size_t>(destination.y()) * Level::Width +
                    destination.x();
                composedPixels[destinationOffset] =
                    selectionPixels_[localOffset];
            }
        }
        imagePixels = composedPixels.data();
    }

    QImage image(imagePixels, static_cast<int>(Level::Width),
                 static_cast<int>(Level::Height),
                 static_cast<int>(Level::Width), QImage::Format_Indexed8);
    QList<QRgb> colors;
    colors.reserve(static_cast<qsizetype>(level_->palette.size()));
    for (const RGB& color : level_->palette) {
        colors.append(qRgb(color.r, color.g, color.b));
    }
    image.setColorTable(colors);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.translate(-horizontalScrollBar()->value(),
                      -verticalScrollBar()->value());
    painter.drawImage(QRectF(0, 0, Level::Width * zoom_, Level::Height * zoom_),
                      image);
    paintShapePreview(painter);
    paintSelection(painter);
}

void LevelCanvas::resizeEvent(QResizeEvent* event)
{
    QAbstractScrollArea::resizeEvent(event);
    updateScrollBars();
}

void LevelCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        panning_ = true;
        lastPanPoint_ = event->position().toPoint();
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    if (event->button() == Qt::LeftButton && level_ != nullptr) {
        setFocus(Qt::MouseFocusReason);
        const QPoint point = imagePoint(event->position());
        if (point.x() >= 0) {
            if (drawTool_ == DrawTool::MoveSelection) {
                if (selectionContains(point.x(), point.y())) {
                    movingSelection_ = true;
                    selectionMoveAnchor_ = point;
                    selectionMoveStart_ = selectionPosition_;
                    viewport()->setCursor(Qt::ClosedHandCursor);
                } else if (selectionActive_) {
                    commitSelection();
                }
                event->accept();
                return;
            }

            if (isSelectionTool(drawTool_) && selectionActive_) {
                commitSelection();
            }
            if (drawTool_ == DrawTool::BezierCurve &&
                curveStage_ != CurveStage::None) {
                drawing_ = true;
                lastImagePoint_ = point;
                if (curveStage_ == CurveStage::FirstControl) {
                    curveControl1_ = point;
                } else if (curveStage_ == CurveStage::SecondControl) {
                    curveControl2_ = point;
                }
                reportPosition(point);
                viewport()->update();
                event->accept();
                return;
            }
            lastImagePoint_ = point;
            strokeStartPoint_ = point;
            strokeTool_ = drawTool_;
            strokePaintIndex_ = paintIndex();
            strokeThickness_ = toolThickness(strokeTool_);
            strokeCornerRadius_ = rectangleCornerRadius_;

            if (strokeTool_ == DrawTool::Eyedropper) {
                setSelectedIndex(pixelAt(point.x(), point.y()));
            } else if (strokeTool_ == DrawTool::FloodFill) {
                beginStroke(tr("Flood fill"));
                floodFill(point, strokePaintIndex_);
                commitStroke();
            } else if (isSelectionTool(strokeTool_)) {
                drawing_ = true;
                freehandSelectionPoints_.clear();
                if (strokeTool_ == DrawTool::SelectFreehand) {
                    freehandSelectionPoints_.push_back(point);
                }
            } else {
                drawing_ = true;
                const bool freehand = strokeTool_ == DrawTool::Pencil ||
                                      strokeTool_ == DrawTool::Eraser ||
                                      strokeTool_ == DrawTool::Spray;
                beginStroke(commandText());
                if (strokeTool_ == DrawTool::BezierCurve) {
                    curveStage_ = CurveStage::Baseline;
                    curveStartPoint_ = point;
                    curveEndPoint_ = point;
                    curveControl1_ = point;
                    curveControl2_ = point;
                    viewport()->update();
                } else if (strokeTool_ == DrawTool::Spray) {
                    sprayDistanceRemainder_ = 0.0;
                    sprayAt(point, strokePaintIndex_, strokeThickness_);
                    viewport()->update();
                } else if (freehand) {
                    setBrushPixel(point.x(), point.y(), strokePaintIndex_,
                                  strokeThickness_);
                    viewport()->update();
                }
            }
            reportPosition(point);
        }
        event->accept();
        return;
    }

    QAbstractScrollArea::mousePressEvent(event);
}

void LevelCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (panning_) {
        const QPoint current = event->position().toPoint();
        const QPoint delta = current - lastPanPoint_;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                        delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastPanPoint_ = current;
        event->accept();
        return;
    }

    const QPoint point = imagePoint(event->position());
    if (point.x() >= 0) {
        reportPosition(point);
        if (movingSelection_ && (event->buttons() & Qt::LeftButton)) {
            setSelectionPosition(selectionMoveStart_ +
                                 (point - selectionMoveAnchor_));
            viewport()->update();
            return;
        }
        if (drawing_ && (event->buttons() & Qt::LeftButton) &&
            strokeTool_ == DrawTool::BezierCurve) {
            lastImagePoint_ = point;
            if (curveStage_ == CurveStage::Baseline) {
                curveEndPoint_ = point;
                const QPoint delta = curveEndPoint_ - curveStartPoint_;
                curveControl1_ = curveStartPoint_ + delta / 3;
                curveControl2_ = curveStartPoint_ + delta * 2 / 3;
            } else if (curveStage_ == CurveStage::FirstControl) {
                curveControl1_ = point;
            } else if (curveStage_ == CurveStage::SecondControl) {
                curveControl2_ = point;
            }
            viewport()->update();
            return;
        }
        if (drawing_ && (event->buttons() & Qt::LeftButton) &&
            (strokeTool_ == DrawTool::Pencil ||
             strokeTool_ == DrawTool::Eraser)) {
            drawLine(lastImagePoint_, point, strokePaintIndex_,
                     strokeThickness_);
            lastImagePoint_ = point;
        } else if (drawing_ && (event->buttons() & Qt::LeftButton) &&
                   strokeTool_ == DrawTool::Spray) {
            sprayLine(lastImagePoint_, point, strokePaintIndex_,
                      strokeThickness_);
            lastImagePoint_ = point;
        } else if (drawing_ && (event->buttons() & Qt::LeftButton) &&
                   strokeTool_ == DrawTool::SelectFreehand) {
            if (freehandSelectionPoints_.empty() ||
                freehandSelectionPoints_.back() != point) {
                freehandSelectionPoints_.push_back(point);
            }
            lastImagePoint_ = point;
            viewport()->update();
        } else if (drawing_ && (event->buttons() & Qt::LeftButton)) {
            lastImagePoint_ = event->modifiers() & Qt::ShiftModifier
                                  ? constrainedShapePoint(point)
                                  : point;
            viewport()->update();
        }
    } else {
        emit cursorLeftCanvas();
    }
}

void LevelCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton && panning_) {
        panning_ = false;
        updateToolCursor();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        if (movingSelection_) {
            movingSelection_ = false;
            updateToolCursor();
            viewport()->update();
            event->accept();
            return;
        }
        const bool wasDrawing = drawing_;
        drawing_ = false;
        if (wasDrawing) {
            const QPoint releasePoint = imagePoint(event->position());
            if (releasePoint.x() >= 0) {
                lastImagePoint_ = event->modifiers() & Qt::ShiftModifier
                                      ? constrainedShapePoint(releasePoint)
                                      : releasePoint;
            }
            if (strokeTool_ == DrawTool::BezierCurve) {
                if (curveStage_ == CurveStage::Baseline) {
                    curveEndPoint_ = lastImagePoint_;
                    const QPoint delta = curveEndPoint_ - curveStartPoint_;
                    curveControl1_ = curveStartPoint_ + delta / 3;
                    curveControl2_ = curveStartPoint_ + delta * 2 / 3;
                    curveStage_ = CurveStage::FirstControl;
                } else if (curveStage_ == CurveStage::FirstControl) {
                    curveControl1_ = lastImagePoint_;
                    curveStage_ = CurveStage::SecondControl;
                } else if (curveStage_ == CurveStage::SecondControl) {
                    curveControl2_ = lastImagePoint_;
                    commitCurve();
                }
                viewport()->update();
                event->accept();
                return;
            }
            if (strokeTool_ == DrawTool::Line) {
                drawLine(strokeStartPoint_, lastImagePoint_, strokePaintIndex_,
                         strokeThickness_);
            } else if (strokeTool_ == DrawTool::Rectangle) {
                drawRectangle(strokeStartPoint_, lastImagePoint_,
                              strokePaintIndex_, strokeThickness_,
                              strokeCornerRadius_, false);
            } else if (strokeTool_ == DrawTool::FilledRectangle) {
                drawRectangle(strokeStartPoint_, lastImagePoint_,
                              strokePaintIndex_, 1, strokeCornerRadius_, true);
            } else if (strokeTool_ == DrawTool::Ellipse) {
                drawEllipse(strokeStartPoint_, lastImagePoint_,
                            strokePaintIndex_, strokeThickness_, false);
            } else if (strokeTool_ == DrawTool::FilledEllipse) {
                drawEllipse(strokeStartPoint_, lastImagePoint_,
                            strokePaintIndex_, 1, true);
            } else if (isSelectionTool(strokeTool_)) {
                if (strokeTool_ == DrawTool::SelectFreehand &&
                    (freehandSelectionPoints_.empty() ||
                     freehandSelectionPoints_.back() != lastImagePoint_)) {
                    freehandSelectionPoints_.push_back(lastImagePoint_);
                }
                createSelection();
            }
            commitStroke();
        }
        event->accept();
        return;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void LevelCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::SelectAll)) {
        selectAll();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Copy)) {
        copySelection();
        event->accept();
        return;
    }
    if (event->matches(QKeySequence::Paste)) {
        pasteSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Delete) {
        deleteSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (curveStage_ != CurveStage::None) {
            commitCurve();
        }
        commitSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape &&
        (selectionActive_ || curveStage_ != CurveStage::None)) {
        cancelCurve();
        if (selectionActive_) {
            clearSelection();
        }
        event->accept();
        return;
    }
    QAbstractScrollArea::keyPressEvent(event);
}

void LevelCanvas::leaveEvent(QEvent* event)
{
    emit cursorLeftCanvas();
    QAbstractScrollArea::leaveEvent(event);
}

QPoint LevelCanvas::imagePoint(const QPointF& viewportPoint) const
{
    if (level_ == nullptr) {
        return {-1, -1};
    }
    const int x = static_cast<int>(
        (viewportPoint.x() + horizontalScrollBar()->value()) / zoom_);
    const int y = static_cast<int>(
        (viewportPoint.y() + verticalScrollBar()->value()) / zoom_);
    if (x < 0 || y < 0 || x >= static_cast<int>(Level::Width) ||
        y >= static_cast<int>(Level::Height)) {
        return {-1, -1};
    }
    return {x, y};
}

bool LevelCanvas::setPixel(const int x, const int y, const std::uint8_t index)
{
    if (selectionActive_) {
        if (!selectionContains(x, y)) {
            return false;
        }
        const int localX = x - selectionPosition_.x();
        const int localY = y - selectionPosition_.y();
        const std::size_t localOffset =
            static_cast<std::size_t>(localY) * selectionBounds_.width() +
            localX;
        if (selectionPixels_[localOffset] == index) {
            return false;
        }
        selectionPixels_[localOffset] = index;
        if (!selectionEditPending_) {
            selectionEditPending_ = true;
            emit pendingSelectionEditChanged(true);
        }
        return true;
    }

    const std::size_t offset = static_cast<std::size_t>(y) * Level::Width +
                               static_cast<std::size_t>(x);
    if (level_->pixels[offset] == index) {
        return false;
    }
    const auto existing = strokeChangeIndices_.find(offset);
    if (existing == strokeChangeIndices_.end()) {
        strokeChangeIndices_.emplace(offset, strokeChanges_.size());
        strokeChanges_.push_back(PixelChange{
            offset,
            level_->pixels[offset],
            index,
        });
    } else {
        strokeChanges_[existing->second].newValue = index;
    }
    level_->pixels[offset] = index;
    return true;
}

bool LevelCanvas::setBrushPixel(const int x, const int y,
                                const std::uint8_t index, const int thickness)
{
    const int lower = (thickness - 1) / 2;
    const int upper = thickness / 2;
    bool changed = false;
    for (int brushY = y - lower; brushY <= y + upper; ++brushY) {
        for (int brushX = x - lower; brushX <= x + upper; ++brushX) {
            if (brushX >= 0 && brushY >= 0 &&
                brushX < static_cast<int>(Level::Width) &&
                brushY < static_cast<int>(Level::Height)) {
                changed |= setPixel(brushX, brushY, index);
            }
        }
    }
    return changed;
}

std::uint8_t LevelCanvas::pixelAt(const int x, const int y) const
{
    if (selectionContains(x, y)) {
        const int localX = x - selectionPosition_.x();
        const int localY = y - selectionPosition_.y();
        const std::size_t localOffset =
            static_cast<std::size_t>(localY) * selectionBounds_.width() +
            localX;
        return selectionPixels_[localOffset];
    }
    if (selectionActive_ && selectionHasSource_) {
        const int localX = x - selectionSourcePosition_.x();
        const int localY = y - selectionSourcePosition_.y();
        if (localX >= 0 && localY >= 0 && localX < selectionBounds_.width() &&
            localY < selectionBounds_.height() &&
            selectionMask_[static_cast<std::size_t>(localY) *
                               selectionBounds_.width() +
                           localX] != 0) {
            return 0;
        }
    }
    return level_->pixels[static_cast<std::size_t>(y) * Level::Width + x];
}

bool LevelCanvas::selectionContains(const int x, const int y) const
{
    if (!selectionActive_) {
        return false;
    }
    const int localX = x - selectionPosition_.x();
    const int localY = y - selectionPosition_.y();
    if (localX < 0 || localY < 0 || localX >= selectionBounds_.width() ||
        localY >= selectionBounds_.height()) {
        return false;
    }
    return selectionMask_[static_cast<std::size_t>(localY) *
                              selectionBounds_.width() +
                          localX] != 0;
}

void LevelCanvas::sprayLine(const QPoint& from, const QPoint& to,
                            const std::uint8_t index, const int radius)
{
    const double deltaX = to.x() - from.x();
    const double deltaY = to.y() - from.y();
    const double distance = std::sqrt(deltaX * deltaX + deltaY * deltaY);
    if (distance <= 0.0) {
        return;
    }

    const double spacing = std::max(2.0, radius * 0.75);
    double nextDistance = spacing - sprayDistanceRemainder_;
    while (nextDistance <= distance) {
        const double position = nextDistance / distance;
        sprayAt({static_cast<int>(std::round(from.x() + deltaX * position)),
                 static_cast<int>(std::round(from.y() + deltaY * position))},
                index, radius);
        nextDistance += spacing;
    }
    sprayDistanceRemainder_ =
        std::fmod(sprayDistanceRemainder_ + distance, spacing);
    viewport()->update();
}

void LevelCanvas::sprayAt(const QPoint& point, const std::uint8_t index,
                          const int radius)
{
    const int diameter = std::max(1, radius);
    const int lower = diameter / 2;
    const int samples = std::max(4, diameter * 2);
    setPixel(point.x(), point.y(), index);
    for (int sample = 0; sample < samples; ++sample) {
        const int x =
            point.x() + QRandomGenerator::global()->bounded(diameter) - lower;
        const int y =
            point.y() + QRandomGenerator::global()->bounded(diameter) - lower;
        const int deltaX = x - point.x();
        const int deltaY = y - point.y();
        if (deltaX * deltaX + deltaY * deltaY <= lower * lower && x >= 0 &&
            y >= 0 && x < static_cast<int>(Level::Width) &&
            y < static_cast<int>(Level::Height)) {
            setPixel(x, y, index);
        }
    }
}

void LevelCanvas::drawLine(const QPoint& from, const QPoint& to,
                           const std::uint8_t index, const int thickness)
{
    int x0 = from.x();
    int y0 = from.y();
    const int x1 = to.x();
    const int y1 = to.y();
    const int dx = std::abs(x1 - x0);
    const int sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0);
    const int sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    bool changed = false;

    while (true) {
        changed |= setBrushPixel(x0, y0, index, thickness);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        const int twiceError = 2 * error;
        if (twiceError >= dy) {
            error += dy;
            x0 += sx;
        }
        if (twiceError <= dx) {
            error += dx;
            y0 += sy;
        }
    }

    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::drawBezier(const QPoint& start, const QPoint& control1,
                             const QPoint& control2, const QPoint& end,
                             const std::uint8_t index, const int thickness)
{
    const auto distance = [](const QPoint& first, const QPoint& second) {
        const double x = second.x() - first.x();
        const double y = second.y() - first.y();
        return std::sqrt(x * x + y * y);
    };
    const double controlLength = distance(start, control1) +
                                 distance(control1, control2) +
                                 distance(control2, end);
    const int steps =
        std::clamp(static_cast<int>(std::ceil(controlLength * 2)), 1, 4096);
    QPoint previous = start;
    for (int step = 1; step <= steps; ++step) {
        const double t = static_cast<double>(step) / steps;
        const double inverse = 1.0 - t;
        const double startWeight = inverse * inverse * inverse;
        const double control1Weight = 3.0 * inverse * inverse * t;
        const double control2Weight = 3.0 * inverse * t * t;
        const double endWeight = t * t * t;
        const QPoint current{
            static_cast<int>(std::round(
                startWeight * start.x() + control1Weight * control1.x() +
                control2Weight * control2.x() + endWeight * end.x())),
            static_cast<int>(std::round(
                startWeight * start.y() + control1Weight * control1.y() +
                control2Weight * control2.y() + endWeight * end.y())),
        };
        if (current != previous) {
            drawLine(previous, current, index, thickness);
            previous = current;
        }
    }
    if (previous != end) {
        drawLine(previous, end, index, thickness);
    }
}

void LevelCanvas::drawRectangle(const QPoint& from, const QPoint& to,
                                const std::uint8_t index, const int thickness,
                                const int cornerRadius, const bool filled)
{
    const int left = std::min(from.x(), to.x());
    const int right = std::max(from.x(), to.x());
    const int top = std::min(from.y(), to.y());
    const int bottom = std::max(from.y(), to.y());
    const int radius = std::min(
        cornerRadius, std::min(right - left + 1, bottom - top + 1) / 2);
    const auto insideRounded = [](const int x, const int y, const int areaLeft,
                                  const int areaTop, const int areaRight,
                                  const int areaBottom, const int areaRadius) {
        if (x < areaLeft || x > areaRight || y < areaTop || y > areaBottom) {
            return false;
        }
        if (areaRadius <= 0) {
            return true;
        }
        const double pixelX = x + 0.5;
        const double pixelY = y + 0.5;
        const double nearestX =
            std::clamp(pixelX, areaLeft + static_cast<double>(areaRadius),
                       areaRight + 1.0 - areaRadius);
        const double nearestY =
            std::clamp(pixelY, areaTop + static_cast<double>(areaRadius),
                       areaBottom + 1.0 - areaRadius);
        const double deltaX = pixelX - nearestX;
        const double deltaY = pixelY - nearestY;
        return deltaX * deltaX + deltaY * deltaY <= areaRadius * areaRadius;
    };

    const int innerLeft = left + thickness;
    const int innerRight = right - thickness;
    const int innerTop = top + thickness;
    const int innerBottom = bottom - thickness;
    const int innerRadius = std::max(0, radius - thickness);
    const bool hasInterior = innerLeft <= innerRight && innerTop <= innerBottom;
    bool changed = false;

    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const bool inside =
                insideRounded(x, y, left, top, right, bottom, radius);
            const bool insideInterior =
                hasInterior &&
                insideRounded(x, y, innerLeft, innerTop, innerRight,
                              innerBottom, innerRadius);
            if (inside && (filled || !insideInterior)) {
                changed |= setPixel(x, y, index);
            }
        }
    }
    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::drawEllipse(const QPoint& from, const QPoint& to,
                              const std::uint8_t index, const int thickness,
                              const bool filled)
{
    const int left = std::min(from.x(), to.x());
    const int right = std::max(from.x(), to.x());
    const int top = std::min(from.y(), to.y());
    const int bottom = std::max(from.y(), to.y());
    const int width = right - left + 1;
    const int height = bottom - top + 1;

    if (width == 1 || height == 1) {
        drawLine({left, top}, {right, bottom}, index, thickness);
        return;
    }

    const double radiusX = width / 2.0;
    const double radiusY = height / 2.0;
    const double centerX = left + radiusX;
    const double centerY = top + radiusY;
    const auto inside = [=](const int x, const int y) {
        const double normalizedX = (x + 0.5 - centerX) / radiusX;
        const double normalizedY = (y + 0.5 - centerY) / radiusY;
        return normalizedX * normalizedX + normalizedY * normalizedY <= 1.0;
    };
    const double innerRadiusX = radiusX - thickness;
    const double innerRadiusY = radiusY - thickness;
    const auto insideInterior = [=](const int x, const int y) {
        if (innerRadiusX <= 0.0 || innerRadiusY <= 0.0) {
            return false;
        }
        const double normalizedX = (x + 0.5 - centerX) / innerRadiusX;
        const double normalizedY = (y + 0.5 - centerY) / innerRadiusY;
        return normalizedX * normalizedX + normalizedY * normalizedY <= 1.0;
    };

    bool changed = false;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (!inside(x, y)) {
                continue;
            }
            if (filled || !insideInterior(x, y)) {
                changed |= setPixel(x, y, index);
            }
        }
    }
    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::paintShapePreview(QPainter& painter) const
{
    if (level_ == nullptr) {
        return;
    }
    if (curveStage_ != CurveStage::None) {
        const RGB& rgb = level_->palette[strokePaintIndex_];
        const QColor color(rgb.r, rgb.g, rgb.b);
        const auto canvasPoint = [this](const QPoint& point) {
            return QPointF((point.x() + 0.5) * zoom_,
                           (point.y() + 0.5) * zoom_);
        };
        QPainterPath curve;
        curve.moveTo(canvasPoint(curveStartPoint_));
        curve.cubicTo(canvasPoint(curveControl1_), canvasPoint(curveControl2_),
                      canvasPoint(curveEndPoint_));

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(
            QPen(color, std::max<qreal>(1.0, zoom_ * strokeThickness_)));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(curve);

        QPen guidePen(palette().color(QPalette::BrightText), 1, Qt::DashLine);
        guidePen.setCosmetic(true);
        painter.setPen(guidePen);
        painter.drawLine(canvasPoint(curveStartPoint_),
                         canvasPoint(curveControl1_));
        painter.drawLine(canvasPoint(curveControl2_),
                         canvasPoint(curveEndPoint_));
        const qreal handleRadius = 3.0;
        painter.drawEllipse(canvasPoint(curveControl1_), handleRadius,
                            handleRadius);
        painter.drawEllipse(canvasPoint(curveControl2_), handleRadius,
                            handleRadius);
        painter.restore();
        return;
    }
    if (!drawing_ ||
        (strokeTool_ != DrawTool::Line && strokeTool_ != DrawTool::Rectangle &&
         strokeTool_ != DrawTool::FilledRectangle &&
         strokeTool_ != DrawTool::Ellipse &&
         strokeTool_ != DrawTool::FilledEllipse &&
         !isSelectionTool(strokeTool_))) {
        return;
    }

    const RGB& rgb = level_->palette[strokePaintIndex_];
    const QColor color(rgb.r, rgb.g, rgb.b);
    const qreal left =
        std::min(strokeStartPoint_.x(), lastImagePoint_.x()) * zoom_;
    const qreal top =
        std::min(strokeStartPoint_.y(), lastImagePoint_.y()) * zoom_;
    const qreal width =
        (std::abs(lastImagePoint_.x() - strokeStartPoint_.x()) + 1) * zoom_;
    const qreal height =
        (std::abs(lastImagePoint_.y() - strokeStartPoint_.y()) + 1) * zoom_;
    const QRectF bounds(left, top, width, height);

    if (isSelectionTool(strokeTool_)) {
        painter.save();
        QPen selectionPen(palette().color(QPalette::BrightText), 1,
                          Qt::DashLine);
        selectionPen.setCosmetic(true);
        painter.setPen(selectionPen);
        painter.setBrush(Qt::NoBrush);
        if (strokeTool_ == DrawTool::SelectEllipse) {
            painter.drawEllipse(bounds);
        } else if (strokeTool_ == DrawTool::SelectFreehand) {
            QPolygonF outline;
            outline.reserve(
                static_cast<qsizetype>(freehandSelectionPoints_.size()));
            for (const QPoint& point : freehandSelectionPoints_) {
                outline.append(QPointF((point.x() + 0.5) * zoom_,
                                       (point.y() + 0.5) * zoom_));
            }
            painter.drawPolyline(outline);
        } else {
            painter.drawRect(bounds);
        }
        painter.restore();
        return;
    }

    const bool outlinedShape = strokeTool_ == DrawTool::Line ||
                               strokeTool_ == DrawTool::Rectangle ||
                               strokeTool_ == DrawTool::Ellipse;
    const int previewThickness = outlinedShape ? strokeThickness_ : 1;
    const qreal strokeWidth = std::max<qreal>(1.0, zoom_ * previewThickness);
    const qreal cornerRadius = strokeCornerRadius_ * zoom_;

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(QPen(color, strokeWidth));
    painter.setBrush(Qt::NoBrush);

    if (strokeTool_ == DrawTool::Line) {
        painter.drawLine(QPointF((strokeStartPoint_.x() + 0.5) * zoom_,
                                 (strokeStartPoint_.y() + 0.5) * zoom_),
                         QPointF((lastImagePoint_.x() + 0.5) * zoom_,
                                 (lastImagePoint_.y() + 0.5) * zoom_));
    } else if (strokeTool_ == DrawTool::Rectangle) {
        painter.drawRoundedRect(bounds, cornerRadius, cornerRadius);
    } else if (strokeTool_ == DrawTool::FilledRectangle) {
        painter.setBrush(color);
        painter.drawRoundedRect(bounds, cornerRadius, cornerRadius);
    } else if (strokeTool_ == DrawTool::Ellipse) {
        painter.drawEllipse(bounds);
    } else if (strokeTool_ == DrawTool::FilledEllipse) {
        painter.setBrush(color);
        painter.drawEllipse(bounds);
    }

    QPen guidePen(palette().color(QPalette::BrightText), 1, Qt::DashLine);
    guidePen.setCosmetic(true);
    painter.setPen(guidePen);
    painter.setBrush(Qt::NoBrush);
    if (strokeTool_ == DrawTool::Line) {
        painter.drawLine(QPointF((strokeStartPoint_.x() + 0.5) * zoom_,
                                 (strokeStartPoint_.y() + 0.5) * zoom_),
                         QPointF((lastImagePoint_.x() + 0.5) * zoom_,
                                 (lastImagePoint_.y() + 0.5) * zoom_));
    } else if (strokeTool_ == DrawTool::Ellipse ||
               strokeTool_ == DrawTool::FilledEllipse) {
        painter.drawEllipse(bounds);
    } else {
        painter.drawRoundedRect(bounds, cornerRadius, cornerRadius);
    }
    painter.restore();
}

void LevelCanvas::paintSelection(QPainter& painter) const
{
    if (!selectionActive_) {
        return;
    }

    painter.save();
    QPen pen(palette().color(QPalette::BrightText), 1, Qt::DashLine);
    pen.setCosmetic(true);
    painter.setPen(pen);
    const int width = selectionBounds_.width();
    const int height = selectionBounds_.height();
    const auto masked = [this, width, height](const int x, const int y) {
        return x >= 0 && y >= 0 && x < width && y < height &&
               selectionMask_[static_cast<std::size_t>(y) * width + x] != 0;
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            if (!masked(x, y)) {
                continue;
            }
            const qreal left = (selectionPosition_.x() + x) * zoom_;
            const qreal top = (selectionPosition_.y() + y) * zoom_;
            const qreal right = left + zoom_;
            const qreal bottom = top + zoom_;
            if (!masked(x, y - 1)) {
                painter.drawLine(QPointF(left, top), QPointF(right, top));
            }
            if (!masked(x + 1, y)) {
                painter.drawLine(QPointF(right, top), QPointF(right, bottom));
            }
            if (!masked(x, y + 1)) {
                painter.drawLine(QPointF(left, bottom), QPointF(right, bottom));
            }
            if (!masked(x - 1, y)) {
                painter.drawLine(QPointF(left, top), QPointF(left, bottom));
            }
        }
    }
    painter.restore();
}

void LevelCanvas::createSelection()
{
    QRect bounds;
    if (strokeTool_ == DrawTool::SelectFreehand) {
        if (freehandSelectionPoints_.size() < 3) {
            freehandSelectionPoints_.clear();
            return;
        }
        int left = freehandSelectionPoints_.front().x();
        int right = left;
        int top = freehandSelectionPoints_.front().y();
        int bottom = top;
        for (const QPoint& point : freehandSelectionPoints_) {
            left = std::min(left, point.x());
            right = std::max(right, point.x());
            top = std::min(top, point.y());
            bottom = std::max(bottom, point.y());
        }
        bounds = QRect(QPoint(left, top), QPoint(right, bottom));
    } else {
        bounds = QRect(strokeStartPoint_, lastImagePoint_).normalized();
    }
    if (bounds.isEmpty()) {
        freehandSelectionPoints_.clear();
        return;
    }

    selectionBounds_ = QRect(QPoint(0, 0), bounds.size());
    selectionSourcePosition_ = bounds.topLeft();
    selectionPosition_ = bounds.topLeft();
    const int width = bounds.width();
    const int height = bounds.height();
    const std::size_t size = static_cast<std::size_t>(width) * height;
    selectionMask_.assign(size, 0);
    selectionPixels_.assign(size, 0);

    QPainterPath freehandPath;
    if (strokeTool_ == DrawTool::SelectFreehand) {
        freehandPath.moveTo(freehandSelectionPoints_.front().x() + 0.5,
                            freehandSelectionPoints_.front().y() + 0.5);
        for (std::size_t index = 1; index < freehandSelectionPoints_.size();
             ++index) {
            freehandPath.lineTo(freehandSelectionPoints_[index].x() + 0.5,
                                freehandSelectionPoints_[index].y() + 0.5);
        }
        freehandPath.closeSubpath();
    }

    const double radiusX = width / 2.0;
    const double radiusY = height / 2.0;
    bool containsPixels = false;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            bool selected = strokeTool_ == DrawTool::SelectRectangle;
            if (strokeTool_ == DrawTool::SelectEllipse) {
                const double normalizedX = (x + 0.5 - radiusX) / radiusX;
                const double normalizedY = (y + 0.5 - radiusY) / radiusY;
                selected =
                    normalizedX * normalizedX + normalizedY * normalizedY <=
                    1.0;
            } else if (strokeTool_ == DrawTool::SelectFreehand) {
                selected = freehandPath.contains(
                    QPointF(bounds.left() + x + 0.5, bounds.top() + y + 0.5));
            }
            if (!selected) {
                continue;
            }
            const std::size_t localOffset =
                static_cast<std::size_t>(y) * width + x;
            selectionMask_[localOffset] = 1;
            selectionPixels_[localOffset] =
                level_->pixels[static_cast<std::size_t>(bounds.top() + y) *
                                   Level::Width +
                               bounds.left() + x];
            containsPixels = true;
        }
    }
    selectionActive_ = containsPixels;
    selectionHasSource_ = containsPixels;
    freehandSelectionPoints_.clear();
    viewport()->update();
}

void LevelCanvas::clearSelection()
{
    const bool wasPending = selectionEditPending_;
    selectionActive_ = false;
    selectionHasSource_ = false;
    selectionEditPending_ = false;
    movingSelection_ = false;
    selectionBounds_ = {};
    selectionMask_.clear();
    selectionPixels_.clear();
    freehandSelectionPoints_.clear();
    updateToolCursor();
    viewport()->update();
    if (wasPending) {
        emit pendingSelectionEditChanged(false);
    }
}

void LevelCanvas::setSelectionPosition(const QPoint& position)
{
    const int maximumX =
        static_cast<int>(Level::Width) - selectionBounds_.width();
    const int maximumY =
        static_cast<int>(Level::Height) - selectionBounds_.height();
    const QPoint constrained{std::clamp(position.x(), 0, maximumX),
                             std::clamp(position.y(), 0, maximumY)};
    if (constrained != selectionPosition_) {
        selectionPosition_ = constrained;
        if (!selectionEditPending_) {
            selectionEditPending_ = true;
            emit pendingSelectionEditChanged(true);
        }
    }
}

bool LevelCanvas::isSelectionTool(const DrawTool tool) const
{
    return tool == DrawTool::SelectRectangle ||
           tool == DrawTool::SelectEllipse || tool == DrawTool::SelectFreehand;
}

void LevelCanvas::updateToolCursor()
{
    if (viewport() == nullptr || panning_ || movingSelection_) {
        return;
    }
    viewport()->setCursor(drawTool_ == DrawTool::MoveSelection
                              ? Qt::OpenHandCursor
                              : Qt::CrossCursor);
}

void LevelCanvas::cancelCurve()
{
    if (curveStage_ == CurveStage::None) {
        return;
    }
    curveStage_ = CurveStage::None;
    drawing_ = false;
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
    viewport()->update();
}

void LevelCanvas::commitCurve()
{
    if (curveStage_ == CurveStage::None || level_ == nullptr) {
        return;
    }
    curveStage_ = CurveStage::None;
    drawing_ = false;
    drawBezier(curveStartPoint_, curveControl1_, curveControl2_, curveEndPoint_,
               strokePaintIndex_, strokeThickness_);
    commitStroke();
    viewport()->update();
}

void LevelCanvas::floodFill(const QPoint& point, const std::uint8_t index)
{
    if (selectionActive_ && !selectionContains(point.x(), point.y())) {
        return;
    }
    const std::uint8_t target = pixelAt(point.x(), point.y());
    if (target == index) {
        return;
    }

    std::vector<QPoint> pending{point};
    setPixel(point.x(), point.y(), index);
    while (!pending.empty()) {
        const QPoint current = pending.back();
        pending.pop_back();
        const QPoint neighbours[] = {
            {current.x() - 1, current.y()},
            {current.x() + 1, current.y()},
            {current.x(), current.y() - 1},
            {current.x(), current.y() + 1},
        };
        for (const QPoint& neighbour : neighbours) {
            if (neighbour.x() < 0 || neighbour.y() < 0 ||
                neighbour.x() >= static_cast<int>(Level::Width) ||
                neighbour.y() >= static_cast<int>(Level::Height) ||
                (selectionActive_ &&
                 !selectionContains(neighbour.x(), neighbour.y()))) {
                continue;
            }
            if (pixelAt(neighbour.x(), neighbour.y()) == target) {
                setPixel(neighbour.x(), neighbour.y(), index);
                pending.push_back(neighbour);
            }
        }
    }
    viewport()->update();
}

void LevelCanvas::beginStroke(const QString& commandText)
{
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
    strokeCommandText_ = commandText;
}

void LevelCanvas::commitStroke()
{
    if (!strokeChanges_.empty()) {
        undoStack_.push(new PixelEditCommand(level_, std::move(strokeChanges_),
                                             this, strokeCommandText_));
    }
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
}

std::uint8_t LevelCanvas::paintIndex() const
{
    return drawTool_ == DrawTool::Eraser ? 0 : selectedIndex_;
}

QPoint LevelCanvas::constrainedShapePoint(const QPoint& point) const
{
    const bool constrain = strokeTool_ == DrawTool::Rectangle ||
                           strokeTool_ == DrawTool::FilledRectangle ||
                           strokeTool_ == DrawTool::Ellipse ||
                           strokeTool_ == DrawTool::FilledEllipse ||
                           strokeTool_ == DrawTool::SelectRectangle ||
                           strokeTool_ == DrawTool::SelectEllipse;
    if (!constrain) {
        return point;
    }

    const int deltaX = point.x() - strokeStartPoint_.x();
    const int deltaY = point.y() - strokeStartPoint_.y();
    const int directionX = deltaX < 0 ? -1 : 1;
    const int directionY = deltaY < 0 ? -1 : 1;
    const int availableX = directionX < 0 ? strokeStartPoint_.x()
                                          : static_cast<int>(Level::Width) - 1 -
                                                strokeStartPoint_.x();
    const int availableY = directionY < 0 ? strokeStartPoint_.y()
                                          : static_cast<int>(Level::Height) -
                                                1 - strokeStartPoint_.y();
    const int side = std::min(
        {std::max(std::abs(deltaX), std::abs(deltaY)), availableX, availableY});
    return {strokeStartPoint_.x() + directionX * side,
            strokeStartPoint_.y() + directionY * side};
}

QString LevelCanvas::commandText() const
{
    switch (strokeTool_) {
    case DrawTool::Pencil:
        return tr("Pencil stroke");
    case DrawTool::Eraser:
        return tr("Eraser stroke");
    case DrawTool::Line:
        return tr("Line");
    case DrawTool::Rectangle:
        return tr("Rectangle");
    case DrawTool::FilledRectangle:
        return tr("Filled rectangle");
    case DrawTool::FloodFill:
        return tr("Flood fill");
    case DrawTool::Eyedropper:
        return tr("Eyedropper");
    case DrawTool::Ellipse:
        return tr("Ellipse");
    case DrawTool::FilledEllipse:
        return tr("Filled ellipse");
    case DrawTool::Spray:
        return tr("Spray stroke");
    case DrawTool::SelectRectangle:
        return tr("Rectangle selection");
    case DrawTool::SelectEllipse:
        return tr("Ellipse selection");
    case DrawTool::SelectFreehand:
        return tr("Freehand selection");
    case DrawTool::MoveSelection:
        return tr("Move selection");
    case DrawTool::BezierCurve:
        return tr("Bezier curve");
    }
    return tr("Edit");
}

void LevelCanvas::updateScrollBars()
{
    const int contentWidth =
        level_ == nullptr ? 0
                          : static_cast<int>(std::ceil(Level::Width * zoom_));
    const int contentHeight =
        level_ == nullptr ? 0
                          : static_cast<int>(std::ceil(Level::Height * zoom_));
    horizontalScrollBar()->setPageStep(viewport()->width());
    verticalScrollBar()->setPageStep(viewport()->height());
    horizontalScrollBar()->setRange(
        0, std::max(0, contentWidth - viewport()->width()));
    verticalScrollBar()->setRange(
        0, std::max(0, contentHeight - viewport()->height()));
}

void LevelCanvas::reportPosition(const QPoint& point)
{
    emit cursorPositionChanged(point.x(), point.y(),
                               pixelAt(point.x(), point.y()));
}
