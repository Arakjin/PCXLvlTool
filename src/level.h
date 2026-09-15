#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

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

    std::string name;
    std::array<std::uint8_t, PixelCount> pixels{};
    std::array<RGB, 256> palette{};
};
