#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

struct RGB {
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    bool operator==(const RGB& other) const
    {
        return r == other.r && g == other.g && b == other.b;
    }
};

struct Level {
    static constexpr std::size_t Width = 640;
    static constexpr std::size_t Height = 800;
    static constexpr std::size_t PixelCount = Width * Height;
    static constexpr std::size_t MaxLayers = 5;

    struct PixelBuffer : std::vector<std::uint8_t> {
        explicit PixelBuffer(const std::size_t size = PixelCount)
            : std::vector<std::uint8_t>(size)
        {
        }

        void fill(const std::uint8_t value)
        {
            std::fill(begin(), end(), value);
        }
    };

    struct Layer {
        std::string name;
        bool visible = true;
        bool locked = false;
        PixelBuffer pixels;
        PixelBuffer mask;
    };

    std::string name;
    std::size_t width = Width;
    std::size_t height = Height;
    PixelBuffer pixels;
    std::array<RGB, 256> palette{};
    std::vector<Layer> layers;
    std::size_t activeLayer = 0;

    std::size_t pixelCount() const { return width * height; }

    void resize(const std::size_t newWidth, const std::size_t newHeight)
    {
        width = newWidth;
        height = newHeight;
        pixels.assign(pixelCount(), 0);
        layers.clear();
        activeLayer = 0;
    }

    void resizePreservingContent(const std::size_t newWidth,
                                 const std::size_t newHeight)
    {
        if (newWidth == width && newHeight == height) {
            return;
        }
        const std::size_t oldWidth = width;
        const std::size_t copyWidth = std::min(oldWidth, newWidth);
        const std::size_t copyHeight = std::min(height, newHeight);
        const auto resizeBuffer = [&](PixelBuffer& buffer) {
            PixelBuffer resized(newWidth * newHeight);
            for (std::size_t y = 0; y < copyHeight; ++y) {
                std::copy_n(buffer.begin() +
                                static_cast<std::ptrdiff_t>(y * oldWidth),
                            copyWidth,
                            resized.begin() +
                                static_cast<std::ptrdiff_t>(y * newWidth));
            }
            buffer = std::move(resized);
        };
        resizeBuffer(pixels);
        for (Layer& layer : layers) {
            resizeBuffer(layer.pixels);
            resizeBuffer(layer.mask);
        }
        width = newWidth;
        height = newHeight;
    }
};
