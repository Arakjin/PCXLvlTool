#include "palette_widget.h"

#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <numeric>
#include <utility>

PaletteWidget::PaletteWidget(QWidget* parent) : QWidget(parent)
{
    indices_.resize(256);
    std::iota(indices_.begin(), indices_.end(), std::uint8_t{0});
    setFixedSize(Columns * CellSize + 2 * Margin,
                 Columns * CellSize + 2 * Margin);
    setToolTip(tr("Click to select; double-click to edit the color"));
}

void PaletteWidget::setIndices(std::vector<std::uint8_t> indices)
{
    indices_ = std::move(indices);
    const int rows = std::max(
        1, (static_cast<int>(indices_.size()) + Columns - 1) / Columns);
    setFixedSize(Columns * CellSize + 2 * Margin, rows * CellSize + 2 * Margin);
    update();
}

void PaletteWidget::setLevel(const Level* level)
{
    level_ = level;
    update();
}

void PaletteWidget::setSelectedIndex(const std::uint8_t index)
{
    if (selectedIndex_ == index) {
        return;
    }
    selectedIndex_ = index;
    update();
}

void PaletteWidget::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), palette().brush(QPalette::Mid));

    for (std::size_t position = 0; position < indices_.size(); ++position) {
        const int index = indices_[position];
        const int column = static_cast<int>(position) % Columns;
        const int row = static_cast<int>(position) / Columns;
        const QRect cell(Margin + column * CellSize, Margin + row * CellSize,
                         CellSize, CellSize);
        QColor color = Qt::black;
        if (level_ != nullptr) {
            const RGB& rgb = level_->palette[static_cast<std::size_t>(index)];
            color = QColor(rgb.r, rgb.g, rgb.b);
        }
        painter.fillRect(cell, color);
        painter.setPen(QColor(0, 0, 0, 80));
        painter.drawRect(cell.adjusted(0, 0, -1, -1));
    }

    const auto selected =
        std::find(indices_.begin(), indices_.end(), selectedIndex_);
    if (selected != indices_.end()) {
        const int position = static_cast<int>(selected - indices_.begin());
        const QRect selection(Margin + (position % Columns) * CellSize,
                              Margin + (position / Columns) * CellSize,
                              CellSize, CellSize);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(QPen(Qt::white, 2));
        painter.drawRect(selection.adjusted(1, 1, -2, -2));
        painter.setPen(QPen(Qt::black, 1));
        painter.drawRect(selection.adjusted(0, 0, -1, -1));
    }
}

void PaletteWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const int index = indexAt(event->position().toPoint());
    if (index < 0) {
        return;
    }
    setSelectedIndex(static_cast<std::uint8_t>(index));
    emit indexSelected(selectedIndex_);
    event->accept();
}

void PaletteWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    const int index = indexAt(event->position().toPoint());
    if (index < 0) {
        return;
    }
    setSelectedIndex(static_cast<std::uint8_t>(index));
    emit indexSelected(index);
    emit indexEditRequested(index);
    event->accept();
}

int PaletteWidget::indexAt(const QPoint& widgetPoint) const
{
    const QPoint point = widgetPoint - QPoint(Margin, Margin);
    if (point.x() < 0 || point.y() < 0) {
        return -1;
    }
    const int column = point.x() / CellSize;
    const int row = point.y() / CellSize;
    const int position = row * Columns + column;
    if (column >= Columns || position >= static_cast<int>(indices_.size())) {
        return -1;
    }
    return indices_[static_cast<std::size_t>(position)];
}
