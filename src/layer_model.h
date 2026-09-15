#pragma once

#include "level.h"

#include <cstddef>
#include <cstdint>

void initializeBackgroundLayer(Level& level);
std::uint8_t compositeLayerPixel(const Level& level, std::size_t offset,
                                 std::size_t skippedLayer = Level::MaxLayers);
void flattenLayers(Level& level);

