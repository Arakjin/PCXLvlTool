#include "layer_model.h"

#include <algorithm>
#include <utility>

void initializeBackgroundLayer(Level& level)
{
    if (!level.layers.empty()) {
        level.activeLayer =
            std::min(level.activeLayer, level.layers.size() - 1);
        return;
    }
    Level::Layer background;
    background.name = "Background";
    background.pixels = level.pixels;
    background.mask.fill(1);
    level.layers.push_back(std::move(background));
    level.activeLayer = 0;
}

std::uint8_t compositeLayerPixel(const Level& level, const std::size_t offset,
                                 const std::size_t skippedLayer)
{
    for (std::size_t index = level.layers.size(); index-- > 0;) {
        const Level::Layer& layer = level.layers[index];
        if (index != skippedLayer && layer.visible && layer.mask[offset] != 0) {
            return layer.pixels[offset];
        }
    }
    return 0;
}

void flattenLayers(Level& level)
{
    initializeBackgroundLayer(level);
    for (std::size_t offset = 0; offset < Level::PixelCount; ++offset) {
        level.pixels[offset] = compositeLayerPixel(level, offset);
    }
}
