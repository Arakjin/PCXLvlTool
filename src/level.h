#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
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

    struct Layer {
        std::string name;
        bool visible = true;
        bool locked = false;
        std::array<std::uint8_t, PixelCount> pixels{};
        std::array<std::uint8_t, PixelCount> mask{};
    };

    std::string name;
    std::array<std::uint8_t, PixelCount> pixels{};
    std::array<RGB, 256> palette{};
    std::vector<Layer> layers;
    std::size_t activeLayer = 0;
};
