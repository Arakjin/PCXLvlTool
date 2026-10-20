#include "level_canvas.h"

#include "layer_model.h"
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
#include <QWheelEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace {

constexpr auto kSelectionMimeType = "application/x-vwing-level-selection";

const QImage& overthrustObjectImage(const std::string& type)
{
    static const QImage door(
        QStringLiteral(":/icons/overthrust/canvas-door.png"));
    static const QImage teleporter(
        QStringLiteral(":/icons/overthrust/canvas-teleporter.png"));
    static const QImage serviceOrb(
        QStringLiteral(":/icons/overthrust/canvas-service-orb.png"));
    static const QImage captureFlag(
        QStringLiteral(":/icons/overthrust/canvas-flag.png"));
    static const QImage empty;
    if (type == "sliding_door") return door;
    if (type == "teleporter") return teleporter;
    if (type == "service_orb") return serviceOrb;
    if (type == "capture_flag") return captureFlag;
    return empty;
}

QRectF fittedImageRect(const QRectF& bounds, const QImage& image,
                       const double fillFraction = 0.86)
{
    if (image.isNull()) return bounds;
    QSizeF size = image.size();
    size.scale(bounds.size() * fillFraction, Qt::KeepAspectRatio);
    return QRectF(bounds.center().x() - size.width() / 2.0,
                  bounds.center().y() - size.height() / 2.0,
                  size.width(), size.height());
}

QPointF rotatedPoint(const QPointF& point, const QPointF& center,
                     const double degrees)
{
    const double radians = degrees * 3.14159265358979323846 / 180.0;
    const double sine = std::sin(radians);
    const double cosine = std::cos(radians);
    const QPointF delta = point - center;
    return center + QPointF(delta.x() * cosine - delta.y() * sine,
                            delta.x() * sine + delta.y() * cosine);
}

template <typename Plot>
void rasterBrush(const int centerX, const int centerY, const int thickness,
                 const BrushShape shape, const int canvasWidth,
                 const int canvasHeight, Plot plot)
{
    const int lower = (thickness - 1) / 2;
    const int upper = thickness / 2;
    const double centerOffset = thickness % 2 == 0 ? 0.5 : 0.0;
    const double radius = thickness % 2 == 0 ? thickness / 2.0
                                              : (thickness - 1) / 2.0;
    for (int y = centerY - lower; y <= centerY + upper; ++y) {
        for (int x = centerX - lower; x <= centerX + upper; ++x) {
            if (x < 0 || y < 0 || x >= canvasWidth || y >= canvasHeight) {
                continue;
            }
            if (shape == BrushShape::Circle) {
                const double deltaX = x - (centerX + centerOffset);
                const double deltaY = y - (centerY + centerOffset);
                if (deltaX * deltaX + deltaY * deltaY > radius * radius) {
                    continue;
                }
            }
            plot(x, y);
        }
    }
}

template <typename Plot>
void rasterLine(QPoint from, const QPoint& to, const int thickness,
                const BrushShape brushShape, const int canvasWidth,
                const int canvasHeight, Plot plot)
{
    const int dx = std::abs(to.x() - from.x());
    const int sx = from.x() < to.x() ? 1 : -1;
    const int dy = -std::abs(to.y() - from.y());
    const int sy = from.y() < to.y() ? 1 : -1;
    int error = dx + dy;
    while (true) {
        rasterBrush(from.x(), from.y(), thickness, brushShape, canvasWidth,
                    canvasHeight, plot);
        if (from == to) {
            break;
        }
        const int twiceError = 2 * error;
        if (twiceError >= dy) {
            error += dy;
            from.rx() += sx;
        }
        if (twiceError <= dx) {
            error += dx;
            from.ry() += sy;
        }
    }
}

template <typename Plot>
void rasterBezier(const QPoint& start, const QPoint& control1,
                  const QPoint& control2, const QPoint& end,
                  const int thickness, const int canvasWidth,
                  const int canvasHeight, Plot plot)
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
            rasterLine(previous, current, thickness, BrushShape::Square,
                       canvasWidth, canvasHeight, plot);
            previous = current;
        }
    }
    if (previous != end) {
        rasterLine(previous, end, thickness, BrushShape::Square, canvasWidth,
                   canvasHeight, plot);
    }
}

template <typename Plot>
void rasterRectangle(const QPoint& from, const QPoint& to,
                     const std::uint8_t outlineIndex,
                     const std::uint8_t fillIndex, const int thickness,
                     const int cornerRadius, const ShapeMode mode, Plot plot)
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
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const bool inside =
                insideRounded(x, y, left, top, right, bottom, radius);
            const bool insideInterior =
                hasInterior &&
                insideRounded(x, y, innerLeft, innerTop, innerRight,
                              innerBottom, innerRadius);
            if (!inside) {
                continue;
            }
            if (mode == ShapeMode::FillOnly) {
                plot(x, y, fillIndex);
            } else if (!insideInterior) {
                plot(x, y, outlineIndex);
            } else if (mode == ShapeMode::OutlineAndFill) {
                plot(x, y, fillIndex);
            }
        }
    }
}

template <typename Plot>
void rasterEllipse(const QPoint& from, const QPoint& to,
                   const std::uint8_t outlineIndex,
                   const std::uint8_t fillIndex, const int thickness,
                   const ShapeMode mode, const int canvasWidth,
                   const int canvasHeight, Plot plot)
{
    const int left = std::min(from.x(), to.x());
    const int right = std::max(from.x(), to.x());
    const int top = std::min(from.y(), to.y());
    const int bottom = std::max(from.y(), to.y());
    const int width = right - left + 1;
    const int height = bottom - top + 1;
    if (width == 1 || height == 1) {
        rasterLine({left, top}, {right, bottom}, thickness, BrushShape::Square,
                   canvasWidth, canvasHeight,
                   [&](const int x, const int y) {
                       plot(x, y, mode == ShapeMode::FillOnly ? fillIndex
                                                              : outlineIndex);
                   });
        return;
    }
    const double radiusX = width / 2.0;
    const double radiusY = height / 2.0;
    const double centerX = left + radiusX;
    const double centerY = top + radiusY;
    const double innerRadiusX = radiusX - thickness;
    const double innerRadiusY = radiusY - thickness;
    for (int y = top; y <= bottom; ++y) {
        for (int x = left; x <= right; ++x) {
            const double normalizedX = (x + 0.5 - centerX) / radiusX;
            const double normalizedY = (y + 0.5 - centerY) / radiusY;
            if (normalizedX * normalizedX + normalizedY * normalizedY > 1.0) {
                continue;
            }
            bool insideInterior = false;
            if (innerRadiusX > 0.0 && innerRadiusY > 0.0) {
                const double innerX = (x + 0.5 - centerX) / innerRadiusX;
                const double innerY = (y + 0.5 - centerY) / innerRadiusY;
                insideInterior = innerX * innerX + innerY * innerY <= 1.0;
            }
            if (mode == ShapeMode::FillOnly) {
                plot(x, y, fillIndex);
            } else if (!insideInterior) {
                plot(x, y, outlineIndex);
            } else if (mode == ShapeMode::OutlineAndFill) {
                plot(x, y, fillIndex);
            }
        }
    }
}

class PixelEditCommand final : public QUndoCommand {
public:
    PixelEditCommand(Level* level, std::vector<PixelChange> changes,
                     const std::size_t layerIndex, LevelCanvas* canvas,
                     QString commandText)
        : level_(level), changes_(std::move(changes)),
          layerIndex_(layerIndex), canvas_(canvas)
    {
        setText(commandText);
    }

    void undo() override
    {
        for (const PixelChange& change : changes_) {
            Level::Layer& layer = level_->layers[layerIndex_];
            layer.pixels[change.offset] = change.oldValue;
            layer.mask[change.offset] = change.oldMask;
        }
        canvas_->refreshImage();
    }

    void redo() override
    {
        for (const PixelChange& change : changes_) {
            Level::Layer& layer = level_->layers[layerIndex_];
            layer.pixels[change.offset] = change.newValue;
            layer.mask[change.offset] = change.newMask;
        }
        canvas_->refreshImage();
    }

private:
    Level* level_;
    std::vector<PixelChange> changes_;
    std::size_t layerIndex_;
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
    if (level == nullptr && level_ != nullptr) {
        undoStack_->clear();
    }
    clearSelection();
    curveStage_ = CurveStage::None;
    curveHandle_ = CurveHandle::None;
    polygonActive_ = false;
    polygonPoints_.clear();
    teamBasePolygonActive_ = false;
    teamBasePolygonObject_ = -1;
    teamBasePolygonPoints_.clear();
    textDraftActive_ = false;
    movingTextBox_ = false;
    textDraft_.clear();
    level_ = level;
    if (level_ != nullptr) {
        auto [entry, inserted] = undoStacks_.try_emplace(level_);
        if (inserted) {
            entry->second = std::make_unique<QUndoStack>();
            QUndoStack* stack = entry->second.get();
            connect(stack, &QUndoStack::cleanChanged, this,
                    [this, level, stack](const bool clean) {
                        if (level_ == level && undoStack_ == stack) {
                            emit undoCleanChanged(clean);
                        }
                    });
        }
        undoStack_ = entry->second.get();
        initializeBackgroundLayer(*level_);
        flattenLayers(*level_);
    } else {
        undoStack_ = &fallbackUndoStack_;
    }
    drawing_ = false;
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
    emit zoomChanged(zoom_);
}

double LevelCanvas::zoom() const { return zoom_; }

void LevelCanvas::setGame(const GameId game)
{
    game_ = game;
    if (isReservedPaletteIndex(game_, selectedIndex_)) {
        setSelectedIndex(game_ == GameId::Wings
                             ? 128
                             : game_ == GameId::Auts ? 7 : 56);
    }
    if (isReservedPaletteIndex(game_, secondaryIndex_)) {
        setSecondaryIndex(game_ == GameId::Wings
                              ? 129
                              : game_ == GameId::Auts ? 39 : 57);
    }
}

void LevelCanvas::setSelectedIndex(const std::uint8_t index)
{
    if (selectedIndex_ == index || isReservedPaletteIndex(game_, index)) {
        return;
    }
    selectedIndex_ = index;
    viewport()->update();
    emit selectedIndexChanged(index);
}

void LevelCanvas::setSecondaryIndex(const std::uint8_t index)
{
    if (secondaryIndex_ == index || isReservedPaletteIndex(game_, index)) {
        return;
    }
    secondaryIndex_ = index;
    emit secondaryIndexChanged(index);
}

void LevelCanvas::setDrawTool(const DrawTool tool)
{
    if (drawTool_ == DrawTool::BezierCurve && tool != DrawTool::BezierCurve) {
        cancelCurve();
    }
    if (drawTool_ == DrawTool::Polygon && tool != DrawTool::Polygon) {
        commitPolygon();
    }
    if (drawTool_ == DrawTool::Text && tool != DrawTool::Text) {
        commitText();
    }
    drawTool_ = tool;
    updateToolCursor();
    viewport()->update();
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
    } else if (tool == DrawTool::Polygon) {
        polygonThickness_ = value;
    }
    viewport()->update();
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
    if (tool == DrawTool::Polygon) {
        return polygonThickness_;
    }
    return 1;
}

void LevelCanvas::setBrushShape(const DrawTool tool, const BrushShape shape)
{
    if (tool == DrawTool::Pencil) {
        pencilBrushShape_ = shape;
    } else if (tool == DrawTool::Eraser) {
        eraserBrushShape_ = shape;
    } else {
        return;
    }
    viewport()->update();
}

BrushShape LevelCanvas::brushShape(const DrawTool tool) const
{
    if (tool == DrawTool::Pencil) {
        return pencilBrushShape_;
    }
    if (tool == DrawTool::Eraser) {
        return eraserBrushShape_;
    }
    return BrushShape::Square;
}

void LevelCanvas::setRectangleCornerRadius(const int radius)
{
    rectangleCornerRadius_ = std::clamp(radius, 0, 32);
}

int LevelCanvas::rectangleCornerRadius() const
{
    return rectangleCornerRadius_;
}

void LevelCanvas::setShapeMode(const ShapeMode mode) { shapeMode_ = mode; }

ShapeMode LevelCanvas::shapeMode() const { return shapeMode_; }

void LevelCanvas::setMaterialSelectionMode(const MaterialSelectionMode mode)
{
    materialSelectionMode_ = mode;
}

MaterialSelectionMode LevelCanvas::materialSelectionMode() const
{
    return materialSelectionMode_;
}

void LevelCanvas::setTextFontFamily(const QString& family)
{
    textFontFamily_ = family;
    viewport()->update();
}

void LevelCanvas::setTextPixelSize(const int size)
{
    textPixelSize_ = std::clamp(size, 6, 64);
    viewport()->update();
}

void LevelCanvas::setPaletteColor(const std::uint8_t index, const RGB color)
{
    if (level_ == nullptr || level_->palette[index] == color) {
        return;
    }
    undoStack_->push(
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
    undoStack_->push(new PaletteReplaceCommand(this, level_->palette, palette));
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
    if (polygonActive_) {
        commitPolygon();
    }
    if (textDraftActive_) {
        commitText();
    }
    if (!selectionActive_ || level_ == nullptr) {
        return;
    }

    using LayerValue = std::pair<std::uint8_t, std::uint8_t>;
    std::unordered_map<std::size_t, LayerValue> finalValues;
    Level::Layer& layer = level_->layers[level_->activeLayer];
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
                const std::size_t sourceOffset =
                    static_cast<std::size_t>(source.y()) * level_->width +
                    source.x();
                finalValues[sourceOffset] =
                    level_->activeLayer == 0
                        ? LayerValue{0, 1}
                        : LayerValue{layer.pixels[sourceOffset], 0};
            }
            if (selectionOpacity_[localOffset] != 0) {
                const QPoint destination = selectionPosition_ + QPoint(x, y);
                finalValues[static_cast<std::size_t>(destination.y()) *
                                level_->width +
                            destination.x()] =
                    {selectionPixels_[localOffset], 1};
            }
        }
    }

    std::vector<PixelChange> changes;
    changes.reserve(finalValues.size());
    for (const auto& [offset, value] : finalValues) {
        if (layer.pixels[offset] != value.first ||
            layer.mask[offset] != value.second) {
            changes.push_back({offset, layer.pixels[offset], value.first,
                               layer.mask[offset], value.second});
        }
    }
    std::sort(changes.begin(), changes.end(),
              [](const PixelChange& left, const PixelChange& right) {
                  return left.offset < right.offset;
              });
    clearSelection();
    if (!changes.empty()) {
        undoStack_->push(new PixelEditCommand(
            level_, std::move(changes), level_->activeLayer, this,
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
    Level::Layer& layer = level_->layers[level_->activeLayer];
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
                    static_cast<std::size_t>(source.y()) * level_->width +
                    source.x();
                const std::uint8_t newValue =
                    level_->activeLayer == 0 ? 0 : layer.pixels[offset];
                const std::uint8_t newMask =
                    level_->activeLayer == 0 ? 1 : 0;
                if (layer.pixels[offset] != newValue ||
                    layer.mask[offset] != newMask) {
                    changes.push_back({offset, layer.pixels[offset], newValue,
                                       layer.mask[offset], newMask});
                }
            }
        }
    }
    clearSelection();
    if (!changes.empty()) {
        undoStack_->push(new PixelEditCommand(
            level_, std::move(changes), level_->activeLayer, this,
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
               << static_cast<quint8>(selectionPixels_[index])
               << static_cast<quint8>(selectionOpacity_[index]);
    }
    selectionClipboard_ = payload;
    auto* mimeData = new QMimeData();
    mimeData->setData(QString::fromLatin1(kSelectionMimeType), payload);
    QApplication::clipboard()->setMimeData(mimeData);
}

void LevelCanvas::pasteSelection()
{
    if (level_ == nullptr) {
        return;
    }
    const QString mimeType = QString::fromLatin1(kSelectionMimeType);
    const QMimeData* mimeData = QApplication::clipboard()->mimeData();
    QByteArray payload;
    if (mimeData != nullptr && mimeData->hasFormat(mimeType)) {
        payload = mimeData->data(mimeType);
    } else {
        payload = selectionClipboard_;
    }
    if (payload.isEmpty()) {
        return;
    }
    QDataStream stream(&payload, QIODevice::ReadOnly);
    stream.setVersion(QDataStream::Qt_6_2);
    quint32 width = 0;
    quint32 height = 0;
    qint32 sourceX = 0;
    qint32 sourceY = 0;
    stream >> width >> height >> sourceX >> sourceY;
    if (stream.status() != QDataStream::Ok || width == 0 || height == 0 ||
        width > static_cast<qint64>(level_->width) ||
        height > static_cast<qint64>(level_->height) ||
        static_cast<std::size_t>(width) * height > level_->pixelCount()) {
        return;
    }
    const std::size_t size = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> mask(size);
    std::vector<std::uint8_t> pixels(size);
    std::vector<std::uint8_t> opacity(size);
    bool containsPixels = false;
    for (std::size_t index = 0; index < size; ++index) {
        quint8 maskValue = 0;
        quint8 pixelValue = 0;
        quint8 opacityValue = 0;
        stream >> maskValue >> pixelValue >> opacityValue;
        mask[index] = maskValue == 0 ? 0 : 1;
        pixels[index] = pixelValue;
        opacity[index] = opacityValue == 0 ? 0 : 1;
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
    selectionOpacity_ = std::move(opacity);
    selectionHasSource_ = false;
    selectionActive_ = true;
    selectionEditPending_ = true;
    emit pendingSelectionEditChanged(true);
    const qint64 maximumX = static_cast<qint64>(level_->width) - width;
    const qint64 maximumY = static_cast<qint64>(level_->height) - height;
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
    selectionBounds_ = QRect(0, 0, static_cast<int>(level_->width),
                             static_cast<int>(level_->height));
    selectionSourcePosition_ = {0, 0};
    selectionPosition_ = {0, 0};
    selectionMask_.assign(level_->pixelCount(), 1);
    const Level::Layer& layer = level_->layers[level_->activeLayer];
    selectionPixels_.assign(layer.pixels.begin(), layer.pixels.end());
    selectionOpacity_.assign(layer.mask.begin(), layer.mask.end());
    selectionHasSource_ = true;
    selectionActive_ = true;
    selectionEditPending_ = false;
    viewport()->update();
}

bool LevelCanvas::hasSelection() const { return selectionActive_; }

bool LevelCanvas::hasPendingSelectionEdit() const
{
    return selectionEditPending_ || textDraftActive_;
}

int LevelCanvas::layerCount() const
{
    return level_ == nullptr ? 0 : static_cast<int>(level_->layers.size());
}

int LevelCanvas::activeLayerIndex() const
{
    return level_ == nullptr ? -1 : static_cast<int>(level_->activeLayer);
}

bool LevelCanvas::addLayer()
{
    if (level_ == nullptr || level_->layers.size() >= Level::MaxLayers) {
        return false;
    }
    commitSelection();
    level_->layers.emplace_back();
    level_->layers.back().pixels.resize(level_->pixelCount());
    level_->layers.back().mask.resize(level_->pixelCount());
    level_->layers.back().name =
        "Layer " + std::to_string(level_->layers.size() - 1);
    level_->activeLayer = level_->layers.size() - 1;
    undoStack_->clear();
    emit layersChanged();
    return true;
}

bool LevelCanvas::deleteActiveLayer()
{
    if (level_ == nullptr || level_->activeLayer == 0) {
        return false;
    }
    commitSelection();
    level_->layers.erase(level_->layers.begin() +
                         static_cast<std::ptrdiff_t>(level_->activeLayer));
    level_->activeLayer = std::min(level_->activeLayer,
                                   level_->layers.size() - 1);
    undoStack_->clear();
    flattenLayers(*level_);
    emit layersChanged();
    viewport()->update();
    return true;
}

bool LevelCanvas::duplicateActiveLayer()
{
    if (level_ == nullptr || level_->layers.size() >= Level::MaxLayers) {
        return false;
    }
    commitSelection();
    const std::size_t sourceIndex = level_->activeLayer;
    const auto position = level_->layers.begin() +
                          static_cast<std::ptrdiff_t>(sourceIndex + 1);
    auto inserted = level_->layers.emplace(position);
    const Level::Layer& source = level_->layers[sourceIndex];
    inserted->name = source.name + " copy";
    inserted->visible = source.visible;
    inserted->locked = source.locked;
    inserted->pixels = source.pixels;
    inserted->mask = source.mask;
    ++level_->activeLayer;
    undoStack_->clear();
    flattenLayers(*level_);
    emit layersChanged();
    viewport()->update();
    return true;
}

bool LevelCanvas::mergeActiveLayerDown()
{
    if (level_ == nullptr || level_->activeLayer == 0) {
        return false;
    }
    const std::size_t sourceIndex = level_->activeLayer;
    const std::size_t destinationIndex = sourceIndex - 1;
    const Level::Layer& initialSource = level_->layers[sourceIndex];
    const Level::Layer& initialDestination = level_->layers[destinationIndex];
    if (!initialSource.visible || !initialDestination.visible ||
        initialSource.locked || initialDestination.locked) {
        return false;
    }
    commitSelection();
    Level::Layer& source = level_->layers[sourceIndex];
    Level::Layer& destination = level_->layers[destinationIndex];
    for (std::size_t offset = 0; offset < level_->pixelCount(); ++offset) {
        if (source.mask[offset] == 0) {
            continue;
        }
        destination.pixels[offset] = source.pixels[offset];
        destination.mask[offset] = 1;
    }
    level_->layers.erase(level_->layers.begin() +
                         static_cast<std::ptrdiff_t>(sourceIndex));
    level_->activeLayer = destinationIndex;
    undoStack_->clear();
    flattenLayers(*level_);
    emit layersChanged();
    viewport()->update();
    return true;
}

bool LevelCanvas::moveActiveLayer(const int direction)
{
    if (level_ == nullptr || level_->activeLayer == 0 || direction == 0) {
        return false;
    }
    const int target = static_cast<int>(level_->activeLayer) + direction;
    if (target < 1 || target >= static_cast<int>(level_->layers.size())) {
        return false;
    }
    commitSelection();
    std::swap(level_->layers[level_->activeLayer],
              level_->layers[static_cast<std::size_t>(target)]);
    level_->activeLayer = static_cast<std::size_t>(target);
    undoStack_->clear();
    flattenLayers(*level_);
    emit layersChanged();
    viewport()->update();
    return true;
}

void LevelCanvas::setActiveLayer(const int index)
{
    if (level_ == nullptr || index < 0 ||
        index >= static_cast<int>(level_->layers.size()) ||
        static_cast<std::size_t>(index) == level_->activeLayer) {
        return;
    }
    commitSelection();
    level_->activeLayer = static_cast<std::size_t>(index);
    emit layersChanged();
}

void LevelCanvas::setLayerVisible(const int index, const bool visible)
{
    if (level_ == nullptr || index <= 0 ||
        index >= static_cast<int>(level_->layers.size()) ||
        level_->layers[static_cast<std::size_t>(index)].visible == visible) {
        return;
    }
    commitSelection();
    level_->layers[static_cast<std::size_t>(index)].visible = visible;
    flattenLayers(*level_);
    emit layersChanged();
    viewport()->update();
}

void LevelCanvas::setLayerLocked(const int index, const bool locked)
{
    if (level_ == nullptr || index < 0 ||
        index >= static_cast<int>(level_->layers.size()) ||
        level_->layers[static_cast<std::size_t>(index)].locked == locked) {
        return;
    }
    commitSelection();
    level_->layers[static_cast<std::size_t>(index)].locked = locked;
    emit layersChanged();
}

void LevelCanvas::renameLayer(const int index, const QString& name)
{
    if (level_ == nullptr || index <= 0 ||
        index >= static_cast<int>(level_->layers.size()) || name.isEmpty()) {
        return;
    }
    level_->layers[static_cast<std::size_t>(index)].name =
        name.toStdString();
    emit layersChanged();
}

QUndoStack* LevelCanvas::undoStack() { return undoStack_; }

void LevelCanvas::forgetLevel(Level* level)
{
    if (level == nullptr) {
        return;
    }
    if (level_ == level) {
        setLevel(nullptr);
    }
    undoStacks_.erase(level);
}

bool LevelCanvas::hasDirtyUndoStack() const
{
    for (const auto& entry : undoStacks_) {
        if (!entry.second->isClean()) {
            return true;
        }
    }
    return false;
}

void LevelCanvas::markAllUndoStacksClean()
{
    for (const auto& entry : undoStacks_) {
        entry.second->setClean();
    }
}

void LevelCanvas::refreshImage()
{
    if (level_ != nullptr) {
        flattenLayers(*level_);
    }
    viewport()->update();
}

void LevelCanvas::setTrueColorBackground(const std::uint8_t* rgba,
                                         const int width, const int height)
{
    if (rgba == nullptr || width <= 0 || height <= 0) {
        trueColorBackground_ = QImage();
    } else {
        const QImage view(rgba, width, height, width * 4,
                          QImage::Format_RGBA8888);
        trueColorBackground_ = view.copy();
    }
    viewport()->update();
}

void LevelCanvas::setObjects(std::vector<OverThrustObject>* objects)
{
    objects_ = objects;
    if (objects_ == nullptr || selectedObject_ >=
                                   static_cast<int>(objects_->size())) {
        selectedObject_ = -1;
    }
    viewport()->update();
}

void LevelCanvas::setObjectEditingEnabled(const bool enabled)
{
    objectEditingEnabled_ = enabled;
    objectDragMode_ = ObjectDragMode::None;
    if (!enabled) cancelTeamBasePolygon();
    viewport()->update();
}

void LevelCanvas::setSelectedObject(const int index)
{
    selectedObject_ = objects_ != nullptr && index >= 0 &&
                              index < static_cast<int>(objects_->size())
                          ? index
                          : -1;
    viewport()->update();
}

void LevelCanvas::beginTeamBasePolygon(const int objectIndex)
{
    if (level_ == nullptr || objects_ == nullptr) return;
    teamBasePolygonActive_ = true;
    teamBasePolygonObject_ = objectIndex;
    teamBasePolygonPoints_.clear();
    viewport()->setFocus(Qt::OtherFocusReason);
    viewport()->setCursor(Qt::CrossCursor);
    viewport()->update();
    emit teamBasePolygonActiveChanged(true);
}

void LevelCanvas::commitTeamBasePolygon()
{
    if (!teamBasePolygonActive_) return;
    if (teamBasePolygonPoints_.size() >= 3) {
        emit teamBasePolygonRequested(teamBasePolygonPoints_,
                                      teamBasePolygonObject_);
    }
    cancelTeamBasePolygon();
}

void LevelCanvas::cancelTeamBasePolygon()
{
    const bool wasActive = teamBasePolygonActive_;
    teamBasePolygonActive_ = false;
    teamBasePolygonObject_ = -1;
    teamBasePolygonPoints_.clear();
    updateToolCursor();
    viewport()->update();
    if (wasActive) emit teamBasePolygonActiveChanged(false);
}

void LevelCanvas::paintObjects(QPainter& painter) const
{
    if (objects_ == nullptr || game_ != GameId::OverThrust) return;
    painter.save();
    const auto objectCenter = [&](const std::size_t index) {
        const OverThrustObject& object = (*objects_)[index];
        const OverThrustTransform& transform =
            objectDragMode_ != ObjectDragMode::None &&
                    static_cast<int>(index) == selectedObject_
                ? objectTransformPreview_
                : object.transform;
        return QPointF((transform.x + transform.width / 2.0) * zoom_,
                       (transform.y + transform.height / 2.0) * zoom_);
    };

    QPen connectionPen(QColor(175, 120, 255, 220), 2.0, Qt::DotLine,
                       Qt::RoundCap);
    connectionPen.setCosmetic(true);
    painter.setPen(connectionPen);
    for (std::size_t index = 0; index < objects_->size(); ++index) {
        const OverThrustObject& object = (*objects_)[index];
        if (object.type != "teleporter" || object.targetObjectId.empty())
            continue;
        const auto target = std::find_if(
            objects_->begin(), objects_->end(),
            [&](const OverThrustObject& candidate) {
                return candidate.type == "teleporter" &&
                       candidate.id == object.targetObjectId;
            });
        if (target == objects_->end()) continue;
        const auto targetIndex = static_cast<std::size_t>(
            std::distance(objects_->begin(), target));
        painter.drawLine(objectCenter(index), objectCenter(targetIndex));
    }

    for (std::size_t index = 0; index < objects_->size(); ++index) {
        const OverThrustObject& object = (*objects_)[index];
        const OverThrustTransform& transform =
            objectDragMode_ != ObjectDragMode::None &&
                    static_cast<int>(index) == selectedObject_
                ? objectTransformPreview_
                : object.transform;
        const int x = transform.x;
        const int y = transform.y;
        const QRectF rectangle(x * zoom_, y * zoom_,
                               transform.width * zoom_,
                               transform.height * zoom_);
        const bool selected = static_cast<int>(index) == selectedObject_;
        if (object.type == "team_base" && object.areaPolygon.size() >= 3) {
            const int moveX = x - object.transform.x;
            const int moveY = y - object.transform.y;
            QPolygonF polygon;
            for (const OverThrustPoint& point : object.areaPolygon) {
                polygon << QPointF((point.x + moveX) * zoom_,
                                   (point.y + moveY) * zoom_);
            }
            painter.setPen(QPen(selected ? QColor(255, 255, 255)
                                         : QColor(65, 145, 255),
                                selected ? 2 : 1));
            painter.setBrush(QColor(70, 130, 255, 85));
            painter.drawPolygon(polygon);
            continue;
        }
        if (object.type == "sliding_door" && selected) {
            const double slideDistance = std::hypot(
                object.openOffsetX, object.openOffsetY);
            const double slideRadians = transform.rotationMillidegrees *
                3.14159265358979323846 / 180000.0;
            const double slideX = slideDistance * std::cos(slideRadians);
            const double slideY = slideDistance * std::sin(slideRadians);
            const QRectF ghost(
                (x + slideX) * zoom_,
                (y + slideY) * zoom_,
                transform.width * zoom_,
                transform.height * zoom_);
            const QPointF ghostCenter = ghost.center();
            QPen ghostPen(QColor(220, 240, 255, 190));
            ghostPen.setStyle(Qt::DashLine);
            painter.setPen(ghostPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawLine(rectangle.center(), ghost.center());
            painter.save();
            painter.translate(ghostCenter);
            painter.rotate(transform.rotationMillidegrees / 1000.0);
            painter.translate(-ghostCenter);
            painter.setPen(ghostPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRect(ghost);
            painter.restore();
        }
        const QPointF center = rectangle.center();
        painter.save();
        painter.translate(center);
        painter.rotate(transform.rotationMillidegrees / 1000.0);
        painter.translate(-center);
        const QColor color =
            object.type == "sliding_door"
                ? QColor(210, 220, 235, 150)
                : object.type == "teleporter"
                      ? QColor(80, 160, 255, 150)
                      : object.type == "service_orb"
                            ? QColor(90, 240, 190, 150)
                            : QColor(255, 210, 80, 150);
        const QImage& icon = overthrustObjectImage(object.type);
        if (object.type == "team_base") {
            painter.fillRect(rectangle, QColor(70, 130, 255, 85));
        } else if (icon.isNull()) {
            painter.fillRect(rectangle, color);
        }
        if (!icon.isNull()) {
            painter.setOpacity(0.92);
            painter.drawImage(object.type == "sliding_door"
                                  ? rectangle
                                  : fittedImageRect(rectangle, icon),
                              icon);
            painter.setOpacity(1.0);
        }
        painter.setPen(QPen(selected ? QColor(255, 255, 255)
                                     : QColor(20, 20, 20),
                            selected ? 2 : 1));
        painter.drawRect(rectangle);
        if (selected) {
            constexpr double handle = 7.0;
            const auto drawHandle = [&](const QPointF& point) {
                painter.fillRect(QRectF(point.x() - handle / 2,
                                        point.y() - handle / 2,
                                        handle, handle),
                                 Qt::white);
            };
            drawHandle({rectangle.right(), rectangle.center().y()});
            drawHandle({rectangle.center().x(), rectangle.bottom()});
            drawHandle(rectangle.bottomRight());
            const QPointF rotateHandle(rectangle.center().x(),
                                       rectangle.top() - 15.0);
            painter.drawLine(QPointF(rectangle.center().x(), rectangle.top()),
                             rotateHandle);
            painter.setBrush(Qt::white);
            painter.drawEllipse(rotateHandle, handle / 2, handle / 2);
        }
        painter.restore();
    }
    if (teamBasePolygonActive_ && !teamBasePolygonPoints_.isEmpty()) {
        QPolygonF preview;
        for (const QPoint& point : teamBasePolygonPoints_) {
            preview << QPointF(point.x() * zoom_, point.y() * zoom_);
        }
        if (hoverImagePoint_.x() >= 0 &&
            teamBasePolygonPoints_.back() != hoverImagePoint_) {
            preview << QPointF(hoverImagePoint_.x() * zoom_,
                               hoverImagePoint_.y() * zoom_);
        }
        QPen pen(QColor(100, 180, 255), 2.0, Qt::DashLine);
        pen.setCosmetic(true);
        painter.setPen(pen);
        painter.setBrush(QColor(70, 130, 255, 55));
        painter.drawPolygon(preview);
        for (const QPointF& point : preview) {
            painter.drawEllipse(point, 3.0, 3.0);
        }
    }
    painter.restore();
}

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
                        static_cast<std::size_t>(source.y()) * level_->width +
                        source.x();
                    composedPixels[sourceOffset] = compositeLayerPixel(
                        *level_, sourceOffset, level_->activeLayer);
                }
                if (selectionOpacity_[localOffset] == 0) {
                    continue;
                }
                const QPoint destination = selectionPosition_ + QPoint(x, y);
                const std::size_t destinationOffset =
                    static_cast<std::size_t>(destination.y()) * level_->width +
                    destination.x();
                composedPixels[destinationOffset] =
                    selectionPixels_[localOffset];
            }
        }
        imagePixels = composedPixels.data();
    }

    QImage image(imagePixels, static_cast<int>(level_->width),
                 static_cast<int>(level_->height),
                 static_cast<int>(level_->width), QImage::Format_Indexed8);
    QList<QRgb> colors;
    colors.reserve(static_cast<qsizetype>(level_->palette.size()));
    for (const RGB& color : level_->palette) {
        colors.append(qRgb(color.r, color.g, color.b));
    }
    if (game_ == GameId::OverThrust && !trueColorBackground_.isNull()) {
        colors[0] = qRgba(0, 0, 0, 0);
    }
    image.setColorTable(colors);

    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
    painter.translate(-horizontalScrollBar()->value(),
                      -verticalScrollBar()->value());
    if (game_ == GameId::OverThrust && !trueColorBackground_.isNull()) {
        painter.drawImage(QRectF(0, 0, level_->width * zoom_,
                                 level_->height * zoom_),
                          trueColorBackground_);
    }
    painter.drawImage(QRectF(0, 0, level_->width * zoom_,
                             level_->height * zoom_),
                      image);
    paintObjects(painter);
    paintShapePreview(painter);
    paintSelection(painter);
    paintBrushPreview(painter);
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

    if (teamBasePolygonActive_ && level_ != nullptr) {
        if (event->button() == Qt::RightButton) {
            commitTeamBasePolygon();
            event->accept();
            return;
        }
        if (event->button() == Qt::LeftButton) {
            const QPoint point = imagePoint(event->position());
            if (point.x() >= 0 &&
                (teamBasePolygonPoints_.isEmpty() ||
                 teamBasePolygonPoints_.back() != point)) {
                teamBasePolygonPoints_ << point;
            }
            viewport()->update();
            event->accept();
            return;
        }
    }

    if (objectEditingEnabled_ && event->button() == Qt::RightButton) {
        event->accept();
        return;
    }
    if (objectEditingEnabled_ && event->button() == Qt::LeftButton &&
        objects_ != nullptr && level_ != nullptr) {
        const QPoint point = imagePoint(event->position());
        const QPointF precisePoint(
            (event->position().x() + horizontalScrollBar()->value()) / zoom_,
            (event->position().y() + verticalScrollBar()->value()) / zoom_);
        if (selectedObject_ >= 0 &&
            selectedObject_ < static_cast<int>(objects_->size())) {
            const OverThrustObject& selected =
                (*objects_)[static_cast<std::size_t>(selectedObject_)];
            if (selected.type != "team_base") {
                const OverThrustTransform& transform = selected.transform;
                const QPointF center(transform.x + transform.width / 2.0,
                                     transform.y + transform.height / 2.0);
                const double angle = transform.rotationMillidegrees / 1000.0;
                const double handleRadius = std::max(1.5, 5.0 / zoom_);
                const auto near = [&](const QPointF& unrotated) {
                    const QPointF handle =
                        rotatedPoint(unrotated, center, angle);
                    const QPointF delta = precisePoint - handle;
                    return delta.x() * delta.x() + delta.y() * delta.y() <=
                           handleRadius * handleRadius;
                };
                ObjectDragMode handleMode = ObjectDragMode::None;
                if (near(QPointF(transform.x + transform.width,
                                 transform.y + transform.height)))
                    handleMode = ObjectDragMode::ResizeBoth;
                else if (near(QPointF(transform.x + transform.width,
                                      center.y())))
                    handleMode = ObjectDragMode::ResizeWidth;
                else if (near(QPointF(center.x(),
                                      transform.y + transform.height)))
                    handleMode = ObjectDragMode::ResizeHeight;
                else if (near(QPointF(center.x(),
                                      transform.y - 15.0 / zoom_)))
                    handleMode = ObjectDragMode::Rotate;
                if (handleMode != ObjectDragMode::None) {
                    objectDragMode_ = handleMode;
                    objectMoveAnchor_ = point;
                    objectTransformStart_ = transform;
                    objectTransformPreview_ = transform;
                    viewport()->setCursor(Qt::SizeAllCursor);
                    event->accept();
                    return;
                }
            }
        }
        int hit = -1;
        for (std::size_t index = objects_->size(); index-- > 0;) {
            const OverThrustObject& object = (*objects_)[index];
            const OverThrustTransform& transform = object.transform;
            bool contains = QRect(transform.x, transform.y, transform.width,
                                  transform.height)
                                .contains(point);
            if (object.type == "team_base" &&
                object.areaPolygon.size() >= 3) {
                QPolygon polygon;
                for (const OverThrustPoint& vertex : object.areaPolygon)
                    polygon << QPoint(vertex.x, vertex.y);
                contains = polygon.containsPoint(point, Qt::OddEvenFill);
            }
            if (contains) {
                hit = static_cast<int>(index);
                break;
            }
        }
        selectedObject_ = hit;
        emit objectSelectionChanged(hit);
        if (hit >= 0) {
            objectDragMode_ = ObjectDragMode::Move;
            objectMoveAnchor_ = point;
            const auto& transform = (*objects_)[static_cast<std::size_t>(hit)]
                                        .transform;
            objectTransformStart_ = transform;
            objectTransformPreview_ = transform;
            viewport()->setCursor(Qt::ClosedHandCursor);
        }
        viewport()->update();
        event->accept();
        return;
    }

    if ((event->button() == Qt::LeftButton ||
         event->button() == Qt::RightButton) &&
        level_ != nullptr) {
        setFocus(Qt::MouseFocusReason);
        const QPoint point = imagePoint(event->position());
        if (point.x() >= 0) {
            hoverImagePoint_ = point;
            if (drawTool_ == DrawTool::Polygon) {
                if (!polygonActive_) {
                    strokeButton_ = event->button();
                    strokeTool_ = drawTool_;
                    strokePaintIndex_ = paintIndex(strokeButton_);
                    strokeThickness_ = toolThickness(strokeTool_);
                    strokeShapeMode_ = shapeMode_;
                    strokeFillIndex_ = strokeButton_ == Qt::LeftButton
                                           ? secondaryIndex_
                                           : selectedIndex_;
                    beginStroke(commandText());
                    polygonPoints_.clear();
                    polygonActive_ = true;
                } else if (event->button() != strokeButton_) {
                    event->accept();
                    return;
                }
                if (polygonPoints_.empty() || polygonPoints_.back() != point) {
                    polygonPoints_.push_back(point);
                }
                lastImagePoint_ = point;
                reportPosition(point);
                viewport()->update();
                event->accept();
                return;
            }
            if (drawTool_ == DrawTool::Text) {
                if (textDraftActive_ && textBoxBounds_.contains(point)) {
                    movingTextBox_ = true;
                    strokeButton_ = event->button();
                    textMoveAnchor_ = point;
                    textMoveStart_ = textBoxBounds_.topLeft();
                    viewport()->setCursor(Qt::ClosedHandCursor);
                } else if (textDraftActive_) {
                    commitText();
                } else {
                    beginTextBox(point, event->button());
                }
                reportPosition(point);
                event->accept();
                return;
            }
            if (drawTool_ == DrawTool::MoveSelection) {
                if (event->button() != Qt::LeftButton) {
                    event->accept();
                    return;
                }
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

            if (isSelectionTool(drawTool_) &&
                event->button() != Qt::LeftButton) {
                event->accept();
                return;
            }
            if (isSelectionTool(drawTool_)) {
                const bool add =
                    event->modifiers() & Qt::ShiftModifier;
                const bool subtract =
                    event->modifiers() & Qt::ControlModifier;
                strokeSelectionCombineMode_ =
                    add && subtract
                        ? SelectionCombineMode::Intersect
                        : add ? SelectionCombineMode::Add
                              : subtract ? SelectionCombineMode::Subtract
                                         : SelectionCombineMode::Replace;
                if (selectionActive_ && selectionEditPending_) {
                    commitSelection();
                    strokeSelectionCombineMode_ =
                        SelectionCombineMode::Replace;
                } else if (selectionActive_ &&
                           strokeSelectionCombineMode_ ==
                               SelectionCombineMode::Replace) {
                    commitSelection();
                }
            }
            if (drawTool_ == DrawTool::BezierCurve &&
                curveStage_ != CurveStage::None) {
                if (event->button() != strokeButton_) {
                    event->accept();
                    return;
                }
                if (curveStage_ == CurveStage::Editing) {
                    const int hitRadius =
                        std::max(1, static_cast<int>(std::ceil(6.0 / zoom_)));
                    const auto distanceSquared = [](const QPoint& first,
                                                    const QPoint& second) {
                        const QPoint delta = first - second;
                        return delta.x() * delta.x() +
                               delta.y() * delta.y();
                    };
                    const int firstDistance =
                        distanceSquared(point, curveControl1_);
                    const int secondDistance =
                        distanceSquared(point, curveControl2_);
                    const int maximumDistance = hitRadius * hitRadius;
                    if (firstDistance <= maximumDistance &&
                        firstDistance <= secondDistance) {
                        curveHandle_ = CurveHandle::First;
                        curveHandleDragOffset_ = curveControl1_ - point;
                    } else if (secondDistance <= maximumDistance) {
                        curveHandle_ = CurveHandle::Second;
                        curveHandleDragOffset_ = curveControl2_ - point;
                    } else {
                        curveHandle_ = CurveHandle::None;
                        event->accept();
                        return;
                    }
                } else {
                    curveHandle_ = CurveHandle::None;
                    curveHandleDragOffset_ = {};
                }
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
            strokeButton_ = event->button();
            strokeTool_ = drawTool_;
            strokePaintIndex_ = paintIndex(strokeButton_);
            strokeThickness_ = toolThickness(strokeTool_);
            strokeCornerRadius_ = rectangleCornerRadius_;
            strokeShapeMode_ = shapeMode_;
            strokeFillIndex_ = strokeButton_ == Qt::LeftButton
                                   ? secondaryIndex_
                                   : selectedIndex_;

            if (strokeTool_ == DrawTool::Eyedropper) {
                if (strokeButton_ == Qt::LeftButton) {
                    setSelectedIndex(pixelAt(point.x(), point.y()));
                } else {
                    setSecondaryIndex(pixelAt(point.x(), point.y()));
                }
            } else if (strokeTool_ == DrawTool::FloodFill) {
                beginStroke(tr("Flood fill"));
                floodFill(point, strokePaintIndex_);
                commitStroke();
            } else if (strokeTool_ == DrawTool::MagicWand) {
                selectMatchingMaterial(point);
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
        hoverImagePoint_ = point;
        reportPosition(point);
        if (objectDragMode_ != ObjectDragMode::None &&
            (event->buttons() & Qt::LeftButton) &&
            selectedObject_ >= 0 && objects_ != nullptr) {
            if (objectDragMode_ == ObjectDragMode::Move) {
                const QPoint candidate{
                    objectTransformStart_.x +
                        point.x() - objectMoveAnchor_.x(),
                    objectTransformStart_.y +
                        point.y() - objectMoveAnchor_.y()};
                objectTransformPreview_.x = std::clamp(
                    candidate.x(), 0,
                    static_cast<int>(level_->width) -
                        objectTransformStart_.width);
                objectTransformPreview_.y = std::clamp(
                    candidate.y(), 0,
                    static_cast<int>(level_->height) -
                        objectTransformStart_.height);
            } else if (objectDragMode_ == ObjectDragMode::Rotate) {
                const double centerX = objectTransformStart_.x +
                                       objectTransformStart_.width / 2.0;
                const double centerY = objectTransformStart_.y +
                                       objectTransformStart_.height / 2.0;
                double degrees = std::atan2(point.y() - centerY,
                                            point.x() - centerX) *
                                     180.0 / 3.14159265358979323846 +
                                 90.0;
                while (degrees > 180.0) degrees -= 360.0;
                while (degrees <= -180.0) degrees += 360.0;
                objectTransformPreview_.rotationMillidegrees =
                    static_cast<int>(std::round(degrees * 1000.0));
            } else {
                const QPointF center(
                    objectTransformStart_.x +
                        objectTransformStart_.width / 2.0,
                    objectTransformStart_.y +
                        objectTransformStart_.height / 2.0);
                const QPointF local = rotatedPoint(
                    QPointF(point), center,
                    -objectTransformStart_.rotationMillidegrees / 1000.0);
                if (objectDragMode_ == ObjectDragMode::ResizeWidth ||
                    objectDragMode_ == ObjectDragMode::ResizeBoth) {
                    objectTransformPreview_.width = std::clamp(
                        static_cast<int>(std::round(local.x())) -
                            objectTransformStart_.x,
                        1, static_cast<int>(level_->width) -
                               objectTransformStart_.x);
                }
                if (objectDragMode_ == ObjectDragMode::ResizeHeight ||
                    objectDragMode_ == ObjectDragMode::ResizeBoth) {
                    objectTransformPreview_.height = std::clamp(
                        static_cast<int>(std::round(local.y())) -
                            objectTransformStart_.y,
                        1, static_cast<int>(level_->height) -
                               objectTransformStart_.y);
                }
            }
            viewport()->update();
            event->accept();
            return;
        }
        if (movingTextBox_ && (event->buttons() & strokeButton_)) {
            setTextBoxPosition(textMoveStart_ + (point - textMoveAnchor_));
            viewport()->update();
            return;
        }
        if (polygonActive_) {
            lastImagePoint_ = point;
            viewport()->update();
            return;
        }
        if (movingSelection_ && (event->buttons() & Qt::LeftButton)) {
            setSelectionPosition(selectionMoveStart_ +
                                 (point - selectionMoveAnchor_));
            viewport()->update();
            return;
        }
        if (drawing_ && (event->buttons() & strokeButton_) &&
            strokeTool_ == DrawTool::BezierCurve) {
            lastImagePoint_ =
                curveStage_ == CurveStage::Baseline &&
                        (event->modifiers() & Qt::ShiftModifier)
                    ? constrainedLinePoint(point)
                    : point;
            if (curveStage_ == CurveStage::Baseline) {
                curveEndPoint_ = lastImagePoint_;
                const QPoint delta = curveEndPoint_ - curveStartPoint_;
                curveControl1_ = curveStartPoint_ + delta / 3;
                curveControl2_ = curveStartPoint_ + delta * 2 / 3;
            } else if (curveStage_ == CurveStage::FirstControl) {
                curveControl1_ = point;
            } else if (curveStage_ == CurveStage::SecondControl) {
                curveControl2_ = point;
            } else if (curveStage_ == CurveStage::Editing) {
                if (curveHandle_ == CurveHandle::First) {
                    curveControl1_ = point + curveHandleDragOffset_;
                } else if (curveHandle_ == CurveHandle::Second) {
                    curveControl2_ = point + curveHandleDragOffset_;
                }
            }
            viewport()->update();
            return;
        }
        if (drawing_ && (event->buttons() & strokeButton_) &&
            (strokeTool_ == DrawTool::Pencil ||
             strokeTool_ == DrawTool::Eraser)) {
            drawLine(lastImagePoint_, point, strokePaintIndex_,
                     strokeThickness_);
            lastImagePoint_ = point;
            // The brush outline follows the cursor even when drawing does not
            // change any pixels (for example over the same index, or while
            // erasing an already empty area).
            viewport()->update();
        } else if (drawing_ && (event->buttons() & strokeButton_) &&
                   strokeTool_ == DrawTool::Spray) {
            sprayLine(lastImagePoint_, point, strokePaintIndex_,
                      strokeThickness_);
            lastImagePoint_ = point;
        } else if (drawing_ && (event->buttons() & strokeButton_) &&
                   strokeTool_ == DrawTool::SelectFreehand) {
            if (freehandSelectionPoints_.empty() ||
                freehandSelectionPoints_.back() != point) {
                freehandSelectionPoints_.push_back(point);
            }
            lastImagePoint_ = point;
            viewport()->update();
        } else if (drawing_ && (event->buttons() & strokeButton_)) {
            lastImagePoint_ = event->modifiers() & Qt::ShiftModifier
                                  ? constrainedShapePoint(point)
                                  : point;
            viewport()->update();
        } else {
            viewport()->update();
        }
    } else {
        hoverImagePoint_ = {-1, -1};
        viewport()->update();
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
    if (event->button() == Qt::LeftButton &&
        objectDragMode_ != ObjectDragMode::None) {
        objectDragMode_ = ObjectDragMode::None;
        viewport()->setCursor(Qt::CrossCursor);
        const bool changed =
            objectTransformPreview_.x != objectTransformStart_.x ||
            objectTransformPreview_.y != objectTransformStart_.y ||
            objectTransformPreview_.width != objectTransformStart_.width ||
            objectTransformPreview_.height != objectTransformStart_.height ||
            objectTransformPreview_.rotationMillidegrees !=
                objectTransformStart_.rotationMillidegrees;
        if (selectedObject_ >= 0 && changed) {
            emit objectTransformRequested(
                selectedObject_, objectTransformPreview_.x,
                objectTransformPreview_.y, objectTransformPreview_.width,
                objectTransformPreview_.height,
                objectTransformPreview_.rotationMillidegrees);
        }
        viewport()->update();
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton && movingSelection_) {
        movingSelection_ = false;
        updateToolCursor();
        viewport()->update();
        event->accept();
        return;
    }
    if (event->button() == strokeButton_ && movingTextBox_) {
        movingTextBox_ = false;
        updateToolCursor();
        viewport()->update();
        event->accept();
        return;
    }
    if (event->button() == strokeButton_) {
        const bool wasDrawing = drawing_;
        drawing_ = false;
        if (wasDrawing) {
            const QPoint releasePoint = imagePoint(event->position());
            if (releasePoint.x() >= 0) {
                hoverImagePoint_ = releasePoint;
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
                    curveStage_ = CurveStage::Editing;
                } else if (curveStage_ == CurveStage::Editing) {
                    if (curveHandle_ == CurveHandle::First) {
                        curveControl1_ = lastImagePoint_ +
                                         curveHandleDragOffset_;
                    } else if (curveHandle_ == CurveHandle::Second) {
                        curveControl2_ = lastImagePoint_ +
                                         curveHandleDragOffset_;
                    }
                }
                curveHandle_ = CurveHandle::None;
                viewport()->update();
                event->accept();
                return;
            }
            if (strokeTool_ == DrawTool::Text) {
                QRect bounds(strokeStartPoint_, lastImagePoint_);
                bounds = bounds.normalized();
                if (bounds.width() < 2 || bounds.height() < 2) {
                    const int defaultWidth = std::max(80, textPixelSize_ * 10);
                    const int defaultHeight = std::max(24, textPixelSize_ * 3);
                    bounds.setSize({defaultWidth, defaultHeight});
                }
                bounds.setRight(std::min(bounds.right(),
                                         static_cast<int>(level_->width) - 1));
                bounds.setBottom(std::min(bounds.bottom(),
                                          static_cast<int>(level_->height) - 1));
                textBoxBounds_ = bounds;
                textDraftActive_ = true;
                emit pendingSelectionEditChanged(true);
                viewport()->update();
                event->accept();
                return;
            }
            if (strokeTool_ == DrawTool::Line) {
                drawLine(strokeStartPoint_, lastImagePoint_, strokePaintIndex_,
                         strokeThickness_);
            } else if (strokeTool_ == DrawTool::Rectangle) {
                drawRectangle(strokeStartPoint_, lastImagePoint_,
                              strokePaintIndex_, strokeFillIndex_,
                              strokeThickness_, strokeCornerRadius_,
                              strokeShapeMode_);
            } else if (strokeTool_ == DrawTool::Ellipse) {
                drawEllipse(strokeStartPoint_, lastImagePoint_,
                            strokePaintIndex_, strokeFillIndex_,
                            strokeThickness_, strokeShapeMode_);
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

void LevelCanvas::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (teamBasePolygonActive_ && event->button() == Qt::LeftButton) {
        const QPoint point = imagePoint(event->position());
        if (point.x() >= 0 &&
            (teamBasePolygonPoints_.isEmpty() ||
             teamBasePolygonPoints_.back() != point)) {
            teamBasePolygonPoints_ << point;
        }
        commitTeamBasePolygon();
        event->accept();
        return;
    }
    if (event->button() == strokeButton_ &&
        drawTool_ == DrawTool::Polygon && polygonActive_) {
        const QPoint point = imagePoint(event->position());
        if (point.x() >= 0 &&
            (polygonPoints_.empty() || polygonPoints_.back() != point)) {
            polygonPoints_.push_back(point);
        }
        commitPolygon();
        event->accept();
        return;
    }
    QAbstractScrollArea::mouseDoubleClickEvent(event);
}

void LevelCanvas::wheelEvent(QWheelEvent* event)
{
    if (!(event->modifiers() & Qt::ControlModifier) ||
        event->angleDelta().y() == 0) {
        QAbstractScrollArea::wheelEvent(event);
        return;
    }

    const QPointF cursorImage{
        (event->position().x() + horizontalScrollBar()->value()) / zoom_,
        (event->position().y() + verticalScrollBar()->value()) / zoom_,
    };
    const double factor = event->angleDelta().y() > 0 ? 2.0 : 0.5;
    setZoom(zoom_ * factor);
    horizontalScrollBar()->setValue(static_cast<int>(std::round(
        cursorImage.x() * zoom_ - event->position().x())));
    verticalScrollBar()->setValue(static_cast<int>(std::round(
        cursorImage.y() * zoom_ - event->position().y())));
    event->accept();
}

void LevelCanvas::keyPressEvent(QKeyEvent* event)
{
    if (teamBasePolygonActive_) {
        if (event->key() == Qt::Key_Escape) {
            cancelTeamBasePolygon();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            commitTeamBasePolygon();
            event->accept();
            return;
        }
    }
    if (event->key() == Qt::Key_Escape && objectEditingEnabled_ &&
        selectedObject_ >= 0) {
        selectedObject_ = -1;
        objectDragMode_ = ObjectDragMode::None;
        emit objectSelectionChanged(-1);
        updateToolCursor();
        viewport()->update();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape &&
        (textDraftActive_ ||
         (drawing_ && strokeTool_ == DrawTool::Text))) {
        cancelText();
        event->accept();
        return;
    }
    if (textDraftActive_) {
        if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) &&
            (event->modifiers() & Qt::ControlModifier)) {
            commitText();
        } else if (event->matches(QKeySequence::Paste)) {
            textDraft_.append(QApplication::clipboard()->text());
            viewport()->update();
        } else if (event->matches(QKeySequence::Copy)) {
            QApplication::clipboard()->setText(textDraft_);
        } else if (event->key() == Qt::Key_Backspace) {
            textDraft_.chop(1);
            viewport()->update();
        } else if (event->key() == Qt::Key_Return ||
                   event->key() == Qt::Key_Enter) {
            textDraft_.append(QLatin1Char('\n'));
            viewport()->update();
        } else if (!(event->modifiers() &
                     (Qt::ControlModifier | Qt::AltModifier)) &&
                   !event->text().isEmpty()) {
            textDraft_.append(event->text());
            viewport()->update();
        }
        event->accept();
        return;
    }
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
        if (polygonActive_) {
            commitPolygon();
        }
        if (curveStage_ != CurveStage::None) {
            commitCurve();
        }
        commitSelection();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape &&
        (selectionActive_ || curveStage_ != CurveStage::None ||
         polygonActive_)) {
        cancelCurve();
        cancelPolygon();
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
    hoverImagePoint_ = {-1, -1};
    viewport()->update();
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
    if (x < 0 || y < 0 || x >= static_cast<int>(level_->width) ||
        y >= static_cast<int>(level_->height)) {
        return {-1, -1};
    }
    return {x, y};
}

bool LevelCanvas::setPixel(const int x, const int y, const std::uint8_t index)
{
    if (level_->layers[level_->activeLayer].locked) {
        return false;
    }
    if (selectionActive_) {
        if (!selectionContains(x, y)) {
            return false;
        }
        const int localX = x - selectionPosition_.x();
        const int localY = y - selectionPosition_.y();
        const std::size_t localOffset =
            static_cast<std::size_t>(localY) * selectionBounds_.width() +
            localX;
        const bool eraseToTransparency =
            strokeTool_ == DrawTool::Eraser && level_->activeLayer > 0;
        const std::uint8_t newMask = eraseToTransparency ? 0 : 1;
        if (selectionPixels_[localOffset] == index &&
            selectionOpacity_[localOffset] == newMask) {
            return false;
        }
        selectionPixels_[localOffset] = index;
        selectionOpacity_[localOffset] = newMask;
        if (!selectionEditPending_) {
            selectionEditPending_ = true;
            emit pendingSelectionEditChanged(true);
        }
        return true;
    }

    const std::size_t offset = static_cast<std::size_t>(y) * level_->width +
                               static_cast<std::size_t>(x);
    Level::Layer& layer = level_->layers[level_->activeLayer];
    const bool eraseToTransparency =
        strokeTool_ == DrawTool::Eraser && level_->activeLayer > 0;
    const std::uint8_t newMask = eraseToTransparency ? 0 : 1;
    const std::uint8_t newValue = eraseToTransparency ? layer.pixels[offset]
                                                       : index;
    if (layer.pixels[offset] == newValue && layer.mask[offset] == newMask) {
        return false;
    }
    const auto existing = strokeChangeIndices_.find(offset);
    if (existing == strokeChangeIndices_.end()) {
        strokeChangeIndices_.emplace(offset, strokeChanges_.size());
        strokeChanges_.push_back(PixelChange{
            offset,
            layer.pixels[offset],
            newValue,
            layer.mask[offset],
            newMask,
        });
    } else {
        strokeChanges_[existing->second].newValue = newValue;
        strokeChanges_[existing->second].newMask = newMask;
    }
    layer.pixels[offset] = newValue;
    layer.mask[offset] = newMask;
    recompositePixel(offset);
    return true;
}

void LevelCanvas::recompositePixel(const std::size_t offset)
{
    level_->pixels[offset] = compositeLayerPixel(*level_, offset);
}

bool LevelCanvas::setBrushPixel(const int x, const int y,
                                const std::uint8_t index, const int thickness)
{
    bool changed = false;
    rasterBrush(x, y, thickness, brushShape(strokeTool_),
                static_cast<int>(level_->width),
                static_cast<int>(level_->height),
                [&](const int brushX, const int brushY) {
                    changed |= setPixel(brushX, brushY, index);
                });
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
        if (selectionOpacity_[localOffset] != 0) {
            return selectionPixels_[localOffset];
        }
    }
    if (selectionActive_ && selectionHasSource_) {
        const int localX = x - selectionSourcePosition_.x();
        const int localY = y - selectionSourcePosition_.y();
        if (localX >= 0 && localY >= 0 && localX < selectionBounds_.width() &&
            localY < selectionBounds_.height() &&
            selectionMask_[static_cast<std::size_t>(localY) *
                               selectionBounds_.width() +
                           localX] != 0) {
            const std::size_t offset =
                static_cast<std::size_t>(y) * level_->width + x;
            return compositeLayerPixel(*level_, offset, level_->activeLayer);
        }
    }
    return level_->pixels[static_cast<std::size_t>(y) * level_->width + x];
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
            y >= 0 && x < static_cast<int>(level_->width) &&
            y < static_cast<int>(level_->height)) {
            setPixel(x, y, index);
        }
    }
}

void LevelCanvas::drawLine(const QPoint& from, const QPoint& to,
                           const std::uint8_t index, const int thickness)
{
    bool changed = false;
    rasterLine(from, to, thickness, brushShape(strokeTool_),
               static_cast<int>(level_->width),
               static_cast<int>(level_->height),
               [&](const int x, const int y) {
                   changed |= setPixel(x, y, index);
               });
    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::drawBezier(const QPoint& start, const QPoint& control1,
                             const QPoint& control2, const QPoint& end,
                             const std::uint8_t index, const int thickness)
{
    bool changed = false;
    rasterBezier(start, control1, control2, end, thickness,
                 static_cast<int>(level_->width),
                 static_cast<int>(level_->height),
                 [&](const int x, const int y) {
                     changed |= setPixel(x, y, index);
                 });
    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::drawRectangle(const QPoint& from, const QPoint& to,
                                const std::uint8_t outlineIndex,
                                const std::uint8_t fillIndex,
                                const int thickness, const int cornerRadius,
                                const ShapeMode mode)
{
    bool changed = false;
    rasterRectangle(from, to, outlineIndex, fillIndex, thickness, cornerRadius,
                    mode, [&](const int x, const int y,
                              const std::uint8_t index) {
                        changed |= setPixel(x, y, index);
                    });
    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::drawEllipse(const QPoint& from, const QPoint& to,
                              const std::uint8_t outlineIndex,
                              const std::uint8_t fillIndex,
                              const int thickness, const ShapeMode mode)
{
    bool changed = false;
    rasterEllipse(from, to, outlineIndex, fillIndex, thickness, mode,
                  static_cast<int>(level_->width),
                  static_cast<int>(level_->height),
                  [&](const int x, const int y,
                      const std::uint8_t index) {
                      changed |= setPixel(x, y, index);
                  });
    if (changed) {
        viewport()->update();
    }
}

void LevelCanvas::drawPolygon(const std::vector<QPoint>& points,
                              const std::uint8_t outlineIndex,
                              const std::uint8_t fillIndex,
                              const int thickness, const ShapeMode mode)
{
    if (points.size() < 3) {
        return;
    }

    if (mode != ShapeMode::Outline) {
        QPolygon polygon;
        polygon.reserve(static_cast<qsizetype>(points.size()));
        for (const QPoint& point : points) {
            polygon.append(point);
        }
        const QRect bounds = polygon.boundingRect().intersected(
            QRect(0, 0, static_cast<int>(level_->width),
                  static_cast<int>(level_->height)));
        if (!bounds.isEmpty()) {
            QImage mask(bounds.size(), QImage::Format_Grayscale8);
            mask.fill(0);
            QPainter painter(&mask);
            painter.setRenderHint(QPainter::Antialiasing, false);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::white);
            painter.translate(-bounds.topLeft());
            painter.drawPolygon(polygon);
            painter.end();
            for (int y = 0; y < bounds.height(); ++y) {
                for (int x = 0; x < bounds.width(); ++x) {
                    if (mask.constScanLine(y)[x] != 0) {
                        setPixel(bounds.left() + x, bounds.top() + y,
                                 fillIndex);
                    }
                }
            }
        }
    }

    if (mode != ShapeMode::FillOnly) {
        for (std::size_t point = 0; point < points.size(); ++point) {
            drawLine(points[point], points[(point + 1) % points.size()],
                     outlineIndex, thickness);
        }
    }
    viewport()->update();
}

void LevelCanvas::drawText(const QRect& bounds, const QString& text,
                           const std::uint8_t index)
{
    if (text.isEmpty() || bounds.isEmpty()) {
        return;
    }

    QFont font(textFontFamily_);
    font.setPixelSize(textPixelSize_);
    const int width = bounds.width();
    const int height = bounds.height();
    QImage mask(width, height, QImage::Format_Grayscale8);
    mask.fill(0);
    QPainter painter(&mask);
    painter.setRenderHint(QPainter::TextAntialiasing, false);
    painter.setFont(font);
    painter.setPen(Qt::white);
    painter.drawText(QRect(0, 0, width, height),
                     Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, text);
    painter.end();

    const int levelWidth = static_cast<int>(level_->width);
    const int levelHeight = static_cast<int>(level_->height);
    for (int y = 0; y < height && bounds.top() + y < levelHeight; ++y) {
        for (int x = 0; x < width && bounds.left() + x < levelWidth; ++x) {
            if (mask.constScanLine(y)[x] != 0) {
                setPixel(bounds.left() + x, bounds.top() + y, index);
            }
        }
    }
    viewport()->update();
}

void LevelCanvas::paintShapePreview(QPainter& painter) const
{
    if (level_ == nullptr) {
        return;
    }
    const auto paintRasterPreview =
        [this, &painter](const auto& rasterize) {
            QImage preview(static_cast<int>(level_->width),
                           static_cast<int>(level_->height),
                           QImage::Format_ARGB32_Premultiplied);
            preview.fill(Qt::transparent);
            std::vector<std::uint8_t> mask(level_->pixelCount(), 0);
            std::vector<QPoint> pixels;
            rasterize([this, &preview, &mask, &pixels](
                          const int x, const int y,
                          const std::uint8_t index) {
                if (selectionActive_ && !selectionContains(x, y)) {
                    return;
                }
                const RGB& rgb = level_->palette[index];
                preview.setPixelColor(x, y,
                                      QColor(rgb.r, rgb.g, rgb.b, 180));
                const std::size_t offset =
                    static_cast<std::size_t>(y) * level_->width + x;
                if (mask[offset] == 0) {
                    mask[offset] = 1;
                    pixels.emplace_back(x, y);
                }
            });
            painter.save();
            painter.setRenderHint(QPainter::SmoothPixmapTransform, false);
            painter.drawImage(
                QRectF(0, 0, level_->width * zoom_, level_->height * zoom_),
                preview);
            const auto occupied = [&mask, this](const int x, const int y) {
                return x >= 0 && y >= 0 &&
                       x < static_cast<int>(level_->width) &&
                       y < static_cast<int>(level_->height) &&
                       mask[static_cast<std::size_t>(y) * level_->width + x] !=
                           0;
            };
            QPainterPath edge;
            for (const QPoint& pixel : pixels) {
                const qreal left = pixel.x() * zoom_;
                const qreal top = pixel.y() * zoom_;
                const qreal right = left + zoom_;
                const qreal bottom = top + zoom_;
                if (!occupied(pixel.x(), pixel.y() - 1)) {
                    edge.moveTo(left, top);
                    edge.lineTo(right, top);
                }
                if (!occupied(pixel.x() + 1, pixel.y())) {
                    edge.moveTo(right, top);
                    edge.lineTo(right, bottom);
                }
                if (!occupied(pixel.x(), pixel.y() + 1)) {
                    edge.moveTo(left, bottom);
                    edge.lineTo(right, bottom);
                }
                if (!occupied(pixel.x() - 1, pixel.y())) {
                    edge.moveTo(left, top);
                    edge.lineTo(left, bottom);
                }
            }
            QColor edgeColor = palette().color(QPalette::BrightText);
            edgeColor.setAlpha(210);
            QPen edgePen(edgeColor, 1);
            edgePen.setCosmetic(true);
            painter.setPen(edgePen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(edge);
            painter.restore();
        };
    if (curveStage_ != CurveStage::None) {
        paintRasterPreview([this](const auto& plot) {
            rasterBezier(curveStartPoint_, curveControl1_, curveControl2_,
                         curveEndPoint_, strokeThickness_,
                         static_cast<int>(level_->width),
                         static_cast<int>(level_->height),
                         [&](const int x, const int y) {
                             plot(x, y, strokePaintIndex_);
                         });
        });
        const auto canvasPoint = [this](const QPoint& point) {
            return QPointF((point.x() + 0.5) * zoom_,
                           (point.y() + 0.5) * zoom_);
        };
        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, false);
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
    if (polygonActive_ && !polygonPoints_.empty()) {
        const RGB& outlineRgb = level_->palette[strokePaintIndex_];
        const RGB& fillRgb = level_->palette[strokeFillIndex_];
        const QColor outlineColor(outlineRgb.r, outlineRgb.g, outlineRgb.b);
        const QColor fillColor(fillRgb.r, fillRgb.g, fillRgb.b);
        QPolygonF polygon;
        polygon.reserve(static_cast<qsizetype>(polygonPoints_.size() + 1));
        for (const QPoint& point : polygonPoints_) {
            polygon.append(QPointF((point.x() + 0.5) * zoom_,
                                   (point.y() + 0.5) * zoom_));
        }
        polygon.append(QPointF((lastImagePoint_.x() + 0.5) * zoom_,
                               (lastImagePoint_.y() + 0.5) * zoom_));

        painter.save();
        painter.setRenderHint(QPainter::Antialiasing, false);
        painter.setPen(strokeShapeMode_ == ShapeMode::FillOnly
                           ? Qt::NoPen
                           : QPen(outlineColor, std::max<qreal>(
                                                    1.0, zoom_ * strokeThickness_)));
        painter.setBrush(strokeShapeMode_ != ShapeMode::Outline &&
                                 polygon.size() >= 3
                             ? fillColor
                             : Qt::NoBrush);
        painter.drawPolygon(polygon);
        QPen guidePen(palette().color(QPalette::BrightText), 1,
                      Qt::DashLine);
        guidePen.setCosmetic(true);
        painter.setPen(guidePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPolygon(polygon);
        painter.restore();
        return;
    }
    if (textDraftActive_ || (drawing_ && strokeTool_ == DrawTool::Text)) {
        const QRect imageBounds = textDraftActive_
                                      ? textBoxBounds_
                                      : QRect(strokeStartPoint_,
                                              lastImagePoint_)
                                            .normalized();
        const QRectF bounds(imageBounds.left() * zoom_,
                            imageBounds.top() * zoom_,
                            imageBounds.width() * zoom_,
                            imageBounds.height() * zoom_);
        painter.save();
        if (textDraftActive_) {
            const RGB& rgb = level_->palette[strokePaintIndex_];
            painter.setPen(QColor(rgb.r, rgb.g, rgb.b));
            QFont font(textFontFamily_);
            font.setPixelSize(
                std::max(1, static_cast<int>(std::round(textPixelSize_ * zoom_))));
            painter.setFont(font);
            painter.setRenderHint(QPainter::TextAntialiasing, false);
            painter.drawText(bounds,
                             Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap,
                             textDraft_);
        }
        QPen guidePen(palette().color(QPalette::BrightText), 1,
                      Qt::DashLine);
        guidePen.setCosmetic(true);
        painter.setPen(guidePen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(bounds);
        painter.restore();
        return;
    }
    if (!drawing_ ||
        (strokeTool_ != DrawTool::Line && strokeTool_ != DrawTool::Rectangle &&
         strokeTool_ != DrawTool::Ellipse &&
         !isSelectionTool(strokeTool_))) {
        return;
    }

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

    if (strokeTool_ == DrawTool::Line) {
        paintRasterPreview([this](const auto& plot) {
            rasterLine(strokeStartPoint_, lastImagePoint_, strokeThickness_,
                       BrushShape::Square, static_cast<int>(level_->width),
                       static_cast<int>(level_->height),
                       [&](const int x, const int y) {
                           plot(x, y, strokePaintIndex_);
                       });
        });
    } else if (strokeTool_ == DrawTool::Rectangle) {
        paintRasterPreview([this](const auto& plot) {
            rasterRectangle(strokeStartPoint_, lastImagePoint_,
                            strokePaintIndex_, strokeFillIndex_,
                            strokeThickness_, strokeCornerRadius_,
                            strokeShapeMode_, plot);
        });
    } else if (strokeTool_ == DrawTool::Ellipse) {
        paintRasterPreview([this](const auto& plot) {
            rasterEllipse(strokeStartPoint_, lastImagePoint_,
                          strokePaintIndex_, strokeFillIndex_,
                          strokeThickness_, strokeShapeMode_,
                          static_cast<int>(level_->width),
                          static_cast<int>(level_->height), plot);
        });
    }
}

void LevelCanvas::paintBrushPreview(QPainter& painter) const
{
    if (level_ == nullptr || hoverImagePoint_.x() < 0 || panning_ ||
        (drawTool_ != DrawTool::Pencil && drawTool_ != DrawTool::Eraser)) {
        return;
    }

    const int thickness = toolThickness(drawTool_);
    const BrushShape shape = brushShape(drawTool_);
    const std::uint8_t index =
        drawing_ ? strokePaintIndex_ : selectedIndex_;
    const RGB& rgb = level_->palette[index];
    QColor fillColor = drawTool_ == DrawTool::Eraser
                           ? QColor(255, 255, 255, 75)
                           : QColor(rgb.r, rgb.g, rgb.b, 105);
    QColor edgeColor = drawTool_ == DrawTool::Eraser
                           ? QColor(255, 80, 80, 220)
                           : palette().color(QPalette::BrightText);
    edgeColor.setAlpha(210);

    painter.save();
    painter.setRenderHint(QPainter::Antialiasing, false);
    QPen pixelPen(edgeColor, 1);
    pixelPen.setCosmetic(true);
    painter.setPen(pixelPen);
    painter.setBrush(fillColor);
    rasterBrush(hoverImagePoint_.x(), hoverImagePoint_.y(), thickness, shape,
                static_cast<int>(level_->width),
                static_cast<int>(level_->height),
                [this, &painter](const int x, const int y) {
                    painter.drawRect(QRectF(x * zoom_, y * zoom_, zoom_,
                                            zoom_));
                });
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

    const int width = bounds.width();
    const int height = bounds.height();
    std::vector<std::uint8_t> candidate(level_->pixelCount(), 0);

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
            const std::size_t levelOffset =
                static_cast<std::size_t>(bounds.top() + y) * level_->width +
                bounds.left() + x;
            candidate[levelOffset] = 1;
        }
    }

    freehandSelectionPoints_.clear();
    applySelectionCandidate(std::move(candidate));
}

void LevelCanvas::selectMatchingMaterial(const QPoint& point)
{
    if (level_ == nullptr || point.x() < 0 || point.y() < 0 ||
        point.x() >= static_cast<int>(level_->width) ||
        point.y() >= static_cast<int>(level_->height)) {
        return;
    }
    const Level::Layer& layer = level_->layers[level_->activeLayer];
    const std::size_t start = static_cast<std::size_t>(point.y()) *
                                  level_->width +
                              point.x();
    const std::uint8_t material = layer.pixels[start];
    const std::uint8_t opacity = layer.mask[start];
    std::vector<std::uint8_t> candidate(level_->pixelCount(), 0);
    const auto matches = [&](const std::size_t offset) {
        return layer.pixels[offset] == material &&
               layer.mask[offset] == opacity;
    };
    if (materialSelectionMode_ == MaterialSelectionMode::AllMatching) {
        for (std::size_t offset = 0; offset < candidate.size(); ++offset) {
            candidate[offset] = matches(offset) ? 1 : 0;
        }
    } else {
        std::vector<QPoint> pending;
        pending.reserve(level_->pixelCount());
        pending.push_back(point);
        candidate[start] = 1;
        for (std::size_t next = 0; next < pending.size(); ++next) {
            const QPoint current = pending[next];
            const std::array<QPoint, 4> neighbors{{
                {current.x() - 1, current.y()},
                {current.x() + 1, current.y()},
                {current.x(), current.y() - 1},
                {current.x(), current.y() + 1},
            }};
            for (const QPoint& neighbor : neighbors) {
                if (neighbor.x() < 0 || neighbor.y() < 0 ||
                    neighbor.x() >= static_cast<int>(level_->width) ||
                    neighbor.y() >= static_cast<int>(level_->height)) {
                    continue;
                }
                const std::size_t offset =
                    static_cast<std::size_t>(neighbor.y()) * level_->width +
                    neighbor.x();
                if (candidate[offset] == 0 && matches(offset)) {
                    candidate[offset] = 1;
                    pending.push_back(neighbor);
                }
            }
        }
    }
    applySelectionCandidate(std::move(candidate));
}

void LevelCanvas::applySelectionCandidate(
    std::vector<std::uint8_t> candidate)
{
    if (strokeSelectionCombineMode_ != SelectionCombineMode::Replace) {
        std::vector<std::uint8_t> previous(level_->pixelCount(), 0);
        if (selectionActive_) {
            const int previousWidth = selectionBounds_.width();
            const int previousHeight = selectionBounds_.height();
            for (int y = 0; y < previousHeight; ++y) {
                for (int x = 0; x < previousWidth; ++x) {
                    const std::size_t localOffset =
                        static_cast<std::size_t>(y) * previousWidth + x;
                    if (selectionMask_[localOffset] != 0) {
                        const QPoint global =
                            selectionPosition_ + QPoint(x, y);
                        previous[static_cast<std::size_t>(global.y()) *
                                     level_->width +
                                 global.x()] = 1;
                    }
                }
            }
        }
        for (std::size_t offset = 0; offset < level_->pixelCount(); ++offset) {
            switch (strokeSelectionCombineMode_) {
            case SelectionCombineMode::Add:
                candidate[offset] = previous[offset] || candidate[offset];
                break;
            case SelectionCombineMode::Subtract:
                candidate[offset] = previous[offset] && !candidate[offset];
                break;
            case SelectionCombineMode::Intersect:
                candidate[offset] = previous[offset] && candidate[offset];
                break;
            case SelectionCombineMode::Replace:
                break;
            }
        }
    }

    int left = static_cast<int>(level_->width);
    int right = -1;
    int top = static_cast<int>(level_->height);
    int bottom = -1;
    for (int y = 0; y < static_cast<int>(level_->height); ++y) {
        for (int x = 0; x < static_cast<int>(level_->width); ++x) {
            if (candidate[static_cast<std::size_t>(y) * level_->width + x] !=
                0) {
                left = std::min(left, x);
                right = std::max(right, x);
                top = std::min(top, y);
                bottom = std::max(bottom, y);
            }
        }
    }

    if (right < left || bottom < top) {
        clearSelection();
        return;
    }

    const QRect combinedBounds(QPoint(left, top), QPoint(right, bottom));
    selectionBounds_ = QRect(QPoint(0, 0), combinedBounds.size());
    selectionSourcePosition_ = combinedBounds.topLeft();
    selectionPosition_ = combinedBounds.topLeft();
    const int combinedWidth = combinedBounds.width();
    const int combinedHeight = combinedBounds.height();
    const std::size_t size =
        static_cast<std::size_t>(combinedWidth) * combinedHeight;
    selectionMask_.assign(size, 0);
    selectionPixels_.assign(size, 0);
    selectionOpacity_.assign(size, 0);
    const Level::Layer& layer = level_->layers[level_->activeLayer];
    for (int y = 0; y < combinedHeight; ++y) {
        for (int x = 0; x < combinedWidth; ++x) {
            const std::size_t levelOffset =
                static_cast<std::size_t>(top + y) * level_->width + left + x;
            if (candidate[levelOffset] == 0) {
                continue;
            }
            const std::size_t localOffset =
                static_cast<std::size_t>(y) * combinedWidth + x;
            selectionMask_[localOffset] = 1;
            selectionPixels_[localOffset] = layer.pixels[levelOffset];
            selectionOpacity_[localOffset] = layer.mask[levelOffset];
        }
    }
    selectionActive_ = true;
    selectionHasSource_ = true;
    selectionEditPending_ = false;
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
    selectionOpacity_.clear();
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
        static_cast<int>(level_->width) - selectionBounds_.width();
    const int maximumY =
        static_cast<int>(level_->height) - selectionBounds_.height();
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
           tool == DrawTool::SelectEllipse || tool == DrawTool::SelectFreehand ||
           tool == DrawTool::MagicWand;
}

void LevelCanvas::updateToolCursor()
{
    if (viewport() == nullptr || panning_ || movingSelection_) {
        return;
    }
    if (drawTool_ == DrawTool::MoveSelection) {
        viewport()->setCursor(Qt::OpenHandCursor);
    } else if (drawTool_ == DrawTool::Text) {
        viewport()->setCursor(Qt::IBeamCursor);
    } else {
        viewport()->setCursor(Qt::CrossCursor);
    }
}

void LevelCanvas::cancelCurve()
{
    if (curveStage_ == CurveStage::None) {
        return;
    }
    curveStage_ = CurveStage::None;
    curveHandle_ = CurveHandle::None;
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
    curveHandle_ = CurveHandle::None;
    drawing_ = false;
    drawBezier(curveStartPoint_, curveControl1_, curveControl2_, curveEndPoint_,
               strokePaintIndex_, strokeThickness_);
    commitStroke();
    viewport()->update();
}

void LevelCanvas::cancelPolygon()
{
    if (!polygonActive_) {
        return;
    }
    polygonActive_ = false;
    polygonPoints_.clear();
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
    viewport()->update();
}

void LevelCanvas::commitPolygon()
{
    if (!polygonActive_ || level_ == nullptr) {
        return;
    }
    polygonActive_ = false;
    if (polygonPoints_.size() >= 3) {
        drawPolygon(polygonPoints_, strokePaintIndex_, strokeFillIndex_,
                    strokeThickness_, strokeShapeMode_);
        commitStroke();
    } else {
        strokeChanges_.clear();
        strokeChangeIndices_.clear();
    }
    polygonPoints_.clear();
    viewport()->update();
}

void LevelCanvas::cancelText()
{
    if (!textDraftActive_ && !(drawing_ && strokeTool_ == DrawTool::Text)) {
        return;
    }
    textDraftActive_ = false;
    movingTextBox_ = false;
    drawing_ = false;
    textBoxBounds_ = {};
    textDraft_.clear();
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
    updateToolCursor();
    emit pendingSelectionEditChanged(selectionEditPending_);
    viewport()->update();
}

void LevelCanvas::commitText()
{
    if (!textDraftActive_ || level_ == nullptr) {
        return;
    }
    textDraftActive_ = false;
    movingTextBox_ = false;
    drawText(textBoxBounds_, textDraft_, strokePaintIndex_);
    textBoxBounds_ = {};
    textDraft_.clear();
    commitStroke();
    updateToolCursor();
    emit pendingSelectionEditChanged(selectionEditPending_);
    viewport()->update();
}

void LevelCanvas::beginTextBox(const QPoint& point,
                               const Qt::MouseButton button)
{
    strokeButton_ = button;
    strokeTool_ = DrawTool::Text;
    strokePaintIndex_ = paintIndex(button);
    strokeStartPoint_ = point;
    lastImagePoint_ = point;
    textDraft_.clear();
    drawing_ = true;
    beginStroke(commandText());
    viewport()->update();
}

void LevelCanvas::setTextBoxPosition(const QPoint& position)
{
    const int maximumX =
        static_cast<int>(level_->width) - textBoxBounds_.width();
    const int maximumY =
        static_cast<int>(level_->height) - textBoxBounds_.height();
    textBoxBounds_.moveTopLeft(
        {std::clamp(position.x(), 0, maximumX),
         std::clamp(position.y(), 0, maximumY)});
}

void LevelCanvas::floodFill(const QPoint& point, const std::uint8_t index)
{
    if (level_->layers[level_->activeLayer].locked ||
        (selectionActive_ && !selectionContains(point.x(), point.y()))) {
        return;
    }
    const auto activeValueAt = [&](const int x, const int y) {
        if (selectionContains(x, y)) {
            const int localX = x - selectionPosition_.x();
            const int localY = y - selectionPosition_.y();
            const std::size_t localOffset =
                static_cast<std::size_t>(localY) * selectionBounds_.width() +
                localX;
            return std::pair{selectionPixels_[localOffset],
                             selectionOpacity_[localOffset]};
        }
        const std::size_t offset = static_cast<std::size_t>(y) * level_->width +
                                   static_cast<std::size_t>(x);
        const Level::Layer& layer = level_->layers[level_->activeLayer];
        return std::pair{layer.pixels[offset], layer.mask[offset]};
    };
    const auto target = activeValueAt(point.x(), point.y());
    if (target.first == index && target.second == 1) {
        return;
    }

    std::vector<std::uint8_t> visited(level_->pixelCount(), 0);
    std::vector<QPoint> pending{point};
    visited[static_cast<std::size_t>(point.y()) * level_->width + point.x()] = 1;
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
                neighbour.x() >= static_cast<int>(level_->width) ||
                neighbour.y() >= static_cast<int>(level_->height) ||
                (selectionActive_ &&
                 !selectionContains(neighbour.x(), neighbour.y()))) {
                continue;
            }
            const std::size_t neighbourOffset =
                static_cast<std::size_t>(neighbour.y()) * level_->width +
                neighbour.x();
            if (visited[neighbourOffset] == 0 &&
                activeValueAt(neighbour.x(), neighbour.y()) == target) {
                visited[neighbourOffset] = 1;
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
    strokeLayerIndex_ = level_ == nullptr ? 0 : level_->activeLayer;
}

void LevelCanvas::commitStroke()
{
    if (!strokeChanges_.empty()) {
        undoStack_->push(new PixelEditCommand(level_, std::move(strokeChanges_),
                                             strokeLayerIndex_, this,
                                             strokeCommandText_));
    }
    strokeChanges_.clear();
    strokeChangeIndices_.clear();
}

std::uint8_t LevelCanvas::paintIndex(const Qt::MouseButton button) const
{
    if (drawTool_ == DrawTool::Eraser) {
        return 0;
    }
    return button == Qt::RightButton ? secondaryIndex_ : selectedIndex_;
}

QPoint LevelCanvas::constrainedShapePoint(const QPoint& point) const
{
    if (strokeTool_ == DrawTool::Line ||
        (strokeTool_ == DrawTool::BezierCurve &&
         curveStage_ == CurveStage::Baseline)) {
        return constrainedLinePoint(point);
    }

    const bool constrain = strokeTool_ == DrawTool::Rectangle ||
                           strokeTool_ == DrawTool::Ellipse;
    if (!constrain) {
        return point;
    }

    const int deltaX = point.x() - strokeStartPoint_.x();
    const int deltaY = point.y() - strokeStartPoint_.y();
    const int directionX = deltaX < 0 ? -1 : 1;
    const int directionY = deltaY < 0 ? -1 : 1;
    const int availableX = directionX < 0 ? strokeStartPoint_.x()
                                          : static_cast<int>(level_->width) - 1 -
                                                strokeStartPoint_.x();
    const int availableY = directionY < 0 ? strokeStartPoint_.y()
                                          : static_cast<int>(level_->height) -
                                                1 - strokeStartPoint_.y();
    const int side = std::min(
        {std::max(std::abs(deltaX), std::abs(deltaY)), availableX, availableY});
    return {strokeStartPoint_.x() + directionX * side,
            strokeStartPoint_.y() + directionY * side};
}

QPoint LevelCanvas::constrainedLinePoint(const QPoint& point) const
{
    const int deltaX = point.x() - strokeStartPoint_.x();
    const int deltaY = point.y() - strokeStartPoint_.y();
    const int absoluteX = std::abs(deltaX);
    const int absoluteY = std::abs(deltaY);
    if (absoluteX == 0 && absoluteY == 0) {
        return strokeStartPoint_;
    }

    // tan(22.5 degrees): the boundary halfway between an axis and a diagonal.
    constexpr double AxisThreshold = 0.4142135623730951;
    if (absoluteY <= absoluteX * AxisThreshold) {
        return {point.x(), strokeStartPoint_.y()};
    }
    if (absoluteX <= absoluteY * AxisThreshold) {
        return {strokeStartPoint_.x(), point.y()};
    }

    // Orthogonally project the cursor onto the nearest 45-degree diagonal.
    const int distance = (absoluteX + absoluteY + 1) / 2;
    return {strokeStartPoint_.x() + (deltaX < 0 ? -distance : distance),
            strokeStartPoint_.y() + (deltaY < 0 ? -distance : distance)};
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
        return strokeShapeMode_ == ShapeMode::Outline
                   ? tr("Rectangle")
                   : tr("Filled rectangle");
    case DrawTool::FloodFill:
        return tr("Flood fill");
    case DrawTool::Eyedropper:
        return tr("Eyedropper");
    case DrawTool::Ellipse:
        return strokeShapeMode_ == ShapeMode::Outline ? tr("Ellipse")
                                                      : tr("Filled ellipse");
    case DrawTool::Spray:
        return tr("Spray stroke");
    case DrawTool::Text:
        return tr("Text");
    case DrawTool::Polygon:
        return strokeShapeMode_ == ShapeMode::Outline ? tr("Polygon")
                                                      : tr("Filled polygon");
    case DrawTool::SelectRectangle:
        return tr("Rectangle selection");
    case DrawTool::SelectEllipse:
        return tr("Ellipse selection");
    case DrawTool::SelectFreehand:
        return tr("Freehand selection");
    case DrawTool::MagicWand:
        return tr("Magic wand selection");
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
                          : static_cast<int>(std::ceil(level_->width * zoom_));
    const int contentHeight =
        level_ == nullptr ? 0
                          : static_cast<int>(std::ceil(level_->height * zoom_));
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
