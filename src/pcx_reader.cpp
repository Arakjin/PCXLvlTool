#include "pcx_reader.h"

#include "layer_model.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kHeaderSize = 128;
constexpr std::size_t kPaletteSize = 256 * 3;
constexpr std::size_t kMaximumDimension = 4096;
constexpr std::array<std::uint8_t, 8> kLayerMagic{
    {'O', 'P', 'C', 'X', 'L', 'A', 'Y', '1'}};

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

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes,
                      std::size_t& offset, const std::size_t end)
{
    if (offset > end || end - offset < 4) {
        throw std::runtime_error("truncated layered PCX field");
    }
    const std::uint32_t value =
        static_cast<std::uint32_t>(bytes[offset]) |
        (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
        (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
        (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    offset += 4;
    return value;
}

std::vector<std::uint8_t> decodeRle(
    const std::vector<std::uint8_t>& bytes, std::size_t offset,
    const std::size_t encodedEnd, const std::size_t decodedSize)
{
    std::vector<std::uint8_t> decoded;
    decoded.reserve(decodedSize);
    while (decoded.size() < decodedSize) {
        if (offset >= encodedEnd) {
            throw std::runtime_error("truncated layered PCX RLE stream");
        }
        const std::uint8_t token = bytes[offset++];
        std::size_t count = 1;
        std::uint8_t value = token;
        if ((token & 0xc0U) == 0xc0U) {
            count = token & 0x3fU;
            if (count == 0 || offset >= encodedEnd) {
                throw std::runtime_error("invalid layered PCX RLE token");
            }
            value = bytes[offset++];
        }
        if (count > decodedSize - decoded.size()) {
            throw std::runtime_error("layered PCX RLE expands beyond its buffer");
        }
        decoded.insert(decoded.end(), count, value);
    }
    if (offset != encodedEnd) {
        throw std::runtime_error("unused data in layered PCX RLE stream");
    }
    return decoded;
}

void readLayerExtension(const std::vector<std::uint8_t>& bytes,
                        const std::size_t extensionOffset,
                        const std::size_t paletteMarkerOffset,
                        const std::size_t bytesPerLine, Level& level)
{
    if (paletteMarkerOffset - extensionOffset < kLayerMagic.size() + 4 ||
        !std::equal(kLayerMagic.begin(), kLayerMagic.end(),
                    bytes.begin() +
                        static_cast<std::ptrdiff_t>(extensionOffset))) {
        throw std::runtime_error("unexpected data after PCX image stream");
    }
    std::size_t offset = extensionOffset + kLayerMagic.size();
    const std::uint32_t payloadSize =
        readU32(bytes, offset, paletteMarkerOffset);
    if (payloadSize != paletteMarkerOffset - offset) {
        throw std::runtime_error("invalid layered PCX block size");
    }
    const std::uint32_t layerCount =
        readU32(bytes, offset, paletteMarkerOffset);
    const std::uint32_t activeLayer =
        readU32(bytes, offset, paletteMarkerOffset);
    if (layerCount == 0 || layerCount > Level::MaxLayers ||
        activeLayer >= layerCount ||
        level.height > std::numeric_limits<std::size_t>::max() / bytesPerLine) {
        throw std::runtime_error("invalid layered PCX layer metadata");
    }
    const std::size_t decodedSize = level.height * bytesPerLine;
    std::vector<Level::Layer> layers;
    layers.reserve(layerCount);
    for (std::uint32_t index = 0; index < layerCount; ++index) {
        const std::uint32_t nameSize =
            readU32(bytes, offset, paletteMarkerOffset);
        if (nameSize > 4096 || nameSize > paletteMarkerOffset - offset) {
            throw std::runtime_error("invalid layered PCX layer name");
        }
        Level::Layer layer;
        layer.name.assign(
            reinterpret_cast<const char*>(bytes.data() + offset), nameSize);
        offset += nameSize;
        if (paletteMarkerOffset - offset < 12) {
            throw std::runtime_error("truncated layered PCX layer record");
        }
        const std::uint8_t flags = bytes[offset++];
        if ((flags & ~3U) != 0 || bytes[offset] != 0 ||
            bytes[offset + 1] != 0 || bytes[offset + 2] != 0) {
            throw std::runtime_error("invalid layered PCX layer flags");
        }
        offset += 3;
        const std::uint32_t pixelSize =
            readU32(bytes, offset, paletteMarkerOffset);
        const std::uint32_t maskSize =
            readU32(bytes, offset, paletteMarkerOffset);
        if (pixelSize > paletteMarkerOffset - offset) {
            throw std::runtime_error("truncated layered PCX pixel stream");
        }
        const std::vector<std::uint8_t> pixels = decodeRle(
            bytes, offset, offset + pixelSize, decodedSize);
        offset += pixelSize;
        if (maskSize > paletteMarkerOffset - offset) {
            throw std::runtime_error("truncated layered PCX mask stream");
        }
        const std::vector<std::uint8_t> mask = decodeRle(
            bytes, offset, offset + maskSize, decodedSize);
        offset += maskSize;
        layer.visible = (flags & 1U) != 0;
        layer.locked = (flags & 2U) != 0;
        layer.pixels.resize(level.pixelCount());
        layer.mask.resize(level.pixelCount());
        for (std::size_t y = 0; y < level.height; ++y) {
            std::copy_n(pixels.begin() + static_cast<std::ptrdiff_t>(
                                             y * bytesPerLine),
                        level.width,
                        layer.pixels.begin() +
                            static_cast<std::ptrdiff_t>(y * level.width));
            std::copy_n(mask.begin() + static_cast<std::ptrdiff_t>(
                                           y * bytesPerLine),
                        level.width,
                        layer.mask.begin() +
                            static_cast<std::ptrdiff_t>(y * level.width));
        }
        if (std::any_of(layer.mask.begin(), layer.mask.end(),
                        [](const std::uint8_t value) { return value > 1; })) {
            throw std::runtime_error("invalid layered PCX transparency mask");
        }
        layers.push_back(std::move(layer));
    }
    if (offset != paletteMarkerOffset) {
        throw std::runtime_error("unused data in layered PCX block");
    }

    const Level::PixelBuffer flattened = level.pixels;
    level.layers = std::move(layers);
    level.activeLayer = activeLayer;
    flattenLayers(level);
    if (level.pixels != flattened) {
        throw std::runtime_error(
            "layered PCX composite does not match its base image");
    }
}

} // namespace

static bool loadPcxImpl(const std::vector<std::uint8_t>& bytes,
                        const std::string& name, Level& level,
                        const bool requireLayers, std::string& error)
{
    error.clear();
    try {
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
        level.resize(width, height);
        level.name = name;
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
        if (inputOffset != paletteMarkerOffset && !requireLayers) {
            throw std::runtime_error(
                "OverThrust layered image must be opened as OPCX");
        }
        if (inputOffset != paletteMarkerOffset) {
            readLayerExtension(bytes, inputOffset, paletteMarkerOffset,
                               bytesPerLine, level);
        } else if (requireLayers) {
            throw std::runtime_error("OPCX layer block is missing");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool loadPcx(const std::filesystem::path& path, Level& level,
             std::string& error)
{
    try {
        return loadPcxImpl(readFile(path), path.stem().string(), level, false,
                           error);
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool loadOverThrustLayeredPcx(const std::filesystem::path& path, Level& level,
                              std::string& error)
{
    try {
        return loadPcxImpl(readFile(path), path.stem().string(), level, true,
                           error);
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool loadPcx(const std::vector<std::uint8_t>& bytes, const std::string& name,
             Level& level, std::string& error)
{
    return loadPcxImpl(bytes, name, level, false, error);
}

bool loadOverThrustLayeredPcx(const std::vector<std::uint8_t>& bytes,
                              const std::string& name, Level& level,
                              std::string& error)
{
    return loadPcxImpl(bytes, name, level, true, error);
}

