#pragma once

#include "level.h"

#include <QAbstractScrollArea>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QUndoStack>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class QMouseEvent;
class QKeyEvent;
class QPainter;
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
    Ellipse,
    FilledEllipse,
    Spray,
    SelectRectangle,
    SelectEllipse,
    SelectFreehand,
    MoveSelection,
    BezierCurve,
};

class LevelCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit LevelCanvas(QWidget* parent = nullptr);

    void setLevel(Level* level);
    void setZoom(double zoom);
    void setSelectedIndex(std::uint8_t index);
    void setDrawTool(DrawTool tool);
    void setToolThickness(DrawTool tool, int thickness);
    int toolThickness(DrawTool tool) const;
    void setRectangleCornerRadius(int radius);
    int rectangleCornerRadius() const;
    void setPaletteColor(std::uint8_t index, RGB color);
    void applyPaletteColor(std::uint8_t index, RGB color);
    void setPalette(const std::array<RGB, 256>& palette);
    void applyPalette(const std::array<RGB, 256>& palette);
    void commitSelection();
    void deleteSelection();
    void copySelection();
    void pasteSelection();
    bool hasSelection() const;
    bool hasPendingSelectionEdit() const;
    QUndoStack* undoStack();
    void refreshImage();

signals:
    void cursorPositionChanged(int x, int y, int index);
    void cursorLeftCanvas();
    void selectedIndexChanged(int index);
    void paletteColorChanged(int index);
    void pendingSelectionEditChanged(bool pending);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class CurveStage {
        None,
        Baseline,
        FirstControl,
        SecondControl,
    };

    QPoint imagePoint(const QPointF& viewportPoint) const;
    bool setPixel(int x, int y, std::uint8_t index);
    bool setBrushPixel(int x, int y, std::uint8_t index, int thickness);
    std::uint8_t pixelAt(int x, int y) const;
    bool selectionContains(int x, int y) const;
    void drawLine(const QPoint& from, const QPoint& to, std::uint8_t index,
                  int thickness);
    void drawBezier(const QPoint& start, const QPoint& control1,
                    const QPoint& control2, const QPoint& end,
                    std::uint8_t index, int thickness);
    void sprayLine(const QPoint& from, const QPoint& to, std::uint8_t index,
                   int radius);
    void sprayAt(const QPoint& point, std::uint8_t index, int radius);
    void drawRectangle(const QPoint& from, const QPoint& to, std::uint8_t index,
                       int thickness, int cornerRadius, bool filled);
    void drawEllipse(const QPoint& from, const QPoint& to, std::uint8_t index,
                     int thickness, bool filled);
    void floodFill(const QPoint& point, std::uint8_t index);
    void paintShapePreview(QPainter& painter) const;
    void paintSelection(QPainter& painter) const;
    void createSelection();
    void clearSelection();
    void setSelectionPosition(const QPoint& position);
    bool isSelectionTool(DrawTool tool) const;
    void updateToolCursor();
    void cancelCurve();
    void commitCurve();
    void beginStroke(const QString& commandText);
    void commitStroke();
    std::uint8_t paintIndex() const;
    QPoint constrainedShapePoint(const QPoint& point) const;
    QString commandText() const;
    void updateScrollBars();
    void reportPosition(const QPoint& point);

    Level* level_ = nullptr;
    double zoom_ = 1.0;
    std::uint8_t selectedIndex_ = 57;
    DrawTool drawTool_ = DrawTool::Pencil;
    std::uint8_t strokePaintIndex_ = 57;
    DrawTool strokeTool_ = DrawTool::Pencil;
    int strokeThickness_ = 1;
    int pencilThickness_ = 1;
    int eraserThickness_ = 1;
    int lineThickness_ = 1;
    int rectangleThickness_ = 1;
    int ellipseThickness_ = 1;
    int sprayThickness_ = 8;
    int curveThickness_ = 1;
    int rectangleCornerRadius_ = 0;
    int strokeCornerRadius_ = 0;
    bool drawing_ = false;
    bool panning_ = false;
    bool movingSelection_ = false;
    CurveStage curveStage_ = CurveStage::None;
    QPoint lastImagePoint_;
    QPoint strokeStartPoint_;
    QString strokeCommandText_;
    QPoint lastPanPoint_;
    QPoint selectionMoveAnchor_;
    QPoint selectionMoveStart_;
    QPoint selectionSourcePosition_;
    QPoint selectionPosition_;
    QPoint curveStartPoint_;
    QPoint curveEndPoint_;
    QPoint curveControl1_;
    QPoint curveControl2_;
    QRect selectionBounds_;
    bool selectionActive_ = false;
    bool selectionHasSource_ = false;
    bool selectionEditPending_ = false;
    std::vector<std::uint8_t> selectionMask_;
    std::vector<std::uint8_t> selectionPixels_;
    std::vector<QPoint> freehandSelectionPoints_;
    QUndoStack undoStack_;
    std::vector<PixelChange> strokeChanges_;
    std::unordered_map<std::size_t, std::size_t> strokeChangeIndices_;
};
