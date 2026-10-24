#pragma once

#include "level.h"

#include <filesystem>
#include <string>

bool savePcx(const std::filesystem::path& path, const Level& level,
             std::string& error);

// Writes OverThrust Layered PCX (.OPCX): a flattened PCX compatibility image
// plus independently encoded layer records before the trailing palette.
bool saveOverThrustLayeredPcx(const std::filesystem::path& path,
                              const Level& level, std::string& error);
