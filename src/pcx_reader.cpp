#include "pcx_reader.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t kHeaderSize = 128;
constexpr std::size_t kPaletteSize = 256 * 3;
constexpr std::size_t kMaximumDimension = 4096;

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open PCX file");
    }
    const std::streampos end = input.tellg();
    if (end < 0 || static_cast<std::uintmax_t>(end) >
                       std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("could not determine PCX file size");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read the complete PCX file");
        }
    }
    return bytes;
}

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes,
                      const std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(bytes[offset + 1]) << 8;
}

} // namespace

bool loadPcx(const std::filesystem::path& path, Level& level,
             std::string& error)
{
    error.clear();
    try {
        const std::vector<std::uint8_t> bytes = readFile(path);
        if (bytes.size() < kHeaderSize + 1 + kPaletteSize) {
            throw std::runtime_error("PCX file is too small");
        }
        if (bytes[0] != 0x0a || bytes[2] != 1 || bytes[3] != 8 ||
            bytes[65] != 1) {
            throw std::runtime_error(
                "import requires an 8-bit, single-plane, RLE-encoded PCX");
        }
        const std::uint16_t xMin = readU16(bytes, 4);
        const std::uint16_t yMin = readU16(bytes, 6);
        const std::uint16_t xMax = readU16(bytes, 8);
        const std::uint16_t yMax = readU16(bytes, 10);
        if (xMax < xMin || yMax < yMin) {
            throw std::runtime_error("PCX has invalid image bounds");
        }
        const std::size_t width =
            static_cast<std::size_t>(xMax) - xMin + 1;
        const std::size_t height =
            static_cast<std::size_t>(yMax) - yMin + 1;
        const std::size_t bytesPerLine = readU16(bytes, 66);
        if (width == 0 || height == 0 || width > kMaximumDimension ||
            height > kMaximumDimension || bytesPerLine < width ||
            bytesPerLine > kMaximumDimension) {
            throw std::runtime_error("PCX dimensions or scanline size are invalid");
        }
        const std::size_t paletteMarkerOffset = bytes.size() - kPaletteSize - 1;
        if (bytes[paletteMarkerOffset] != 0x0c) {
            throw std::runtime_error("PCX is missing its 256-color palette");
        }
        if (height > std::numeric_limits<std::size_t>::max() / bytesPerLine) {
            throw std::runtime_error("PCX decoded image is too large");
        }
        const std::size_t decodedSize = height * bytesPerLine;
        std::vector<std::uint8_t> decoded;
        decoded.reserve(decodedSize);
        std::size_t inputOffset = kHeaderSize;
        while (decoded.size() < decodedSize) {
            if (inputOffset >= paletteMarkerOffset) {
                throw std::runtime_error("truncated PCX image data");
            }
            const std::uint8_t token = bytes[inputOffset++];
            std::size_t count = 1;
            std::uint8_t value = token;
            if ((token & 0xc0) == 0xc0) {
                count = token & 0x3f;
                if (count == 0 || inputOffset >= paletteMarkerOffset) {
                    throw std::runtime_error("invalid PCX RLE token");
                }
                value = bytes[inputOffset++];
            }
            if (count > decodedSize - decoded.size()) {
                throw std::runtime_error("PCX RLE expands beyond the image");
            }
            decoded.insert(decoded.end(), count, value);
        }
        if (inputOffset != paletteMarkerOffset) {
            throw std::runtime_error("unexpected data after PCX image stream");
        }

        level.resize(width, height);
        level.name = path.stem().string();
        for (std::size_t y = 0; y < height; ++y) {
            std::copy_n(decoded.begin() +
                            static_cast<std::ptrdiff_t>(y * bytesPerLine),
                        width,
                        level.pixels.begin() +
                            static_cast<std::ptrdiff_t>(y * width));
        }
        const std::size_t paletteOffset = paletteMarkerOffset + 1;
        for (std::size_t index = 0; index < level.palette.size(); ++index) {
            level.palette[index] = RGB{
                bytes[paletteOffset + index * 3],
                bytes[paletteOffset + index * 3 + 1],
                bytes[paletteOffset + index * 3 + 2],
            };
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

