#pragma once

#include "level.h"

#include <QAbstractScrollArea>
#include <QPoint>

#include <cstdint>

class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

class LevelCanvas final : public QAbstractScrollArea {
    Q_OBJECT

public:
    explicit LevelCanvas(QWidget* parent = nullptr);

    void setLevel(Level* level);
    void setZoom(double zoom);
    void setSelectedIndex(std::uint8_t index);

signals:
    void cursorPositionChanged(int x, int y, int index);
    void cursorLeftCanvas();
    void levelEdited();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QPoint imagePoint(const QPointF& viewportPoint) const;
    bool setPixel(int x, int y);
    void drawLine(const QPoint& from, const QPoint& to);
    void updateScrollBars();
    void reportPosition(const QPoint& point);

    Level* level_ = nullptr;
    double zoom_ = 1.0;
    std::uint8_t selectedIndex_ = 57;
    bool drawing_ = false;
    bool panning_ = false;
    QPoint lastImagePoint_;
    QPoint lastPanPoint_;
};
