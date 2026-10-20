#pragma once

#include "level.h"

#include <algorithm>
#include <array>
#include <cstddef>
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

constexpr std::size_t OverThrustMaximumBackgroundSide = 8192;
constexpr std::size_t OverThrustMaximumBackgroundPixels = 33554432;

constexpr std::size_t overthrustMaximumBackgroundDimension(
    const std::size_t worldDimension)
{
    return std::min(worldDimension * 4,
                    OverThrustMaximumBackgroundSide);
}

bool overthrustBackgroundDimensionsAllowed(std::size_t imageWidth,
                                           std::size_t imageHeight,
                                           std::size_t worldWidth,
                                           std::size_t worldHeight);

