#include "auts_io.h"

#include "default_palette.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t kWidth = 320;
constexpr std::size_t kHeight = 400;
constexpr std::size_t kPixelCount = kWidth * kHeight;
constexpr std::size_t kConverterTrailerSize = 8;
constexpr std::uint8_t kBorderIndex = 7;

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open file");
    }
    const std::streampos end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) >
                       std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("could not determine file size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read the complete file");
        }
    }
    return bytes;
}

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes,
                      const std::size_t offset)
{
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("truncated BMP header");
    }
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes,
                      const std::size_t offset)
{
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("truncated BMP header");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 8 |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16 |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24;
}

void applyBorder(Level::PixelBuffer& pixels)
{
    for (std::size_t y = 0; y < kHeight; ++y) {
        for (std::size_t x = 0; x < kWidth; ++x) {
            if (x < 2 || x >= kWidth - 2 || y < 2 || y >= kHeight - 2) {
                pixels[y * kWidth + x] = kBorderIndex;
            }
        }
    }
}

void initializeAutsLevelFromPixels(Level& level, Level::PixelBuffer pixels,
                                   const std::filesystem::path& path)
{
    level.resize(kWidth, kHeight);
    level.name = path.stem().string();
    level.palette = defaultAutsPalette();
    level.pixels = std::move(pixels);
    level.layers.clear();
    level.activeLayer = 0;
}

} // namespace

void initializeBlankAutsLevel(Level& level)
{
    level.resize(kWidth, kHeight);
    level.name = "UNTITLED";
    level.palette = defaultAutsPalette();
    level.pixels.fill(0);
    applyBorder(level.pixels);
}

void applyAutsGameRules(Level& level)
{
    if (level.width != kWidth || level.height != kHeight ||
        level.pixels.size() != kPixelCount) {
        throw std::invalid_argument(
            "AUTS levels must be exactly 320 x 400 pixels");
    }
    level.palette = defaultAutsPalette();
    applyBorder(level.pixels);
    level.layers.clear();
    level.activeLayer = 0;
}

bool loadAutsLev(const std::filesystem::path& path, Level& level,
                 std::string& error)
{
    error.clear();
    try {
        const std::vector<std::uint8_t> bytes = readFile(path);
        if (bytes.size() <= kConverterTrailerSize) {
            throw std::runtime_error("file is too small to contain an AUTS level");
        }

        Level::PixelBuffer pixels(kPixelCount);
        std::size_t inputOffset = 0;
        std::size_t outputOffset = 0;
        while (outputOffset < kPixelCount) {
            if (inputOffset >= bytes.size()) {
                throw std::runtime_error("truncated AUTS RLE stream");
            }
            const std::uint8_t value = bytes[inputOffset++];
            pixels[outputOffset++] = value;
            if (outputOffset < kPixelCount && inputOffset < bytes.size() &&
                bytes[inputOffset] == value) {
                ++inputOffset;
                if (inputOffset >= bytes.size()) {
                    throw std::runtime_error("truncated AUTS RLE run");
                }
                const std::size_t repeated = bytes[inputOffset++];
                if (repeated == 0 || repeated > kPixelCount - outputOffset) {
                    throw std::runtime_error("invalid AUTS RLE run length");
                }
                std::fill_n(pixels.begin() +
                                static_cast<std::ptrdiff_t>(outputOffset),
                            repeated, value);
                outputOffset += repeated;
            }
        }
        if (bytes.size() - inputOffset != kConverterTrailerSize) {
            throw std::runtime_error(
                "AUTS level does not have the expected 8-byte converter trailer");
        }
        initializeAutsLevelFromPixels(level, std::move(pixels), path);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool saveAutsLev(const std::filesystem::path& path, const Level& level,
                 std::string& error)
{
    error.clear();
    try {
        if (level.width != kWidth || level.height != kHeight ||
            level.pixels.size() != kPixelCount) {
            throw std::runtime_error("AUTS levels must be exactly 320 x 400 pixels");
        }

        Level::PixelBuffer pixels = level.pixels;
        // BMP2LEV always creates a two-pixel indestructible boundary.
        applyBorder(pixels);

        std::vector<std::uint8_t> encoded;
        encoded.reserve(kPixelCount + kConverterTrailerSize);
        const auto encodeRange = [&](const std::size_t begin,
                                     const std::size_t end) {
            std::size_t offset = begin;
            while (offset < end) {
                const std::uint8_t value = pixels[offset];
                std::size_t runLength = 1;
                while (offset + runLength < end &&
                       pixels[offset + runLength] == value) {
                    ++runLength;
                }
                std::size_t remaining = runLength;
                while (remaining >= 2) {
                    const std::size_t chunk =
                        std::min<std::size_t>(remaining, 256);
                    encoded.push_back(value);
                    encoded.push_back(value);
                    encoded.push_back(
                        static_cast<std::uint8_t>(chunk - 1));
                    remaining -= chunk;
                }
                if (remaining == 1) {
                    encoded.push_back(value);
                }
                offset += runLength;
            }
        };
        // BMP2LEV processes and previews the upper and lower 200-row halves
        // separately, so a run is also split at the midpoint.
        encodeRange(0, kPixelCount / 2);
        encodeRange(kPixelCount / 2, kPixelCount);

        // The original converter writes eight bytes beyond its image stream.
        // Their values vary between runs and are ignored by the decoder; write
        // deterministic zeros while retaining the compatible file layout.
        encoded.insert(encoded.end(), kConverterTrailerSize, 0);

        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("could not create AUTS level");
        }
        output.write(reinterpret_cast<const char*>(encoded.data()),
                     static_cast<std::streamsize>(encoded.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("could not write the complete AUTS level");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool loadAutsBmp(const std::filesystem::path& path, Level& level,
                 bool& paletteMatches, std::string& error)
{
    error.clear();
    paletteMatches = false;
    try {
        const std::vector<std::uint8_t> bytes = readFile(path);
        if (bytes.size() < 14 + 40 || bytes[0] != 'B' || bytes[1] != 'M') {
            throw std::runtime_error("not a Windows BMP file");
        }
        const std::uint32_t pixelOffset = readU32(bytes, 10);
        const std::uint32_t dibSize = readU32(bytes, 14);
        const std::int32_t width = static_cast<std::int32_t>(readU32(bytes, 18));
        const std::int32_t signedHeight =
            static_cast<std::int32_t>(readU32(bytes, 22));
        if (dibSize < 40 || width != static_cast<std::int32_t>(kWidth) ||
            (signedHeight != static_cast<std::int32_t>(kHeight) &&
             signedHeight != -static_cast<std::int32_t>(kHeight)) ||
            readU16(bytes, 26) != 1 || readU16(bytes, 28) != 8 ||
            readU32(bytes, 30) != 0) {
            throw std::runtime_error(
                "AUTS import requires an uncompressed 320 x 400, 8-bit indexed BMP");
        }
        const std::uint32_t colorsUsed = readU32(bytes, 46);
        const std::size_t paletteCount = colorsUsed == 0 ? 256 : colorsUsed;
        const std::size_t paletteOffset = 14 + dibSize;
        if (paletteCount < 256 || paletteOffset + 256 * 4 > bytes.size() ||
            pixelOffset < paletteOffset + 256 * 4) {
            throw std::runtime_error("AUTS BMP must contain a 256-color palette");
        }
        const std::size_t rowStride = (kWidth + 3) & ~std::size_t(3);
        if (pixelOffset > bytes.size() ||
            rowStride * kHeight > bytes.size() - pixelOffset) {
            throw std::runtime_error("truncated AUTS BMP pixel data");
        }

        const auto autsPalette = defaultAutsPalette();
        paletteMatches = true;
        for (std::size_t index = 0; index < 256; ++index) {
            const std::size_t entry = paletteOffset + index * 4;
            const RGB source{bytes[entry + 2], bytes[entry + 1], bytes[entry]};
            paletteMatches = paletteMatches && source == autsPalette[index];
        }

        Level::PixelBuffer pixels(kPixelCount);
        const bool bottomUp = signedHeight > 0;
        for (std::size_t y = 0; y < kHeight; ++y) {
            const std::size_t sourceY = bottomUp ? kHeight - 1 - y : y;
            const auto begin = bytes.begin() +
                               static_cast<std::ptrdiff_t>(pixelOffset +
                                                           sourceY * rowStride);
            std::copy_n(begin, kWidth,
                        pixels.begin() + static_cast<std::ptrdiff_t>(y * kWidth));
        }
        applyBorder(pixels);
        initializeAutsLevelFromPixels(level, std::move(pixels), path);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
