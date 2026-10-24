#pragma once

#include "level.h"
#include "overthrust_model.h"

#include <filesystem>
#include <string>

bool publishOverThrustPackage(const std::filesystem::path& file,
                              const Level& materials,
                              const Level& background,
                              const OverThrustProjectData& data,
                              std::string& error);

bool loadOverThrustPackage(const std::filesystem::path& file,
                           Level& materials, Level& background,
                           OverThrustProjectData& data, std::string& error);

