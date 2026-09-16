#include "main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
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
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    } else {
        QTimer::singleShot(0, &window, &MainWindow::promptForInitialLevel);
    }
    return application.exec();
}
