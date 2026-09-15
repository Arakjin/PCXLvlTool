#pragma once

#include "level.h"

#include <QWidget>

#include <cstdint>
#include <vector>

class QMouseEvent;
class QPaintEvent;

class PaletteWidget final : public QWidget {
    Q_OBJECT

public:
    explicit PaletteWidget(QWidget* parent = nullptr);

    void setLevel(const Level* level);
    void setIndices(std::vector<std::uint8_t> indices);
    void setSelectedIndex(std::uint8_t index);
    void setSecondaryIndex(std::uint8_t index);

signals:
    void indexSelected(int index);
    void secondaryIndexSelected(int index);
    void indexEditRequested(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    int indexAt(const QPoint& point) const;

    static constexpr int Columns = 16;
    static constexpr int CellSize = 14;
    static constexpr int Margin = 1;

    const Level* level_ = nullptr;
    std::vector<std::uint8_t> indices_;
    std::uint8_t selectedIndex_ = 56;
    std::uint8_t secondaryIndex_ = 57;
};
