#include "palette_widget.h"

#include <QMouseEvent>
#include <QPainter>

PaletteWidget::PaletteWidget(QWidget* parent) : QWidget(parent)
{
    setFixedSize(Columns * CellSize + 2 * Margin, Rows * CellSize + 2 * Margin);
    setToolTip(tr("Select a palette index"));
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

    for (int index = 0; index < 256; ++index) {
        const int column = index % Columns;
        const int row = index / Columns;
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

    const int selected = selectedIndex_;
    const QRect selection(Margin + (selected % Columns) * CellSize,
                          Margin + (selected / Columns) * CellSize, CellSize,
                          CellSize);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(Qt::white, 2));
    painter.drawRect(selection.adjusted(1, 1, -2, -2));
    painter.setPen(QPen(Qt::black, 1));
    painter.drawRect(selection.adjusted(0, 0, -1, -1));
}

void PaletteWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint point = event->position().toPoint() - QPoint(Margin, Margin);
    if (point.x() < 0 || point.y() < 0) {
        return;
    }
    const int column = point.x() / CellSize;
    const int row = point.y() / CellSize;
    if (column >= Columns || row >= Rows) {
        return;
    }

    setSelectedIndex(static_cast<std::uint8_t>(row * Columns + column));
    emit indexSelected(selectedIndex_);
    event->accept();
}
