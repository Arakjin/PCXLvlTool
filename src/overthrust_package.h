#pragma once

#include "level.h"
#include "overthrust_model.h"

#include <filesystem>
#include <string>

bool publishOverThrustPackage(const std::filesystem::path& directory,
                              const Level& materials,
                              const OverThrustProjectData& data,
                              std::string& error);

