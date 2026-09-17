#pragma once

#include "game_profile.h"

#include <QString>

class QSettings;

QString defaultProjectDirectory();
QString projectDirectorySetting(const QSettings& settings);
QString effectiveProjectDirectorySetting(const QSettings& settings);
void setProjectDirectorySetting(QSettings& settings,
                                const QString& directory);
QString exportDirectorySetting(const QSettings& settings, GameId game);
void setExportDirectorySetting(QSettings& settings, GameId game,
                               const QString& directory);
