#pragma once

#include "game_profile.h"

#include <QString>

class QSettings;

QString exportDirectorySetting(const QSettings& settings, GameId game);
void setExportDirectorySetting(QSettings& settings, GameId game,
                               const QString& directory);

