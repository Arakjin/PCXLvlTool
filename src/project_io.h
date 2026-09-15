#pragma once

#include "level.h"

#include <filesystem>
#include <string>

bool loadProject(const std::filesystem::path& path, Level& level,
                 std::string& error);
bool saveProject(const std::filesystem::path& path, const Level& level,
                 std::string& error);

