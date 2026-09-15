#include "level_canvas.h"

#include <QEvent>
#include <QImage>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollBar>
#include <QUndoCommand>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {

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

} // namespace

LevelCanvas::LevelCanvas(QWidget* parent) : QAbstractScrollArea(parent)
{
    setMouseTracking(true);
    setBackgroundRole(QPalette::Dark);
    viewport()->setCursor(Qt::CrossCursor);
}

void LevelCanvas::setLevel(Level* level)
{
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
    if (selectedIndex_ == index) {
        return;
    }
    selectedIndex_ = index;
    emit selectedIndexChanged(index);
}

void LevelCanvas::setDrawTool(const DrawTool tool) { drawTool_ = tool; }

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

    QImage image(level_->pixels.data(), static_cast<int>(Level::Width),
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
        const QPoint point = imagePoint(event->position());
        if (point.x() >= 0) {
            lastImagePoint_ = point;
            strokeStartPoint_ = point;
            strokeTool_ = drawTool_;
            strokePaintIndex_ = paintIndex();

            if (strokeTool_ == DrawTool::Eyedropper) {
                const std::size_t offset =
                    static_cast<std::size_t>(point.y()) * Level::Width +
                    static_cast<std::size_t>(point.x());
                setSelectedIndex(level_->pixels[offset]);
            } else if (strokeTool_ == DrawTool::FloodFill) {
                beginStroke(tr("Flood fill"));
                floodFill(point, strokePaintIndex_);
                commitStroke();
            } else {
                drawing_ = true;
                const bool freehand = strokeTool_ == DrawTool::Pencil ||
                                      strokeTool_ == DrawTool::Eraser;
                beginStroke(commandText());
                if (freehand) {
                    setPixel(point.x(), point.y(), strokePaintIndex_);
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
        if (drawing_ && (event->buttons() & Qt::LeftButton) &&
            (strokeTool_ == DrawTool::Pencil ||
             strokeTool_ == DrawTool::Eraser)) {
            drawLine(lastImagePoint_, point, strokePaintIndex_);
            lastImagePoint_ = point;
        } else if (drawing_ && (event->buttons() & Qt::LeftButton)) {
            lastImagePoint_ = point;
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
        viewport()->setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        const bool wasDrawing = drawing_;
        drawing_ = false;
        if (wasDrawing) {
            const QPoint releasePoint = imagePoint(event->position());
            if (releasePoint.x() >= 0) {
                lastImagePoint_ = releasePoint;
            }
            if (strokeTool_ == DrawTool::Line) {
                drawLine(strokeStartPoint_, lastImagePoint_, strokePaintIndex_);
            } else if (strokeTool_ == DrawTool::Rectangle) {
                drawRectangle(strokeStartPoint_, lastImagePoint_,
                              strokePaintIndex_, false);
            } else if (strokeTool_ == DrawTool::FilledRectangle) {
                drawRectangle(strokeStartPoint_, lastImagePoint_,
                              strokePaintIndex_, true);
            } else if (strokeTool_ == DrawTool::Ellipse) {
                drawEllipse(strokeStartPoint_, lastImagePoint_,
                            strokePaintIndex_, false);
            } else if (strokeTool_ == DrawTool::FilledEllipse) {
                drawEllipse(strokeStartPoint_, lastImagePoint_,
                            strokePaintIndex_, true);
            }
            commitStroke();
        }
        event->accept();
        return;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
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

void LevelCanvas::drawLine(const QPoint& from, const QPoint& to,
                           const std::uint8_t index)
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
        changed |= setPixel(x0, y0, index);
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

void LevelCanvas::drawRectangle(const QPoint& from, const QPoint& to,
                                const std::uint8_t index, const bool filled)
{
    const int left = std::min(from.x(), to.x());
    const int right = std::max(from.x(), to.x());
    const int top = std::min(from.y(), to.y());
    const int bottom = std::max(from.y(), to.y());
    bool changed = false;

    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (filled || y == top || y == bottom || x == left || x == right) {
                changed |= setPixel(x, y, index);
            }
        }
    }
    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::drawEllipse(const QPoint& from, const QPoint& to,
                              const std::uint8_t index, const bool filled)
{
    const int left = std::min(from.x(), to.x());
    const int right = std::max(from.x(), to.x());
    const int top = std::min(from.y(), to.y());
    const int bottom = std::max(from.y(), to.y());
    const int width = right - left + 1;
    const int height = bottom - top + 1;

    if (width == 1 || height == 1) {
        drawLine({left, top}, {right, bottom}, index);
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

    bool changed = false;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            if (!inside(x, y)) {
                continue;
            }
            const bool boundary = !inside(x - 1, y) || !inside(x + 1, y) ||
                                  !inside(x, y - 1) || !inside(x, y + 1);
            if (filled || boundary) {
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
    if (!drawing_ || level_ == nullptr ||
        (strokeTool_ != DrawTool::Line && strokeTool_ != DrawTool::Rectangle &&
         strokeTool_ != DrawTool::FilledRectangle &&
         strokeTool_ != DrawTool::Ellipse &&
         strokeTool_ != DrawTool::FilledEllipse)) {
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
    const qreal strokeWidth = std::max<qreal>(1.0, zoom_);

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
        painter.drawRect(bounds);
    } else if (strokeTool_ == DrawTool::FilledRectangle) {
        painter.fillRect(bounds, color);
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
        painter.drawRect(bounds);
    }
    painter.restore();
}

void LevelCanvas::floodFill(const QPoint& point, const std::uint8_t index)
{
    const auto startOffset =
        static_cast<std::size_t>(point.y()) * Level::Width +
        static_cast<std::size_t>(point.x());
    const std::uint8_t target = level_->pixels[startOffset];
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
                neighbour.y() >= static_cast<int>(Level::Height)) {
                continue;
            }
            const auto offset =
                static_cast<std::size_t>(neighbour.y()) * Level::Width +
                static_cast<std::size_t>(neighbour.x());
            if (level_->pixels[offset] == target) {
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
    const std::size_t offset =
        static_cast<std::size_t>(point.y()) * Level::Width +
        static_cast<std::size_t>(point.x());
    emit cursorPositionChanged(point.x(), point.y(), level_->pixels[offset]);
}
