#include "level_canvas.h"
#include "palette_widget.h"

#include <QApplication>
#include <QIcon>
#include <QImage>
#include <QMouseEvent>

#include <cstddef>
#include <iostream>

namespace {

bool expect(const bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

void sendMouseEvent(QWidget *target, const QEvent::Type type,
                    const QPointF position, const Qt::MouseButton button,
                    const Qt::MouseButtons buttons,
                    const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    const QPointF globalPosition(target->mapToGlobal(position.toPoint()));
    QMouseEvent event(type, position, position, globalPosition, button, buttons,
                      modifiers);
    QApplication::sendEvent(target, &event);
}

void click(QWidget *target, const QPointF position)
{
    sendMouseEvent(target, QEvent::MouseButtonPress, position, Qt::LeftButton,
                   Qt::LeftButton);
    sendMouseEvent(target, QEvent::MouseButtonRelease, position, Qt::LeftButton,
                   Qt::NoButton);
}

void drag(QWidget *target, const QPointF from, const QPointF to,
          const Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    sendMouseEvent(target, QEvent::MouseButtonPress, from, Qt::LeftButton,
                   Qt::LeftButton, modifiers);
    sendMouseEvent(target, QEvent::MouseMove, to, Qt::NoButton, Qt::LeftButton,
                   modifiers);
    sendMouseEvent(target, QEvent::MouseButtonRelease, to, Qt::LeftButton,
                   Qt::NoButton, modifiers);
}

std::size_t offset(const int x, const int y)
{
    return static_cast<std::size_t>(y) * Level::Width +
           static_cast<std::size_t>(x);
}

void clearLevel(Level &level)
{
    level.name.clear();
    level.pixels.fill(0);
    level.palette.fill({});
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);

    Level level;
    LevelCanvas canvas;
    canvas.resize(320, 240);
    canvas.setLevel(&level);
    canvas.setSelectedIndex(57);
    canvas.show();
    application.processEvents();

    QWidget *viewport = canvas.viewport();
    drag(viewport, {1.5, 1.5}, {3.5, 1.5});

    bool ok = true;
    const std::size_t rowOffset = Level::Width;
    ok &= expect(level.pixels[rowOffset + 1] == 57 &&
                     level.pixels[rowOffset + 2] == 57 &&
                     level.pixels[rowOffset + 3] == 57,
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
    ok &= expect(level.pixels[rowOffset + 1] == 57 &&
                     level.pixels[rowOffset + 2] == 57 &&
                     level.pixels[rowOffset + 3] == 57,
                 "redo should reapply every pixel in the stroke");

    click(viewport, {2.5, 1.5});
    ok &= expect(canvas.undoStack()->count() == 1,
                 "drawing the existing index should not add an undo command");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setSelectedIndex(58);
    canvas.setDrawTool(DrawTool::Line);
    drag(viewport, {10.5, 10.5}, {13.5, 10.5});
    ok &= expect(level.pixels[offset(10, 10)] == 58 &&
                     level.pixels[offset(11, 10)] == 58 &&
                     level.pixels[offset(12, 10)] == 58 &&
                     level.pixels[offset(13, 10)] == 58,
                 "line should draw both endpoints and the pixels between them");
    ok &= expect(canvas.undoStack()->count() == 1,
                 "line should create one undo command");

    canvas.setLevel(nullptr);
    clearLevel(level);
    level.palette[58] = RGB{255, 0, 0};
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
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Rectangle);
    drag(viewport, {20.5, 20.5}, {22.5, 22.5});
    ok &= expect(level.pixels[offset(20, 20)] == 58 &&
                     level.pixels[offset(22, 22)] == 58 &&
                     level.pixels[offset(21, 21)] == 0,
                 "rectangle should draw an unfilled outline");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::FilledRectangle);
    drag(viewport, {30.5, 30.5}, {32.5, 32.5});
    ok &= expect(level.pixels[offset(30, 30)] == 58 &&
                     level.pixels[offset(31, 31)] == 58 &&
                     level.pixels[offset(32, 32)] == 58,
                 "filled rectangle should also draw its interior");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Rectangle);
    canvas.setToolThickness(DrawTool::Rectangle, 2);
    canvas.setRectangleCornerRadius(0);
    drag(viewport, {110.5, 110.5}, {116.5, 116.5});
    ok &= expect(level.pixels[offset(111, 113)] == 58 &&
                     level.pixels[offset(113, 113)] == 0,
                 "rectangle outline should use its configured thickness");
    ok &= expect(canvas.toolThickness(DrawTool::Rectangle) == 2,
                 "rectangle should remember its own thickness");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setToolThickness(DrawTool::Rectangle, 1);
    canvas.setRectangleCornerRadius(3);
    drag(viewport, {120.5, 120.5}, {128.5, 128.5});
    ok &= expect(level.pixels[offset(120, 120)] == 0 &&
                     level.pixels[offset(124, 120)] == 58,
                 "rounded rectangle should trim corners but keep its edges");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setRectangleCornerRadius(0);
    drag(viewport, {140.5, 110.5}, {146.5, 113.5}, Qt::ShiftModifier);
    ok &= expect(level.pixels[offset(143, 116)] == 58,
                 "Shift should constrain a rectangle to a square");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Ellipse);
    drag(viewport, {35.5, 35.5}, {41.5, 41.5});
    ok &= expect(level.pixels[offset(38, 35)] == 58 &&
                     level.pixels[offset(38, 38)] == 0,
                 "ellipse should draw an unfilled outline");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::FilledEllipse);
    drag(viewport, {45.5, 45.5}, {51.5, 51.5});
    ok &= expect(level.pixels[offset(48, 48)] == 58,
                 "filled ellipse should draw its interior");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Ellipse);
    canvas.setToolThickness(DrawTool::Ellipse, 2);
    drag(viewport, {180.5, 120.5}, {190.5, 130.5});
    ok &= expect(level.pixels[offset(185, 121)] == 58 &&
                     level.pixels[offset(185, 125)] == 0,
                 "ellipse outline should use its configured thickness");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::FilledEllipse);
    drag(viewport, {160.5, 110.5}, {166.5, 113.5}, Qt::ShiftModifier);
    ok &= expect(level.pixels[offset(163, 116)] == 58 &&
                     level.pixels[offset(166, 116)] == 0,
                 "Shift should constrain an ellipse to a circle");

    canvas.setLevel(nullptr);
    clearLevel(level);
    for (int coordinate = 60; coordinate <= 64; ++coordinate) {
        level.pixels[offset(coordinate, 60)] = 1;
        level.pixels[offset(coordinate, 64)] = 1;
        level.pixels[offset(60, coordinate)] = 1;
        level.pixels[offset(64, coordinate)] = 1;
    }
    canvas.setLevel(&level);
    canvas.setSelectedIndex(59);
    canvas.setDrawTool(DrawTool::FloodFill);
    click(viewport, {62.5, 62.5});
    ok &= expect(level.pixels[offset(61, 61)] == 59 &&
                     level.pixels[offset(63, 63)] == 59 &&
                     level.pixels[offset(60, 62)] == 1 &&
                     level.pixels[offset(59, 62)] == 0,
                 "flood fill should stop at a different palette index");
    canvas.undoStack()->undo();
    ok &= expect(level.pixels[offset(61, 61)] == 0 &&
                     level.pixels[offset(63, 63)] == 0,
                 "flood fill should undo as one operation");

    canvas.setLevel(nullptr);
    clearLevel(level);
    level.pixels[offset(40, 40)] = 123;
    canvas.setLevel(&level);
    canvas.setSelectedIndex(59);
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
    clearLevel(level);
    level.pixels[offset(50, 50)] = 77;
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Eraser);
    click(viewport, {50.5, 50.5});
    ok &= expect(level.pixels[offset(50, 50)] == 0,
                 "eraser should write palette index zero");
    canvas.undoStack()->undo();
    ok &= expect(level.pixels[offset(50, 50)] == 77,
                 "eraser should restore the old index when undone");

    canvas.setLevel(nullptr);
    clearLevel(level);
    level.palette[42] = RGB{1, 2, 3};
    canvas.setLevel(&level);
    int changedPaletteIndex = -1;
    QObject::connect(&canvas, &LevelCanvas::paletteColorChanged,
                     [&changedPaletteIndex](const int index) {
                         changedPaletteIndex = index;
                     });
    canvas.setPaletteColor(42, RGB{10, 20, 30});
    ok &= expect(level.palette[42] == RGB{10, 20, 30} &&
                     changedPaletteIndex == 42,
                 "palette color edit should update the exact index");
    canvas.undoStack()->undo();
    ok &= expect(level.palette[42] == RGB{1, 2, 3},
                 "palette color edit should be undoable");
    canvas.undoStack()->redo();
    ok &= expect(level.palette[42] == RGB{10, 20, 30},
                 "palette color edit should be redoable");
    auto replacementPalette = level.palette;
    replacementPalette[16] = RGB{40, 80, 160};
    canvas.setPalette(replacementPalette);
    ok &= expect(level.palette[16] == RGB{40, 80, 160},
                 "complete palette replacement should apply every entry");
    canvas.undoStack()->undo();
    ok &= expect(level.palette[16] == RGB{},
                 "complete palette replacement should be undoable");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Pencil);
    canvas.setSelectedIndex(58);
    canvas.setSelectedIndex(2);
    canvas.setToolThickness(DrawTool::Pencil, 3);
    click(viewport, {80.5, 80.5});
    ok &= expect(level.pixels[offset(79, 79)] == 58 &&
                     level.pixels[offset(81, 81)] == 58,
                 "thick pencil should paint its configured square footprint");
    ok &= expect(canvas.toolThickness(DrawTool::Pencil) == 3,
                 "pencil should remember its own thickness");

    canvas.setLevel(nullptr);
    clearLevel(level);
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Line);
    canvas.setSelectedIndex(59);
    canvas.setToolThickness(DrawTool::Line, 3);
    drag(viewport, {90.5, 90.5}, {94.5, 90.5});
    ok &= expect(level.pixels[offset(92, 89)] == 59 &&
                     level.pixels[offset(92, 91)] == 59,
                 "line should use its configured thickness");

    canvas.setLevel(nullptr);
    clearLevel(level);
    for (int y = 99; y <= 102; ++y) {
        for (int x = 99; x <= 102; ++x) {
            level.pixels[offset(x, y)] = 77;
        }
    }
    canvas.setLevel(&level);
    canvas.setDrawTool(DrawTool::Eraser);
    canvas.setToolThickness(DrawTool::Eraser, 4);
    click(viewport, {100.5, 100.5});
    ok &= expect(level.pixels[offset(99, 99)] == 0 &&
                     level.pixels[offset(102, 102)] == 0,
                 "eraser should use its independently configured thickness");

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
    palette.setIndices({16, 17, 18, 19});
    paletteIndex = -1;
    application.processEvents();
    click(&palette,
          {1.0 + 2 * paletteCell + paletteCell / 2.0, 1.0 + paletteCell / 2.0});
    ok &= expect(paletteIndex == 18,
                 "filtered palette should map cells to their original indices");

    int editRequestedIndex = -1;
    QObject::connect(
        &palette, &PaletteWidget::indexEditRequested,
        [&editRequestedIndex](const int index) { editRequestedIndex = index; });
    sendMouseEvent(
        &palette, QEvent::MouseButtonDblClick,
        {1.0 + 3 * paletteCell + paletteCell / 2.0, 1.0 + paletteCell / 2.0},
        Qt::LeftButton, Qt::LeftButton);
    ok &= expect(editRequestedIndex == 19,
                 "palette double-click should edit the exact filtered index");

    ok &= expect(
        !QIcon(QStringLiteral(":/icons/icons/pencil.svg")).isNull() &&
            !QIcon(QStringLiteral(":/icons/icons/bucket-droplet.svg")).isNull(),
        "embedded Tabler tool icons should load from resources");

    if (ok) {
        std::cout << "All level canvas tests passed\n";
        return 0;
    }
    return 1;
}
