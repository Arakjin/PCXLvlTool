#pragma once

#include "level.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct OverThrustMaterial {
    std::uint8_t index;
    std::string label;
    std::string category;
    std::string materialId;
    std::string appearanceId;
    RGB preview;
};

const std::vector<OverThrustMaterial>& overthrustMaterials();
const OverThrustMaterial* overthrustMaterial(std::uint8_t index);
std::array<RGB, 256> defaultOverThrustPalette();

