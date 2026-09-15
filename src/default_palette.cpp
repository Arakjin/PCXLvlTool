#include "default_palette.h"

#include "palette_rules.h"

#include <cstddef>
#include <cstdint>

namespace {

std::uint8_t interpolate(const std::uint8_t from, const std::uint8_t to,
                         const int position, const int count)
{
    if (count <= 1) {
        return from;
    }
    const int value = static_cast<int>(from) +
                      (static_cast<int>(to) - static_cast<int>(from)) *
                          position / (count - 1);
    return static_cast<std::uint8_t>(value);
}

void fillGradient(std::array<RGB, 256>& palette, const int first,
                  const int last, const RGB from, const RGB to)
{
    const int count = last - first + 1;
    for (int index = first; index <= last; ++index) {
        const int position = index - first;
        palette[static_cast<std::size_t>(index)] = RGB{
            interpolate(from.r, to.r, position, count),
            interpolate(from.g, to.g, position, count),
            interpolate(from.b, to.b, position, count),
        };
    }
}

} // namespace

std::array<RGB, 256> defaultVWingPalette()
{
    std::array<RGB, 256> palette{};
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto gray = static_cast<std::uint8_t>(index);
        palette[index] = RGB{gray, gray, gray};
    }

    palette[0] = RGB{0, 0, 0};
    palette[1] = RGB{0, 0, 0};
    fillGradient(palette, 2, 15, RGB{80, 0, 80}, RGB{255, 0, 255});
    palette[16] = RGB{20, 85, 190};
    palette[17] = RGB{45, 170, 235};
    palette[18] = RGB{20, 120, 220};
    palette[19] = RGB{15, 70, 165};
    fillGradient(palette, 20, 30, RGB{25, 35, 55}, RGB{115, 155, 190});
    fillGradient(palette, 32, 37, RGB{245, 245, 245}, RGB{125, 125, 125});
    palette[39] = RGB{175, 235, 255};
    fillGradient(palette, 40, 45, RGB{255, 210, 20}, RGB{205, 25, 15});
    palette[46] = RGB{135, 90, 45};
    palette[48] = RGB{155, 0, 20};
    palette[49] = RGB{145, 90, 55};
    palette[50] = RGB{70, 105, 145};
    palette[51] = RGB{75, 75, 75};
    palette[52] = RGB{245, 250, 255};
    palette[56] = RGB{90, 210, 240};

    fillGradient(palette, 57, 79, RGB{70, 45, 25}, RGB{185, 130, 70});
    fillGradient(palette, 80, 103, RGB{30, 65, 25}, RGB{125, 175, 70});
    fillGradient(palette, 104, 126, RGB{55, 55, 60}, RGB{185, 185, 190});
    fillGradient(palette, 127, 149, RGB{115, 85, 45}, RGB{230, 205, 130});
    palette[150] = RGB{95, 95, 100};
    fillGradient(palette, 151, 174, RGB{80, 40, 20}, RGB{225, 130, 35});
    palette[175] = RGB{255, 80, 0};
    fillGradient(palette, 176, 199, RGB{210, 70, 20}, RGB{65, 65, 65});
    palette[200] = RGB{255, 0, 255};
    palette[201] = RGB{65, 80, 90};
    palette[202] = RGB{105, 85, 70};
    palette[203] = RGB{145, 215, 235};
    fillGradient(palette, 204, 219, RGB{45, 100, 105}, RGB{35, 150, 190});
    palette[220] = RGB{255, 0, 255};
    fillGradient(palette, 221, 243, RGB{55, 60, 70}, RGB{175, 185, 200});
    palette[244] = RGB{255, 230, 80};
    palette[245] = RGB{95, 105, 115};
    palette[246] = RGB{220, 210, 125};
    palette[247] = RGB{70, 80, 90};
    fillGradient(palette, 248, 255, RGB{65, 65, 70}, RGB{210, 210, 215});
    for (std::size_t index = 0; index < palette.size(); ++index) {
        if (isReservedPaletteIndex(static_cast<int>(index))) {
            palette[index] = RGB{255, 0, 255};
        }
    }
    return palette;
}
