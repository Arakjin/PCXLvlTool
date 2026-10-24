#include "pcx_writer.h"

#include "layer_model.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

void putLittleEndian16(std::array<std::uint8_t, 128>& header,
                       const std::size_t offset, const std::uint16_t value)
{
    header[offset] = static_cast<std::uint8_t>(value & 0xff);
    header[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void writeRun(std::ofstream& output, const std::uint8_t value,
              const std::size_t length)
{
    if (length > 1 || value >= 0xc0) {
        output.put(static_cast<char>(0xc0 | length));
    }
    output.put(static_cast<char>(value));
}

void appendU32(std::vector<std::uint8_t>& output, const std::uint32_t value)
{
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8U));
    output.push_back(static_cast<std::uint8_t>(value >> 16U));
    output.push_back(static_cast<std::uint8_t>(value >> 24U));
}

void appendRun(std::vector<std::uint8_t>& output, const std::uint8_t value,
               const std::size_t length)
{
    if (length > 1 || value >= 0xc0) {
        output.push_back(static_cast<std::uint8_t>(0xc0U | length));
    }
    output.push_back(value);
}

std::vector<std::uint8_t> encodeLayerBuffer(
    const Level::PixelBuffer& buffer, const std::size_t width,
    const std::size_t height, const std::size_t bytesPerLine)
{
    std::vector<std::uint8_t> encoded;
    for (std::size_t y = 0; y < height; ++y) {
        const std::size_t rowStart = y * width;
        std::size_t x = 0;
        while (x < bytesPerLine) {
            const std::uint8_t value = x < width ? buffer[rowStart + x] : 0;
            std::size_t length = 1;
            while (x + length < bytesPerLine && length < 63) {
                const std::size_t nextX = x + length;
                const std::uint8_t nextValue =
                    nextX < width ? buffer[rowStart + nextX] : 0;
                if (nextValue != value) break;
                ++length;
            }
            appendRun(encoded, value, length);
            x += length;
        }
    }
    return encoded;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const std::streampos end = input.tellg();
    if (!input || end < 0) {
        throw std::runtime_error("could not read layered PCX base image");
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        throw std::runtime_error("could not read layered PCX base image");
    }
    return bytes;
}

} // namespace

bool savePcx(const std::filesystem::path& path, const Level& level,
             std::string& error)
{
    error.clear();

    try {
        constexpr std::size_t maximumU16 =
            std::numeric_limits<std::uint16_t>::max();
        const std::size_t bytesPerLine = (level.width + 1U) & ~std::size_t{1U};
        if (level.width == 0 || level.height == 0 ||
            level.width > maximumU16 || level.height > maximumU16 ||
            bytesPerLine > maximumU16 ||
            level.pixels.size() != level.pixelCount()) {
            throw std::runtime_error("invalid PCX image dimensions or pixels");
        }
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("could not create output file");
        }

        std::array<std::uint8_t, 128> header{};
        header[0] = 0x0a;
        header[1] = 5;
        header[2] = 1;
        header[3] = 8;
        putLittleEndian16(header, 8,
                          static_cast<std::uint16_t>(level.width - 1));
        putLittleEndian16(header, 10,
                          static_cast<std::uint16_t>(level.height - 1));
        putLittleEndian16(header, 12,
                          static_cast<std::uint16_t>(level.width));
        putLittleEndian16(header, 14,
                          static_cast<std::uint16_t>(level.height));
        header[65] = 1;
        putLittleEndian16(header, 66,
                          static_cast<std::uint16_t>(bytesPerLine));
        putLittleEndian16(header, 68, 1);
        putLittleEndian16(header, 70,
                          static_cast<std::uint16_t>(level.width));
        putLittleEndian16(header, 72,
                          static_cast<std::uint16_t>(level.height));

        output.write(reinterpret_cast<const char*>(header.data()),
                     static_cast<std::streamsize>(header.size()));

        for (std::size_t y = 0; y < level.height; ++y) {
            const std::size_t rowStart = y * level.width;
            std::size_t x = 0;
            while (x < bytesPerLine) {
                const std::uint8_t value =
                    x < level.width ? level.pixels[rowStart + x] : 0;
                std::size_t length = 1;
                while (x + length < bytesPerLine && length < 63) {
                    const std::size_t nextX = x + length;
                    const std::uint8_t nextValue =
                        nextX < level.width
                            ? level.pixels[rowStart + nextX]
                            : 0;
                    if (nextValue != value) {
                        break;
                    }
                    ++length;
                }
                writeRun(output, value, length);
                x += length;
            }
        }

        output.put(static_cast<char>(0x0c));
        for (const RGB& color : level.palette) {
            output.put(static_cast<char>(color.r));
            output.put(static_cast<char>(color.g));
            output.put(static_cast<char>(color.b));
        }

        output.close();
        if (!output) {
            throw std::runtime_error("could not write the complete PCX file");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool saveOverThrustLayeredPcx(const std::filesystem::path& path,
                              const Level& level, std::string& error)
{
    error.clear();
    try {
        if (level.layers.empty() || level.layers.size() > Level::MaxLayers ||
            level.activeLayer >= level.layers.size()) {
            throw std::runtime_error("invalid layered PCX layer metadata");
        }
        for (const Level::Layer& layer : level.layers) {
            if (layer.name.size() > 4096 ||
                layer.pixels.size() != level.pixelCount() ||
                layer.mask.size() != level.pixelCount() ||
                std::any_of(layer.mask.begin(), layer.mask.end(),
                            [](const std::uint8_t value) { return value > 1; })) {
                throw std::runtime_error("invalid layered PCX layer data");
            }
        }

        Level flattened = level;
        flattenLayers(flattened);
        if (!savePcx(path, flattened, error)) {
            throw std::runtime_error(error);
        }
        std::vector<std::uint8_t> pcx = readBytes(path);
        constexpr std::size_t paletteBytes = 1 + 256 * 3;
        if (pcx.size() < paletteBytes ||
            pcx[pcx.size() - paletteBytes] != 0x0c) {
            throw std::runtime_error("layered PCX base palette is missing");
        }

        const std::size_t bytesPerLine =
            (level.width + 1U) & ~std::size_t{1U};
        std::vector<std::uint8_t> payload;
        appendU32(payload, static_cast<std::uint32_t>(level.layers.size()));
        appendU32(payload, static_cast<std::uint32_t>(level.activeLayer));
        for (const Level::Layer& layer : level.layers) {
            const std::vector<std::uint8_t> pixels = encodeLayerBuffer(
                layer.pixels, level.width, level.height, bytesPerLine);
            const std::vector<std::uint8_t> mask = encodeLayerBuffer(
                layer.mask, level.width, level.height, bytesPerLine);
            if (pixels.size() > 0xffffffffULL ||
                mask.size() > 0xffffffffULL) {
                throw std::runtime_error("layered PCX layer is too large");
            }
            appendU32(payload, static_cast<std::uint32_t>(layer.name.size()));
            payload.insert(payload.end(), layer.name.begin(), layer.name.end());
            payload.push_back(static_cast<std::uint8_t>(
                (layer.visible ? 1U : 0U) | (layer.locked ? 2U : 0U)));
            payload.insert(payload.end(), 3, 0);
            appendU32(payload, static_cast<std::uint32_t>(pixels.size()));
            appendU32(payload, static_cast<std::uint32_t>(mask.size()));
            payload.insert(payload.end(), pixels.begin(), pixels.end());
            payload.insert(payload.end(), mask.begin(), mask.end());
        }
        if (payload.size() > 0xffffffffULL) {
            throw std::runtime_error("layered PCX metadata is too large");
        }

        const std::array<std::uint8_t, 8> magic{
            {'O', 'P', 'C', 'X', 'L', 'A', 'Y', '1'}};
        std::vector<std::uint8_t> extension(magic.begin(), magic.end());
        appendU32(extension, static_cast<std::uint32_t>(payload.size()));
        extension.insert(extension.end(), payload.begin(), payload.end());
        pcx.insert(pcx.end() - static_cast<std::ptrdiff_t>(paletteBytes),
                   extension.begin(), extension.end());

        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.write(reinterpret_cast<const char*>(pcx.data()),
                     static_cast<std::streamsize>(pcx.size()));
        output.close();
        if (!output) {
            throw std::runtime_error("could not write complete layered PCX");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
