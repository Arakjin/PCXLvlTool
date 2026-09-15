#pragma once

#include "level.h"

#include <array>
#include <filesystem>
#include <string>

bool loadJascPalette(const std::filesystem::path& path,
                     std::array<RGB, 256>& palette, std::string& error);
bool saveJascPalette(const std::filesystem::path& path,
                     const std::array<RGB, 256>& palette, std::string& error);
