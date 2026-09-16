#pragma once

#include "game_profile.h"
#include "level.h"

#include <filesystem>
#include <string>

bool saveWingsLev(const std::filesystem::path& path, const Level& level,
                  const Level* background,
                  const LevelCreationSettings& settings,
                  std::string& error);
