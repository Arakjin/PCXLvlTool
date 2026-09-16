#pragma once

#include "game_profile.h"
#include "level.h"

#include <filesystem>
#include <memory>
#include <string>

// Versioned, game-tagged editable project format used by PCX Level Tool.
// Game-compatible LEV files remain separate publishing formats.
bool savePxlProject(const std::filesystem::path& path, const Level& level,
                    const Level* background,
                    const LevelCreationSettings& settings,
                    std::string& error);

bool loadPxlProject(const std::filesystem::path& path, Level& level,
                    std::unique_ptr<Level>& background,
                    LevelCreationSettings& settings,
                    std::string& error);
