#include "project_io.h"

#include "layer_model.h"

#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::array<char, 8> kMagic{{'V', 'W', 'P', 'R', 'O', 'J', '1', 0}};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kMaximumStringLength = 1024;

void writeU32(std::ostream& output, const std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8) {
        output.put(static_cast<char>((value >> shift) & 0xff));
    }
}

std::uint32_t readU32(std::istream& input)
{
    std::uint32_t value = 0;
    for (int shift = 0; shift < 32; shift += 8) {
        const int byte = input.get();
        if (byte == std::char_traits<char>::eof()) {
            throw std::runtime_error("unexpected end of project file");
        }
        value |= static_cast<std::uint32_t>(byte) << shift;
    }
    return value;
}

void writeString(std::ostream& output, const std::string& value)
{
    if (value.size() > kMaximumStringLength) {
        throw std::runtime_error("project string is too long");
    }
    writeU32(output, static_cast<std::uint32_t>(value.size()));
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::string readString(std::istream& input)
{
    const std::uint32_t size = readU32(input);
    if (size > kMaximumStringLength) {
        throw std::runtime_error("project string is too long");
    }
    std::string value(size, '\0');
    input.read(value.data(), static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("unexpected end of project file");
    }
    return value;
}

void requireRead(std::istream& input, char* data, const std::size_t size)
{
    input.read(data, static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("unexpected end of project file");
    }
}

} // namespace

bool saveProject(const std::filesystem::path& path, const Level& level,
                 std::string& error)
{
    error.clear();
    try {
        if (level.layers.empty() || level.layers.size() > Level::MaxLayers) {
            throw std::runtime_error("project must contain 1-5 layers");
        }
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("could not create project file");
        }
        output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
        writeU32(output, kVersion);
        writeU32(output, static_cast<std::uint32_t>(Level::Width));
        writeU32(output, static_cast<std::uint32_t>(Level::Height));
        writeString(output, level.name);
        for (const RGB& color : level.palette) {
            output.put(static_cast<char>(color.r));
            output.put(static_cast<char>(color.g));
            output.put(static_cast<char>(color.b));
        }
        writeU32(output, static_cast<std::uint32_t>(level.layers.size()));
        writeU32(output, static_cast<std::uint32_t>(level.activeLayer));
        for (const Level::Layer& layer : level.layers) {
            writeString(output, layer.name);
            output.put(static_cast<char>((layer.visible ? 1 : 0) |
                                         (layer.locked ? 2 : 0)));
            output.write(reinterpret_cast<const char*>(layer.pixels.data()),
                         static_cast<std::streamsize>(layer.pixels.size()));
            output.write(reinterpret_cast<const char*>(layer.mask.data()),
                         static_cast<std::streamsize>(layer.mask.size()));
        }
        output.close();
        if (!output) {
            throw std::runtime_error("could not write the complete project");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool loadProject(const std::filesystem::path& path, Level& level,
                 std::string& error)
{
    error.clear();
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("could not open project file");
        }
        std::array<char, kMagic.size()> magic{};
        requireRead(input, magic.data(), magic.size());
        if (magic != kMagic || readU32(input) != kVersion) {
            throw std::runtime_error("unrecognized project format");
        }
        if (readU32(input) != Level::Width ||
            readU32(input) != Level::Height) {
            throw std::runtime_error("project dimensions are not 640 x 800");
        }

        Level loaded;
        loaded.name = readString(input);
        for (RGB& color : loaded.palette) {
            const int red = input.get();
            const int green = input.get();
            const int blue = input.get();
            if (red == std::char_traits<char>::eof() ||
                green == std::char_traits<char>::eof() ||
                blue == std::char_traits<char>::eof()) {
                throw std::runtime_error("unexpected end of project palette");
            }
            color = {static_cast<std::uint8_t>(red),
                     static_cast<std::uint8_t>(green),
                     static_cast<std::uint8_t>(blue)};
        }
        const std::uint32_t layerCount = readU32(input);
        const std::uint32_t activeLayer = readU32(input);
        if (layerCount == 0 || layerCount > Level::MaxLayers ||
            activeLayer >= layerCount) {
            throw std::runtime_error("project has invalid layer metadata");
        }
        loaded.layers.resize(layerCount);
        loaded.activeLayer = activeLayer;
        for (Level::Layer& layer : loaded.layers) {
            layer.name = readString(input);
            const int flags = input.get();
            if (flags == std::char_traits<char>::eof()) {
                throw std::runtime_error("unexpected end of project layer");
            }
            layer.visible = (flags & 1) != 0;
            layer.locked = (flags & 2) != 0;
            requireRead(input, reinterpret_cast<char*>(layer.pixels.data()),
                        layer.pixels.size());
            requireRead(input, reinterpret_cast<char*>(layer.mask.data()),
                        layer.mask.size());
            for (std::uint8_t& mask : layer.mask) {
                mask = mask == 0 ? 0 : 1;
            }
        }
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("unexpected data after project layers");
        }
        loaded.layers.front().name = "Background";
        loaded.layers.front().visible = true;
        loaded.layers.front().mask.fill(1);
        flattenLayers(loaded);
        level = std::move(loaded);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
