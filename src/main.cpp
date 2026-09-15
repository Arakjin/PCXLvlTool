#include "main_window.h"

#include <QApplication>
#include <QCoreApplication>
#include <QTimer>

int main(int argc, char* argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("V-Wing Level Editor"));
    QApplication::setOrganizationName(QStringLiteral("V-Wing Tools"));

    MainWindow window;
    window.show();
    if (application.arguments().contains(QStringLiteral("--smoke-test"))) {
        QTimer::singleShot(0, &application, &QCoreApplication::quit);
    }
    return application.exec();
}
