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
                     LevelCanvas* canvas)
        : level_(level), changes_(std::move(changes)), canvas_(canvas)
    {
        setText(QObject::tr("Pencil stroke"));
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
    selectedIndex_ = index;
}

QUndoStack* LevelCanvas::undoStack()
{
    return &undoStack_;
}

void LevelCanvas::refreshImage()
{
    viewport()->update();
}

void LevelCanvas::paintEvent(QPaintEvent*)
{
    QPainter painter(viewport());
    painter.fillRect(viewport()->rect(), palette().brush(QPalette::Dark));

    if (level_ == nullptr) {
        painter.setPen(palette().color(QPalette::BrightText));
        painter.drawText(viewport()->rect(), Qt::AlignCenter, tr("No level loaded"));
        return;
    }

    QImage image(level_->pixels.data(), static_cast<int>(Level::Width),
                 static_cast<int>(Level::Height), static_cast<int>(Level::Width),
                 QImage::Format_Indexed8);
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
            drawing_ = true;
            lastImagePoint_ = point;
            beginStroke();
            setPixel(point.x(), point.y());
            viewport()->update();
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
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        lastPanPoint_ = current;
        event->accept();
        return;
    }

    const QPoint point = imagePoint(event->position());
    if (point.x() >= 0) {
        reportPosition(point);
        if (drawing_ && (event->buttons() & Qt::LeftButton)) {
            drawLine(lastImagePoint_, point);
            lastImagePoint_ = point;
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
    const int x = static_cast<int>((viewportPoint.x() +
                                    horizontalScrollBar()->value()) /
                                   zoom_);
    const int y = static_cast<int>((viewportPoint.y() +
                                    verticalScrollBar()->value()) /
                                   zoom_);
    if (x < 0 || y < 0 || x >= static_cast<int>(Level::Width) ||
        y >= static_cast<int>(Level::Height)) {
        return {-1, -1};
    }
    return {x, y};
}

bool LevelCanvas::setPixel(const int x, const int y)
{
    const std::size_t offset = static_cast<std::size_t>(y) * Level::Width +
                               static_cast<std::size_t>(x);
    if (level_->pixels[offset] == selectedIndex_) {
        return false;
    }
    const auto existing = strokeChangeIndices_.find(offset);
    if (existing == strokeChangeIndices_.end()) {
        strokeChangeIndices_.emplace(offset, strokeChanges_.size());
        strokeChanges_.push_back(PixelChange{
            offset,
            level_->pixels[offset],
            selectedIndex_,
        });
    } else {
        strokeChanges_[existing->second].newValue = selectedIndex_;
    }
    level_->pixels[offset] = selectedIndex_;
    return true;
}

void LevelCanvas::drawLine(const QPoint& from, const QPoint& to)
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
        changed |= setPixel(x0, y0);
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

void LevelCanvas::beginStroke()
{
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
}

void LevelCanvas::commitStroke()
{
    if (!strokeChanges_.empty()) {
        undoStack_.push(
            new PixelEditCommand(level_, std::move(strokeChanges_), this));
    }
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
}

void LevelCanvas::updateScrollBars()
{
    const int contentWidth = level_ == nullptr
                                 ? 0
                                 : static_cast<int>(std::ceil(Level::Width * zoom_));
    const int contentHeight = level_ == nullptr
                                  ? 0
                                  : static_cast<int>(std::ceil(Level::Height * zoom_));
    horizontalScrollBar()->setPageStep(viewport()->width());
    verticalScrollBar()->setPageStep(viewport()->height());
    horizontalScrollBar()->setRange(0,
                                    std::max(0, contentWidth - viewport()->width()));
    verticalScrollBar()->setRange(0,
                                  std::max(0, contentHeight - viewport()->height()));
}

void LevelCanvas::reportPosition(const QPoint& point)
{
    const std::size_t offset = static_cast<std::size_t>(point.y()) * Level::Width +
                               static_cast<std::size_t>(point.x());
    emit cursorPositionChanged(point.x(), point.y(), level_->pixels[offset]);
}
