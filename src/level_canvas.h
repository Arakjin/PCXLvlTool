#pragma once

#include "level.h"

#include <QAbstractScrollArea>
#include <QPoint>
#include <QString>
#include <QUndoStack>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

struct PixelChange {
    std::size_t offset;
    std::uint8_t oldValue;
    std::uint8_t newValue;
};

enum class DrawTool {
    Pencil,
    Eraser,
    Line,
    Rectangle,
    FilledRectangle,
    FloodFill,
    Eyedropper,
};

class LevelCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit LevelCanvas(QWidget* parent = nullptr);

    void setLevel(Level* level);
    void setZoom(double zoom);
    void setSelectedIndex(std::uint8_t index);
    void setDrawTool(DrawTool tool);
    QUndoStack* undoStack();
    void refreshImage();

signals:
    void cursorPositionChanged(int x, int y, int index);
    void cursorLeftCanvas();
    void selectedIndexChanged(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPoint imagePoint(const QPointF& viewportPoint) const;
    bool setPixel(int x, int y, std::uint8_t index);
    void drawLine(const QPoint& from, const QPoint& to, std::uint8_t index);
    void drawRectangle(const QPoint& from, const QPoint& to,
                       std::uint8_t index, bool filled);
    void floodFill(const QPoint& point, std::uint8_t index);
    void beginStroke(const QString& commandText);
    void commitStroke();
    std::uint8_t paintIndex() const;
    QString commandText() const;
    void updateScrollBars();
    void reportPosition(const QPoint& point);

    Level* level_ = nullptr;
    double zoom_ = 1.0;
    std::uint8_t selectedIndex_ = 57;
    DrawTool drawTool_ = DrawTool::Pencil;
    std::uint8_t strokePaintIndex_ = 57;
    DrawTool strokeTool_ = DrawTool::Pencil;
    bool drawing_ = false;
    bool panning_ = false;
    QPoint lastImagePoint_;
    QPoint strokeStartPoint_;
    QString strokeCommandText_;
    QPoint lastPanPoint_;
    QUndoStack undoStack_;
    std::vector<PixelChange> strokeChanges_;
    std::unordered_map<std::size_t, std::size_t> strokeChangeIndices_;
};
