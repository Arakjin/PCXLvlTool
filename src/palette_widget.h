#pragma once

#include "level.h"

#include <QWidget>

#include <cstdint>

class QMouseEvent;
class QPaintEvent;

class PaletteWidget final : public QWidget {
    Q_OBJECT

public:
    explicit PaletteWidget(QWidget* parent = nullptr);

    void setLevel(const Level* level);
    void setSelectedIndex(std::uint8_t index);

signals:
    void indexSelected(int index);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private:
    static constexpr int Columns = 16;
    static constexpr int Rows = 16;
    static constexpr int CellSize = 14;
    static constexpr int Margin = 1;

    const Level* level_ = nullptr;
    std::uint8_t selectedIndex_ = 57;
};
