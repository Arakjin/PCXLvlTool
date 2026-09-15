#include "level_canvas.h"

#include <QApplication>
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
    sendMouseEvent(viewport, QEvent::MouseButtonPress, {1.5, 1.5},
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(viewport, QEvent::MouseMove, {3.5, 1.5}, Qt::NoButton,
                   Qt::LeftButton);
    sendMouseEvent(viewport, QEvent::MouseButtonRelease, {3.5, 1.5},
                   Qt::LeftButton, Qt::NoButton);

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

    sendMouseEvent(viewport, QEvent::MouseButtonPress, {2.5, 1.5},
                   Qt::LeftButton, Qt::LeftButton);
    sendMouseEvent(viewport, QEvent::MouseButtonRelease, {2.5, 1.5},
                   Qt::LeftButton, Qt::NoButton);
    ok &= expect(canvas.undoStack()->count() == 1,
                 "drawing the existing index should not add an undo command");

    if (ok) {
        std::cout << "All level canvas tests passed\n";
        return 0;
    }
    return 1;
}
