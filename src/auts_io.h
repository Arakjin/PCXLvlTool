#pragma once

#include "level.h"

#include <filesystem>
#include <string>

// AUTS uses a fixed 320 x 400 indexed image and a fixed game palette.
void initializeBlankAutsLevel(Level& level);
bool loadAutsLev(const std::filesystem::path& path, Level& level,
                 std::string& error);
bool saveAutsLev(const std::filesystem::path& path, const Level& level,
                 std::string& error);

// Imports the exact 8-bit, uncompressed BMP layout accepted by AUTSCONV.
// Pixels are preserved as palette indices and the editor applies the fixed
// AUTS palette. paletteMatches reports whether the source already used it.
bool loadAutsBmp(const std::filesystem::path& path, Level& level,
                 bool& paletteMatches, std::string& error);
