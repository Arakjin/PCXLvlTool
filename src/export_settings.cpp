#include "export_settings.h"

#include "game_profile.h"

#include <QDir>
#include <QSettings>

namespace {

QString settingKey(const GameId game)
{
    const std::string_view gameKey = gameProfile(game).key;
    return QStringLiteral("exportDirectories/%1")
        .arg(QString::fromUtf8(gameKey.data(),
                               static_cast<qsizetype>(gameKey.size())));
}

} // namespace

QString exportDirectorySetting(const QSettings& settings, const GameId game)
{
    return settings.value(settingKey(game)).toString();
}

void setExportDirectorySetting(QSettings& settings, const GameId game,
                               const QString& directory)
{
    const QString normalized =
        QDir::cleanPath(QDir::fromNativeSeparators(directory.trimmed()));
    if (directory.trimmed().isEmpty() || normalized == QStringLiteral(".")) {
        settings.remove(settingKey(game));
        return;
    }
    settings.setValue(settingKey(game), normalized);
}

