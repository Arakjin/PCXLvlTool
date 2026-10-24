#pragma once

#include "level.h"

#include <filesystem>
#include <cstdint>
#include <string>
#include <vector>

// Loads an 8-bit, single-plane, RLE-encoded indexed PCX image. Dimensions and
// palette indices are preserved for game-specific import validation by the UI.
bool loadPcx(const std::filesystem::path& path, Level& level,
             std::string& error);

bool loadOverThrustLayeredPcx(const std::filesystem::path& path, Level& level,
                              std::string& error);

// In-memory variants used by container formats such as OverThrust OVL.
bool loadPcx(const std::vector<std::uint8_t>& bytes, const std::string& name,
             Level& level, std::string& error);
bool loadOverThrustLayeredPcx(const std::vector<std::uint8_t>& bytes,
                              const std::string& name, Level& level,
                              std::string& error);

