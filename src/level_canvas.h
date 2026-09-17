#pragma once

#include "level.h"
#include "game_profile.h"

#include <QAbstractScrollArea>
#include <QPoint>
#include <QRect>
#include <QString>
#include <QUndoStack>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

class QMouseEvent;
class QKeyEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

struct PixelChange {
    std::size_t offset;
    std::uint8_t oldValue;
    std::uint8_t newValue;
    std::uint8_t oldMask = 1;
    std::uint8_t newMask = 1;
};

enum class DrawTool {
    Pencil,
    Eraser,
    Line,
    Rectangle,
    FloodFill,
    Eyedropper,
    Ellipse,
    Spray,
    Text,
    Polygon,
    SelectRectangle,
    SelectEllipse,
    SelectFreehand,
    MoveSelection,
    BezierCurve,
};

enum class ShapeMode {
    Outline,
    OutlineAndFill,
    FillOnly,
};

enum class BrushShape {
    Square,
    Circle,
};

class LevelCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit LevelCanvas(QWidget* parent = nullptr);

    void setLevel(Level* level);
    void setGame(GameId game);
    void setZoom(double zoom);
    double zoom() const;
    void setSelectedIndex(std::uint8_t index);
    void setSecondaryIndex(std::uint8_t index);
    void setDrawTool(DrawTool tool);
    void setToolThickness(DrawTool tool, int thickness);
    int toolThickness(DrawTool tool) const;
    void setBrushShape(DrawTool tool, BrushShape shape);
    BrushShape brushShape(DrawTool tool) const;
    void setRectangleCornerRadius(int radius);
    int rectangleCornerRadius() const;
    void setShapeMode(ShapeMode mode);
    ShapeMode shapeMode() const;
    void setTextFontFamily(const QString& family);
    void setTextPixelSize(int size);
    void setPaletteColor(std::uint8_t index, RGB color);
    void applyPaletteColor(std::uint8_t index, RGB color);
    void setPalette(const std::array<RGB, 256>& palette);
    void applyPalette(const std::array<RGB, 256>& palette);
    void commitSelection();
    void deleteSelection();
    void copySelection();
    void pasteSelection();
    void selectAll();
    bool hasSelection() const;
    bool hasPendingSelectionEdit() const;
    int layerCount() const;
    int activeLayerIndex() const;
    bool addLayer();
    bool deleteActiveLayer();
    bool duplicateActiveLayer();
    bool moveActiveLayer(int direction);
    void setActiveLayer(int index);
    void setLayerVisible(int index, bool visible);
    void setLayerLocked(int index, bool locked);
    void renameLayer(int index, const QString& name);
    QUndoStack* undoStack();
    void forgetLevel(Level* level);
    bool hasDirtyUndoStack() const;
    void markAllUndoStacksClean();
    void refreshImage();

signals:
    void zoomChanged(double zoom);
    void cursorPositionChanged(int x, int y, int index);
    void cursorLeftCanvas();
    void selectedIndexChanged(int index);
    void secondaryIndexChanged(int index);
    void paletteColorChanged(int index);
    void pendingSelectionEditChanged(bool pending);
    void layersChanged();
    void undoCleanChanged(bool clean);

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    enum class CurveStage {
        None,
        Baseline,
        FirstControl,
        SecondControl,
        Editing,
    };

    enum class CurveHandle {
        None,
        First,
        Second,
    };

    enum class SelectionCombineMode {
        Replace,
        Add,
        Subtract,
        Intersect,
    };

    QPoint imagePoint(const QPointF& viewportPoint) const;
    bool setPixel(int x, int y, std::uint8_t index);
    void recompositePixel(std::size_t offset);
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
    void drawRectangle(const QPoint& from, const QPoint& to,
                       std::uint8_t outlineIndex, std::uint8_t fillIndex,
                       int thickness, int cornerRadius, ShapeMode mode);
    void drawEllipse(const QPoint& from, const QPoint& to,
                     std::uint8_t outlineIndex, std::uint8_t fillIndex,
                     int thickness, ShapeMode mode);
    void drawPolygon(const std::vector<QPoint>& points,
                     std::uint8_t outlineIndex, std::uint8_t fillIndex,
                     int thickness, ShapeMode mode);
    void drawText(const QRect& bounds, const QString& text,
                  std::uint8_t index);
    void floodFill(const QPoint& point, std::uint8_t index);
    void paintShapePreview(QPainter& painter) const;
    void paintBrushPreview(QPainter& painter) const;
    void paintSelection(QPainter& painter) const;
    void createSelection();
    void clearSelection();
    void setSelectionPosition(const QPoint& position);
    bool isSelectionTool(DrawTool tool) const;
    void updateToolCursor();
    void cancelCurve();
    void commitCurve();
    void cancelPolygon();
    void commitPolygon();
    void cancelText();
    void commitText();
    void beginTextBox(const QPoint& point, Qt::MouseButton button);
    void setTextBoxPosition(const QPoint& position);
    void beginStroke(const QString& commandText);
    void commitStroke();
    std::uint8_t paintIndex(Qt::MouseButton button) const;
    QPoint constrainedLinePoint(const QPoint& point) const;
    QPoint constrainedShapePoint(const QPoint& point) const;
    QString commandText() const;
    void updateScrollBars();
    void reportPosition(const QPoint& point);

    Level* level_ = nullptr;
    GameId game_ = GameId::VWing;
    double zoom_ = 1.0;
    std::uint8_t selectedIndex_ = 56;
    std::uint8_t secondaryIndex_ = 57;
    DrawTool drawTool_ = DrawTool::Pencil;
    std::uint8_t strokePaintIndex_ = 57;
    DrawTool strokeTool_ = DrawTool::Pencil;
    int strokeThickness_ = 1;
    int pencilThickness_ = 1;
    int eraserThickness_ = 1;
    BrushShape pencilBrushShape_ = BrushShape::Square;
    BrushShape eraserBrushShape_ = BrushShape::Square;
    int lineThickness_ = 1;
    int rectangleThickness_ = 1;
    int ellipseThickness_ = 1;
    int sprayThickness_ = 8;
    double sprayDistanceRemainder_ = 0.0;
    int curveThickness_ = 1;
    int polygonThickness_ = 1;
    int rectangleCornerRadius_ = 0;
    int strokeCornerRadius_ = 0;
    ShapeMode shapeMode_ = ShapeMode::Outline;
    ShapeMode strokeShapeMode_ = ShapeMode::Outline;
    SelectionCombineMode strokeSelectionCombineMode_ =
        SelectionCombineMode::Replace;
    std::uint8_t strokeFillIndex_ = 58;
    QString textFontFamily_;
    int textPixelSize_ = 12;
    bool drawing_ = false;
    Qt::MouseButton strokeButton_ = Qt::LeftButton;
    bool panning_ = false;
    bool movingSelection_ = false;
    bool textDraftActive_ = false;
    bool movingTextBox_ = false;
    CurveStage curveStage_ = CurveStage::None;
    CurveHandle curveHandle_ = CurveHandle::None;
    QPoint lastImagePoint_;
    QPoint hoverImagePoint_{-1, -1};
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
    QPoint curveHandleDragOffset_;
    QRect textBoxBounds_;
    QPoint textMoveAnchor_;
    QPoint textMoveStart_;
    QString textDraft_;
    bool polygonActive_ = false;
    std::vector<QPoint> polygonPoints_;
    QRect selectionBounds_;
    bool selectionActive_ = false;
    bool selectionHasSource_ = false;
    bool selectionEditPending_ = false;
    std::vector<std::uint8_t> selectionMask_;
    std::vector<std::uint8_t> selectionPixels_;
    std::vector<std::uint8_t> selectionOpacity_;
    std::vector<QPoint> freehandSelectionPoints_;
    QUndoStack fallbackUndoStack_;
    QUndoStack* undoStack_ = &fallbackUndoStack_;
    std::unordered_map<Level*, std::unique_ptr<QUndoStack>> undoStacks_;
    std::vector<PixelChange> strokeChanges_;
    std::size_t strokeLayerIndex_ = 0;
    std::unordered_map<std::size_t, std::size_t> strokeChangeIndices_;
};
