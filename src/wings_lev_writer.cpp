#include "wings_lev_writer.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void appendU16(std::vector<std::uint8_t>& output, const std::uint16_t value)
{
    output.push_back(static_cast<std::uint8_t>(value & 0xff));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void appendU32(std::vector<std::uint8_t>& output, const std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        output.push_back(static_cast<std::uint8_t>((value >> shift) & 0xff));
    }
}

std::vector<std::uint8_t> encodeRle(const std::vector<std::uint8_t>& pixels,
                                    const std::size_t width,
                                    const std::size_t height)
{
    if (pixels.size() != width * height) {
        throw std::runtime_error("Wings image buffer has invalid dimensions");
    }
    std::vector<std::uint8_t> encoded;
    encoded.reserve(pixels.size());
    for (std::size_t y = 0; y < height; ++y) {
        const std::size_t row = y * width;
        std::size_t x = 0;
        while (x < width) {
            const std::uint8_t value = pixels[row + x];
            std::size_t length = 1;
            while (x + length < width && length < 63 &&
                   pixels[row + x + length] == value) {
                ++length;
            }
            if (length > 1 || value >= 0xc0) {
                encoded.push_back(
                    static_cast<std::uint8_t>(0xc0 | length));
            }
            encoded.push_back(value);
            x += length;
        }
    }
    return encoded;
}

std::uint16_t checkedU16(const int value, const char* field)
{
    if (value < 0 || value > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error(std::string(field) + " is out of range");
    }
    return static_cast<std::uint16_t>(value);
}

std::uint16_t checkedPercent(const int value, const char* field)
{
    if (value < 0 || value > 100) {
        throw std::runtime_error(std::string(field) +
                                 " probability must be 0-100 percent");
    }
    return static_cast<std::uint16_t>(value);
}

void appendImage(std::vector<std::uint8_t>& output,
                 const std::vector<std::uint8_t>& pixels,
                 const std::size_t width, const std::size_t height)
{
    if (width == 0 || height == 0 ||
        width > std::numeric_limits<std::uint16_t>::max() ||
        height > std::numeric_limits<std::uint16_t>::max()) {
        throw std::runtime_error("Wings image dimensions are out of range");
    }
    const std::vector<std::uint8_t> encoded =
        encodeRle(pixels, width, height);
    if (encoded.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("Wings compressed image is too large");
    }
    appendU16(output, static_cast<std::uint16_t>(width));
    appendU16(output, static_cast<std::uint16_t>(height));
    appendU32(output, static_cast<std::uint32_t>(encoded.size()));
    output.insert(output.end(), encoded.begin(), encoded.end());
}

} // namespace

bool saveWingsLev(const std::filesystem::path& path, const Level& level,
                  const Level* background,
                  const LevelCreationSettings& settings,
                  std::string& error)
{
    error.clear();
    try {
        if (settings.game != GameId::Wings) {
            throw std::runtime_error("document is not a Wings level");
        }
        if (level.width < 157 || level.height < 90) {
            throw std::runtime_error(
                "Wings levels must be at least 157 x 90 pixels");
        }
        if (level.width != static_cast<std::size_t>(settings.width) ||
            level.height != static_cast<std::size_t>(settings.height)) {
            throw std::runtime_error(
                "Wings level dimensions do not match its settings");
        }
        if (background != nullptr && background->palette != level.palette) {
            throw std::runtime_error(
                "Wings level and background must use the same palette");
        }

        const std::vector<std::uint8_t> mainPixels(level.pixels.begin(),
                                                   level.pixels.end());

        std::vector<std::uint8_t> output;
        output.reserve(768 + mainPixels.size() + 32);
        for (const RGB& color : level.palette) {
            output.push_back(static_cast<std::uint8_t>(color.r >> 2));
            output.push_back(static_cast<std::uint8_t>(color.g >> 2));
            output.push_back(static_cast<std::uint8_t>(color.b >> 2));
        }
        appendImage(output, mainPixels, level.width, level.height);

        const bool parallax =
            settings.backgroundMode == BackgroundMode::Parallax;
        output.push_back(parallax ? 1 : 0);
        if (parallax) {
            const auto required = wingsParallaxSize(settings.width,
                                                    settings.height);
            if (background == nullptr ||
                background->width != static_cast<std::size_t>(required.first) ||
                background->height != static_cast<std::size_t>(required.second)) {
                throw std::runtime_error(
                    "Wings parallax background has invalid dimensions");
            }
            appendImage(output, background->pixels, background->width,
                        background->height);
        }

        output.push_back(settings.stars ? 1 : 0);
        appendU16(output, 2); // Constant used by MAKELEV 1.40 output.
        appendU16(output, checkedPercent(settings.rainProbability, "rain"));
        appendU16(output, checkedPercent(settings.snowProbability, "snow"));
        appendU16(output, checkedPercent(settings.bombingProbability,
                                         "bombing"));
        appendU16(output, checkedU16(settings.civilians, "civilians"));
        appendU16(output, checkedPercent(settings.armedCiviliansProbability,
                                         "armed civilians"));

        std::ofstream file(path, std::ios::binary);
        if (!file) {
            throw std::runtime_error("could not create output file");
        }
        file.write(reinterpret_cast<const char*>(output.data()),
                   static_cast<std::streamsize>(output.size()));
        file.close();
        if (!file) {
            throw std::runtime_error("could not write the complete Wings LEV");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
