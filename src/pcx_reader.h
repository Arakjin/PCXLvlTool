#pragma once

#include "level.h"

#include <filesystem>
#include <string>

// Loads an 8-bit, single-plane, RLE-encoded indexed PCX image. Dimensions and
// palette indices are preserved for game-specific import validation by the UI.
bool loadPcx(const std::filesystem::path& path, Level& level,
             std::string& error);

