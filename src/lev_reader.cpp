#include "lev_reader.h"

#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {

constexpr std::size_t kHeaderSize = 128;
constexpr std::size_t kPaletteMarkerSize = 1;
constexpr std::size_t kPaletteSize = 256 * 3;
constexpr std::size_t kMinimumFileSize =
    kHeaderSize + kPaletteMarkerSize + kPaletteSize;
constexpr std::size_t kNameStart = 2;
constexpr std::size_t kNameAreaEnd = 23;

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        throw std::runtime_error("could not open file");
    }

    const std::streampos end = input.tellg();
    if (end < 0) {
        throw std::runtime_error("could not determine file size");
    }

    const auto size = static_cast<std::uintmax_t>(end);
    if (size > std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("file is too large to read into memory");
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0, std::ios::beg);
    if (!bytes.empty()) {
        if (bytes.size() > static_cast<std::size_t>(
                               std::numeric_limits<std::streamsize>::max())) {
            throw std::runtime_error("file is too large for the input stream");
        }
        input.read(reinterpret_cast<char*>(bytes.data()),
                   static_cast<std::streamsize>(bytes.size()));
        if (!input) {
            throw std::runtime_error("could not read the complete file");
        }
    }

    return bytes;
}

std::string offsetString(const std::size_t offset)
{
    std::ostringstream output;
    output << "0x" << std::hex << std::uppercase << std::setw(8)
           << std::setfill('0') << offset;
    return output.str();
}

std::string readName(const std::vector<std::uint8_t>& bytes)
{
    std::string name;
    for (std::size_t offset = kNameStart; offset < kNameAreaEnd; ++offset) {
        if (bytes[offset] == 0) {
            break;
        }
        name.push_back(static_cast<char>(bytes[offset]));
    }

    while (!name.empty() && name.back() == ' ') {
        name.pop_back();
    }
    return name;
}

bool decodePixels(const std::vector<std::uint8_t>& bytes,
                  const std::size_t dataEnd,
                  std::vector<std::uint8_t>& pixels, std::string& error)
{
    pixels.clear();
    pixels.reserve(Level::PixelCount);

    std::size_t inputOffset = kHeaderSize;
    std::size_t rowPosition = 0;
    while (inputOffset < dataEnd && pixels.size() < Level::PixelCount) {
        const std::size_t tokenOffset = inputOffset;
        const std::uint8_t token = bytes[inputOffset++];
        std::size_t count = 1;
        std::uint8_t value = token;

        if ((token & 0xc0) == 0xc0) {
            count = token & 0x3f;
            if (count == 0) {
                error = "invalid zero-length RLE run at " +
                        offsetString(tokenOffset);
                return false;
            }
            if (inputOffset >= dataEnd) {
                error = "RLE run at " + offsetString(tokenOffset) +
                        " has no value byte";
                return false;
            }
            value = bytes[inputOffset++];
        }

        if (count > Level::PixelCount - pixels.size()) {
            error = "RLE data expands beyond 640 x 800 pixels at " +
                    offsetString(tokenOffset);
            return false;
        }
        if (count > Level::Width - rowPosition) {
            error = "RLE run crosses a 640-pixel scanline at " +
                    offsetString(tokenOffset);
            return false;
        }
        pixels.insert(pixels.end(), count, value);
        rowPosition = (rowPosition + count) % Level::Width;
    }

    if (pixels.size() != Level::PixelCount) {
        error = "RLE data decoded to " + std::to_string(pixels.size()) +
                " pixels; expected " + std::to_string(Level::PixelCount);
        return false;
    }
    if (inputOffset != dataEnd) {
        error = "unexpected data after the 640 x 800 pixel stream at " +
                offsetString(inputOffset);
        return false;
    }

    return true;
}

} // namespace

bool loadLev(const std::filesystem::path& path, Level& level,
             std::string& error)
{
    error.clear();

    try {
        const std::vector<std::uint8_t> bytes = readFile(path);
        if (bytes.size() < kMinimumFileSize) {
            error = "file is too small to contain a LEV header and palette";
            return false;
        }
        if (bytes[0] != 0x76 || bytes[1] != 0x07) {
            error = "unrecognized LEV signature";
            return false;
        }

        const std::size_t paletteMarkerOffset = bytes.size() - kPaletteSize - 1;
        if (bytes[paletteMarkerOffset] != 0x0c) {
            error = "missing PCX palette marker at " +
                    offsetString(paletteMarkerOffset);
            return false;
        }

        std::vector<std::uint8_t> pixels;
        if (!decodePixels(bytes, paletteMarkerOffset, pixels, error)) {
            return false;
        }

        std::array<RGB, 256> palette{};
        const std::size_t paletteOffset = paletteMarkerOffset + 1;
        for (std::size_t index = 0; index < palette.size(); ++index) {
            palette[index] = RGB{
                bytes[paletteOffset + index * 3],
                bytes[paletteOffset + index * 3 + 1],
                bytes[paletteOffset + index * 3 + 2],
            };
        }

        level.name = readName(bytes);
        std::copy(pixels.begin(), pixels.end(), level.pixels.begin());
        level.palette = palette;
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
