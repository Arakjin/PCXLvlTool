#pragma once

#include "level.h"

#include <filesystem>
#include <string>

bool loadLev(const std::filesystem::path& path, Level& level,
             std::string& error);
