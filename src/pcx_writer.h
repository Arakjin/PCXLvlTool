#pragma once

#include "level.h"

#include <filesystem>
#include <string>

bool savePcx(const std::filesystem::path& path, const Level& level,
             std::string& error);
