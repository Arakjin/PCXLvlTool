#include "export_settings.h"

#include "game_profile.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {

constexpr auto kProjectDirectoryKey = "projectDirectory";

QString normalizedDirectory(const QString& directory)
{
    return QDir::cleanPath(
        QDir::fromNativeSeparators(directory.trimmed()));
}

QString settingKey(const GameId game)
{
    const std::string_view gameKey = gameProfile(game).key;
    return QStringLiteral("exportDirectories/%1")
        .arg(QString::fromUtf8(gameKey.data(),
                               static_cast<qsizetype>(gameKey.size())));
}

} // namespace

QString defaultProjectDirectory()
{
    QString parent = QStandardPaths::writableLocation(
        QStandardPaths::DocumentsLocation);
    if (parent.isEmpty()) {
        parent = QDir::homePath();
    }
    return QDir(parent).filePath(
        QStringLiteral("PCX Level Tool/Projects"));
}

QString projectDirectorySetting(const QSettings& settings)
{
    return settings.value(QString::fromLatin1(kProjectDirectoryKey)).toString();
}

QString effectiveProjectDirectorySetting(const QSettings& settings)
{
    const QString configured = projectDirectorySetting(settings);
    return configured.isEmpty() ? defaultProjectDirectory() : configured;
}

void setProjectDirectorySetting(QSettings& settings,
                                const QString& directory)
{
    const QString normalized = normalizedDirectory(directory);
    const QString key = QString::fromLatin1(kProjectDirectoryKey);
    if (directory.trimmed().isEmpty() || normalized == QStringLiteral(".") ||
        normalized == normalizedDirectory(defaultProjectDirectory())) {
        settings.remove(key);
        return;
    }
    settings.setValue(key, normalized);
}

QString exportDirectorySetting(const QSettings& settings, const GameId game)
{
    return settings.value(settingKey(game)).toString();
}

void setExportDirectorySetting(QSettings& settings, const GameId game,
                               const QString& directory)
{
    const QString normalized = normalizedDirectory(directory);
    if (directory.trimmed().isEmpty() || normalized == QStringLiteral(".")) {
        settings.remove(settingKey(game));
        return;
    }
    settings.setValue(settingKey(game), normalized);
}
