#include "layer_model.h"

#include <algorithm>

void initializeBackgroundLayer(Level& level)
{
    if (!level.layers.empty()) {
        level.activeLayer =
            std::min(level.activeLayer, level.layers.size() - 1);
        return;
    }
    level.layers.emplace_back();
    Level::Layer& background = level.layers.back();
    background.pixels.resize(level.pixelCount());
    background.mask.resize(level.pixelCount());
    background.name = "Background";
    background.pixels = level.pixels;
    background.mask.fill(1);
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
    for (std::size_t offset = 0; offset < level.pixelCount(); ++offset) {
        level.pixels[offset] = compositeLayerPixel(level, offset);
    }
}
