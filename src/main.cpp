#include "main_window.h"

#include "level_canvas.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QSignalBlocker>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("PCX Level Tool"));
    QApplication::setApplicationVersion(
        QStringLiteral(PCXLVLTOOL_VERSION));
    QApplication::setOrganizationName(QStringLiteral("PCX Level Tools"));
    QApplication::setWindowIcon(
        QIcon(QStringLiteral(":/icons/app/pcxlvltool.svg")));

    const bool smokeTest =
        application.arguments().contains(QStringLiteral("--smoke-test"));
    MainWindow window;
    window.show();
    if (smokeTest) {
        if (auto* canvas = window.findChild<LevelCanvas*>()) {
            const QSignalBlocker blocker(canvas);
            canvas->setPaletteColor(0, RGB{1, 2, 3});
        }
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    } else {
        QTimer::singleShot(0, &window, &MainWindow::promptForInitialLevel);
    }
    return application.exec();
}
