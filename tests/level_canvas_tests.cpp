#include "level_canvas.h"
#include "palette_widget.h"

#include <QApplication>
#include <QImage>
#include <QMouseEvent>

#include <cstddef>
#include <iostream>

namespace {

bool expect(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

void sendMouseEvent(QWidget* target, const QEvent::Type type,
                    const QPointF position, const Qt::MouseButton button,
                    const Qt::MouseButtons buttons)
{
    const QPointF globalPosition(target->mapToGlobal(position.toPoint()));
    QMouseEvent event(type, position, position, globalPosition, button, buttons,
                      Qt::NoModifier);
    QApplication::sendEvent(target, &event);
}

void click(QWidget* target, const QPointF position)
{
    sendMouseEvent(target, QEvent::MouseButtonPress, position, Qt::LeftButton,
                   Qt::LeftButton);
    sendMouseEvent(target, QEvent::MouseButtonRelease, position, Qt::LeftButton,
                   Qt::NoButton);
}

void drag(QWidget* target, const QPointF from, const QPointF to)
{
    sendMouseEvent(target, QEvent::MouseButtonPress, from, Qt::LeftButton,
                   Qt::LeftButton);
    sendMouseEvent(target, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton);
    sendMouseEvent(target, QEvent::MouseButtonRelease, to, Qt::LeftButton,
                   Qt::NoButton);
}

std::size_t offset(const int x, const int y)
{
    return static_cast<std::size_t>(y) * Level::Width +
           static_cast<std::size_t>(x);
}

} // namespace

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);

    Level level;
    LevelCanvas canvas;
    canvas.resize(320, 240);
    canvas.setLevel(&level);
    canvas.setSelectedIndex(7);
    canvas.show();
    application.processEvents();

    QWidget* viewport = canvas.viewport();
    drag(viewport, {1.5, 1.5}, {3.5, 1.5});

    bool ok = true;
    const std::size_t rowOffset = Level::Width;
    ok &= expect(level.pixels[rowOffset + 1] == 7 &&
                     level.pixels[rowOffset + 2] == 7 &&
                     level.pixels[rowOffset + 3] == 7,
                 "a drag should draw every pixel along the stroke");
    ok &= expect(canvas.undoStack()->count() == 1,
                 "one drag should create one undo command");
    ok &= expect(!canvas.undoStack()->isClean(),
                 "drawing should mark the undo stack dirty");

    canvas.undoStack()->undo();
    ok &= expect(level.pixels[rowOffset + 1] == 0 &&
                     level.pixels[rowOffset + 2] == 0 &&
                     level.pixels[rowOffset + 3] == 0,
                 "undo should restore every pixel in the stroke");
    ok &= expect(canvas.undoStack()->isClean(),
                 "undoing the first stroke should restore the clean state");

    canvas.undoStack()->redo();
    ok &= expect(level.pixels[rowOffset + 1] == 7 &&
                     level.pixels[rowOffset + 2] == 7 &&
                     level.pixels[rowOffset + 3] == 7,
                 "redo should reapply every pixel in the stroke");

    click(viewport, {2.5, 1.5});
    ok &= expect(canvas.undoStack()->count() == 1,
                 "drawing the existing index should not add an undo command");

    canvas.setLevel(nullptr);
    level = Level{};
    canvas.setLevel(&level);
    canvas.setSelectedIndex(8);
    canvas.setDrawTool(DrawTool::Line);
    drag(viewport, {10.5, 10.5}, {13.5, 10.5});
    ok &= expect(level.pixels[offset(10, 10)] == 8 &&
                     level.pixels[offset(11, 10)] == 8 &&
                     level.pixels[offset(12, 10)] == 8 &&
                     level.pixels[offset(13, 10)] == 8,
                 "line should draw both endpoints and the pixels between them");
    ok &= expect(canvas.undoStack()->count() == 1,
                 "line should create one undo command");

    canvas.setLevel(nullptr);
    level = Level{};
    level.palette[8] = RGB{255, 0, 0};
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Line);
    const QImage beforePreview = viewport->grab().toImage();
    sendMouseEvent(viewport, QEvent::MouseButtonPress, {70.5, 70.5},
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(viewport, QEvent::MouseMove, {74.5, 70.5}, Qt::NoButton,
                   Qt::LeftButton);
    application.processEvents();
    const QImage duringPreview = viewport->grab().toImage();
    ok &= expect(beforePreview != duringPreview,
                 "shape drag should render a live preview");
    ok &= expect(level.pixels[offset(72, 70)] == 0,
                 "shape preview should not edit level pixels before release");
    sendMouseEvent(viewport, QEvent::MouseButtonRelease, {74.5, 70.5},
                   Qt::LeftButton, Qt::NoButton);

    canvas.setLevel(nullptr);
    level = Level{};
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Rectangle);
    drag(viewport, {20.5, 20.5}, {22.5, 22.5});
    ok &= expect(level.pixels[offset(20, 20)] == 8 &&
                     level.pixels[offset(22, 22)] == 8 &&
                     level.pixels[offset(21, 21)] == 0,
                 "rectangle should draw an unfilled outline");

    canvas.setLevel(nullptr);
    level = Level{};
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::FilledRectangle);
    drag(viewport, {30.5, 30.5}, {32.5, 32.5});
    ok &= expect(level.pixels[offset(30, 30)] == 8 &&
                     level.pixels[offset(31, 31)] == 8 &&
                     level.pixels[offset(32, 32)] == 8,
                 "filled rectangle should also draw its interior");

    canvas.setLevel(nullptr);
    level = Level{};
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Ellipse);
    drag(viewport, {35.5, 35.5}, {41.5, 41.5});
    ok &= expect(level.pixels[offset(38, 35)] == 8 &&
                     level.pixels[offset(38, 38)] == 0,
                 "ellipse should draw an unfilled outline");

    canvas.setLevel(nullptr);
    level = Level{};
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::FilledEllipse);
    drag(viewport, {45.5, 45.5}, {51.5, 51.5});
    ok &= expect(level.pixels[offset(48, 48)] == 8,
                 "filled ellipse should draw its interior");

    canvas.setLevel(nullptr);
    level = Level{};
    for (int coordinate = 60; coordinate <= 64; ++coordinate) {
        level.pixels[offset(coordinate, 60)] = 1;
        level.pixels[offset(coordinate, 64)] = 1;
        level.pixels[offset(60, coordinate)] = 1;
        level.pixels[offset(64, coordinate)] = 1;
    }
    canvas.setLevel(&level);
    canvas.setSelectedIndex(9);
    canvas.setDrawTool(DrawTool::FloodFill);
    click(viewport, {62.5, 62.5});
    ok &= expect(level.pixels[offset(61, 61)] == 9 &&
                     level.pixels[offset(63, 63)] == 9 &&
                     level.pixels[offset(60, 62)] == 1 &&
                     level.pixels[offset(59, 62)] == 0,
                 "flood fill should stop at a different palette index");
    canvas.undoStack()->undo();
    ok &= expect(level.pixels[offset(61, 61)] == 0 &&
                     level.pixels[offset(63, 63)] == 0,
                 "flood fill should undo as one operation");

    canvas.setLevel(nullptr);
    level = Level{};
    level.pixels[offset(40, 40)] = 123;
    canvas.setLevel(&level);
    canvas.setSelectedIndex(9);
    int pickedIndex = -1;
    QObject::connect(&canvas, &LevelCanvas::selectedIndexChanged,
                     [&pickedIndex](const int index) { pickedIndex = index; });
    canvas.setDrawTool(DrawTool::Eyedropper);
    click(viewport, {40.5, 40.5});
    ok &= expect(pickedIndex == 123,
                 "eyedropper should select the exact palette index");
    ok &= expect(canvas.undoStack()->count() == 0,
                 "eyedropper should not create an undo command");

    canvas.setLevel(nullptr);
    level = Level{};
    level.pixels[offset(50, 50)] = 77;
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Eraser);
    click(viewport, {50.5, 50.5});
    ok &= expect(level.pixels[offset(50, 50)] == 0,
                 "eraser should write palette index zero");
    canvas.undoStack()->undo();
    ok &= expect(level.pixels[offset(50, 50)] == 77,
                 "eraser should restore the old index when undone");

    PaletteWidget palette;
    palette.setLevel(&level);
    palette.show();
    application.processEvents();
    int paletteIndex = -1;
    QObject::connect(
        &palette, &PaletteWidget::indexSelected,
        [&paletteIndex](const int index) { paletteIndex = index; });
    const int paletteCell = (palette.width() - 2) / 16;
    click(&palette, {1.0 + 13 * paletteCell + paletteCell / 2.0,
                     1.0 + 7 * paletteCell + paletteCell / 2.0});
    ok &= expect(paletteIndex == 125,
                 "palette click should preserve the exact grid index");

    if (ok) {
        std::cout << "All level canvas tests passed\n";
        return 0;
    }
    return 1;
}
