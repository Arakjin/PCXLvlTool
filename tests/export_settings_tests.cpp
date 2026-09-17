#include "export_settings.h"

#include <QSettings>
#include <QTemporaryDir>

#include <iostream>

namespace {

bool expect(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    QTemporaryDir temporaryDirectory;
    if (!temporaryDirectory.isValid()) {
        std::cerr << "FAIL: could not create temporary directory\n";
        return 1;
    }

    QSettings settings(temporaryDirectory.filePath(QStringLiteral("test.ini")),
                       QSettings::IniFormat);
    bool ok = true;
    ok &= expect(exportDirectorySetting(settings, GameId::VWing).isEmpty(),
                 "unset V-Wing directory");

    setExportDirectorySetting(settings, GameId::VWing,
                              QStringLiteral(
                                  " games/vwing/levels/../levels "));
    setExportDirectorySetting(settings, GameId::Wings,
                              QStringLiteral("games/wings"));
    setExportDirectorySetting(settings, GameId::Auts,
                              QStringLiteral("games/auts"));
    ok &= expect(exportDirectorySetting(settings, GameId::VWing) ==
                     QStringLiteral("games/vwing/levels"),
                 "V-Wing directory is normalized");
    ok &= expect(exportDirectorySetting(settings, GameId::Wings) ==
                     QStringLiteral("games/wings"),
                 "Wings has an independent directory");
    ok &= expect(exportDirectorySetting(settings, GameId::Auts) ==
                     QStringLiteral("games/auts"),
                 "AUTS has an independent directory");

    setExportDirectorySetting(settings, GameId::Wings, QString{});
    ok &= expect(exportDirectorySetting(settings, GameId::Wings).isEmpty() &&
                     !exportDirectorySetting(settings, GameId::VWing).isEmpty(),
                 "clearing one game leaves the others unchanged");
    return ok ? 0 : 1;
}
