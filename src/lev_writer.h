#pragma once

#include "level.h"

#include <filesystem>
#include <string>

bool saveLev(const std::filesystem::path& path, const Level& level,
             std::string& error);
