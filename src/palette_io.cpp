#include "palette_io.h"

#include <fstream>
#include <string>

bool loadJascPalette(const std::filesystem::path& path,
                     std::array<RGB, 256>& palette, std::string& error)
{
    std::ifstream input(path);
    if (!input) {
        error = "Could not open palette file";
        return false;
    }

    std::string signature;
    std::string version;
    int colorCount = 0;
    if (!(input >> signature >> version >> colorCount) ||
        signature != "JASC-PAL" || version != "0100" || colorCount != 256) {
        error = "Palette must be a JASC-PAL 0100 file with 256 colors";
        return false;
    }

    std::array<RGB, 256> loaded{};
    for (RGB& color : loaded) {
        int red = 0;
        int green = 0;
        int blue = 0;
        if (!(input >> red >> green >> blue) || red < 0 || red > 255 ||
            green < 0 || green > 255 || blue < 0 || blue > 255) {
            error = "Palette contains a missing or invalid RGB value";
            return false;
        }
        color = RGB{static_cast<std::uint8_t>(red),
                    static_cast<std::uint8_t>(green),
                    static_cast<std::uint8_t>(blue)};
    }

    std::string extra;
    if (input >> extra) {
        error = "Palette contains data after the 256th color";
        return false;
    }
    palette = loaded;
    error.clear();
    return true;
}

bool saveJascPalette(const std::filesystem::path& path,
                     const std::array<RGB, 256>& palette, std::string& error)
{
    std::ofstream output(path);
    if (!output) {
        error = "Could not open palette file for writing";
        return false;
    }

    output << "JASC-PAL\n0100\n256\n";
    for (const RGB& color : palette) {
        output << static_cast<int>(color.r) << ' ' << static_cast<int>(color.g)
               << ' ' << static_cast<int>(color.b) << '\n';
    }
    if (!output) {
        error = "Could not finish writing palette file";
        return false;
    }
    error.clear();
    return true;
}
