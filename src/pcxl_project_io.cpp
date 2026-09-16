#include "pcxl_project_io.h"

#include "default_palette.h"
#include "layer_model.h"

#include <array>
#include <charconv>
#include <cstdint>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {

constexpr std::array<char, 8> kMagic{{'P', 'X', 'L', 'P', 'R', 'J', '1', 0}};
constexpr std::uint32_t kVersion = 1;
constexpr std::uint32_t kMaximumStringLength = 1024;
constexpr std::uint32_t kMaximumDimension = 4096;
constexpr std::uint32_t kMaximumSettings = 128;

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
            throw std::runtime_error("unexpected end of PXLP project");
        }
        value |= static_cast<std::uint32_t>(byte) << shift;
    }
    return value;
}

void writeString(std::ostream& output, const std::string_view value)
{
    if (value.size() > kMaximumStringLength) {
        throw std::runtime_error("PXLP project string is too long");
    }
    writeU32(output, static_cast<std::uint32_t>(value.size()));
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
}

std::string readString(std::istream& input)
{
    const std::uint32_t size = readU32(input);
    if (size > kMaximumStringLength) {
        throw std::runtime_error("PXLP project string is too long");
    }
    std::string value(size, '\0');
    input.read(value.data(), static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("unexpected end of PXLP project");
    }
    return value;
}

void requireRead(std::istream& input, char* data, const std::size_t size)
{
    input.read(data, static_cast<std::streamsize>(size));
    if (!input) {
        throw std::runtime_error("unexpected end of PXLP project");
    }
}

int parseInt(const std::unordered_map<std::string, std::string>& settings,
             const char* key)
{
    const auto found = settings.find(key);
    if (found == settings.end()) {
        throw std::runtime_error(std::string("missing PXLP setting: ") + key);
    }
    int value = 0;
    const char* begin = found->second.data();
    const char* end = begin + found->second.size();
    const auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw std::runtime_error(std::string("invalid PXLP setting: ") + key);
    }
    return value;
}

void writeSetting(std::ostream& output, const std::string_view key,
                  const std::string& value)
{
    writeString(output, key);
    writeString(output, value);
}

void validateDocument(const Level& document)
{
    if (document.width == 0 || document.height == 0 ||
        document.width > kMaximumDimension ||
        document.height > kMaximumDimension ||
        document.pixels.size() != document.pixelCount() ||
        document.layers.empty() || document.layers.size() > Level::MaxLayers ||
        document.activeLayer >= document.layers.size()) {
        throw std::runtime_error("invalid PXLP document");
    }
    for (const Level::Layer& layer : document.layers) {
        if (layer.pixels.size() != document.pixelCount() ||
            layer.mask.size() != document.pixelCount()) {
            throw std::runtime_error("invalid PXLP layer dimensions");
        }
    }
}

void writeDocument(std::ostream& output, const std::string_view role,
                   const Level& document)
{
    validateDocument(document);
    writeString(output, role);
    writeU32(output, static_cast<std::uint32_t>(document.width));
    writeU32(output, static_cast<std::uint32_t>(document.height));
    writeU32(output, static_cast<std::uint32_t>(document.layers.size()));
    writeU32(output, static_cast<std::uint32_t>(document.activeLayer));
    for (const Level::Layer& layer : document.layers) {
        writeString(output, layer.name);
        output.put(static_cast<char>((layer.visible ? 1 : 0) |
                                     (layer.locked ? 2 : 0)));
        output.write(reinterpret_cast<const char*>(layer.pixels.data()),
                     static_cast<std::streamsize>(layer.pixels.size()));
        output.write(reinterpret_cast<const char*>(layer.mask.data()),
                     static_cast<std::streamsize>(layer.mask.size()));
    }
}

std::pair<std::string, std::unique_ptr<Level>> readDocument(std::istream& input)
{
    std::string role = readString(input);
    const std::uint32_t width = readU32(input);
    const std::uint32_t height = readU32(input);
    if (width == 0 || height == 0 || width > kMaximumDimension ||
        height > kMaximumDimension) {
        throw std::runtime_error("invalid PXLP document dimensions");
    }
    const std::uint32_t layerCount = readU32(input);
    const std::uint32_t activeLayer = readU32(input);
    if (layerCount == 0 || layerCount > Level::MaxLayers ||
        activeLayer >= layerCount) {
        throw std::runtime_error("invalid PXLP layer metadata");
    }
    auto document = std::make_unique<Level>();
    document->resize(width, height);
    document->layers.resize(layerCount);
    document->activeLayer = activeLayer;
    for (Level::Layer& layer : document->layers) {
        layer.name = readString(input);
        const int flags = input.get();
        if (flags == std::char_traits<char>::eof()) {
            throw std::runtime_error("unexpected end of PXLP layer");
        }
        layer.visible = (flags & 1) != 0;
        layer.locked = (flags & 2) != 0;
        layer.pixels.resize(document->pixelCount());
        layer.mask.resize(document->pixelCount());
        requireRead(input, reinterpret_cast<char*>(layer.pixels.data()),
                    layer.pixels.size());
        requireRead(input, reinterpret_cast<char*>(layer.mask.data()),
                    layer.mask.size());
        for (std::uint8_t& mask : layer.mask) {
            mask = mask == 0 ? 0 : 1;
        }
    }
    document->layers.front().name = "Background";
    document->layers.front().visible = true;
    document->layers.front().mask.fill(1);
    flattenLayers(*document);
    return {std::move(role), std::move(document)};
}

} // namespace

bool savePxlProject(const std::filesystem::path& path, const Level& level,
                    const Level* background,
                    const LevelCreationSettings& settings,
                    std::string& error)
{
    error.clear();
    try {
        const GameProfile& profile = gameProfile(settings.game);
        if (settings.width < profile.minimumWidth ||
            settings.width > profile.maximumWidth ||
            settings.height < profile.minimumHeight ||
            settings.height > profile.maximumHeight ||
            level.width != std::size_t(settings.width) ||
            level.height != std::size_t(settings.height)) {
            throw std::runtime_error(
                "PXLP level dimensions do not match its settings");
        }
        if (settings.game == GameId::Auts &&
            level.palette != defaultAutsPalette()) {
            throw std::runtime_error(
                "AUTS PXLP project must use the fixed game palette");
        }
        const bool parallax =
            settings.backgroundMode == BackgroundMode::Parallax;
        if (parallax) {
            const auto size = wingsParallaxSize(settings.width, settings.height);
            if (settings.game != GameId::Wings || background == nullptr ||
                background->width != std::size_t(size.first) ||
                background->height != std::size_t(size.second)) {
                throw std::runtime_error("invalid PXLP parallax document");
            }
        } else if (background != nullptr) {
            throw std::runtime_error(
                "PXLP project has an unexpected background document");
        }

        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("could not create PXLP project");
        }
        output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
        writeU32(output, kVersion);
        writeString(output, profile.key);
        writeU32(output, 10); // Tagged settings; new keys can be added later.
        writeSetting(output, "name", settings.name);
        writeSetting(output, "width", std::to_string(settings.width));
        writeSetting(output, "height", std::to_string(settings.height));
        writeSetting(output, "background", parallax ? "parallax" : "none");
        writeSetting(output, "stars", settings.stars ? "1" : "0");
        writeSetting(output, "rain", std::to_string(settings.rainProbability));
        writeSetting(output, "snow", std::to_string(settings.snowProbability));
        writeSetting(output, "bombing",
                     std::to_string(settings.bombingProbability));
        writeSetting(output, "civilians", std::to_string(settings.civilians));
        writeSetting(output, "armed_civilians",
                     std::to_string(settings.armedCiviliansProbability));
        for (const RGB& color : level.palette) {
            output.put(static_cast<char>(color.r));
            output.put(static_cast<char>(color.g));
            output.put(static_cast<char>(color.b));
        }
        writeU32(output, parallax ? 2U : 1U);
        writeDocument(output, "level", level);
        if (parallax) {
            writeDocument(output, "background", *background);
        }
        output.close();
        if (!output) {
            throw std::runtime_error("could not write complete PXLP project");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}

bool loadPxlProject(const std::filesystem::path& path, Level& level,
                    std::unique_ptr<Level>& background,
                    LevelCreationSettings& settings,
                    std::string& error)
{
    error.clear();
    try {
        std::ifstream input(path, std::ios::binary);
        if (!input) {
            throw std::runtime_error("could not open PXLP project");
        }
        std::array<char, kMagic.size()> magic{};
        requireRead(input, magic.data(), magic.size());
        if (magic != kMagic || readU32(input) != kVersion) {
            throw std::runtime_error("unrecognized PXLP project format");
        }
        const std::string gameKey = readString(input);
        const GameProfile* profile = gameProfileByKey(gameKey);
        if (profile == nullptr) {
            throw std::runtime_error("PXLP project uses an unsupported game");
        }
        const std::uint32_t settingCount = readU32(input);
        if (settingCount > kMaximumSettings) {
            throw std::runtime_error("too many PXLP settings");
        }
        std::unordered_map<std::string, std::string> values;
        for (std::uint32_t index = 0; index < settingCount; ++index) {
            std::string key = readString(input);
            std::string value = readString(input);
            values.insert_or_assign(std::move(key), std::move(value));
        }
        LevelCreationSettings loadedSettings;
        loadedSettings.game = profile->id;
        loadedSettings.name = values.at("name");
        loadedSettings.width = parseInt(values, "width");
        loadedSettings.height = parseInt(values, "height");
        const std::string backgroundMode = values.at("background");
        if (backgroundMode == "parallax") {
            loadedSettings.backgroundMode = BackgroundMode::Parallax;
        } else if (backgroundMode != "none") {
            throw std::runtime_error("invalid PXLP background setting");
        }
        loadedSettings.stars = parseInt(values, "stars") != 0;
        loadedSettings.rainProbability = parseInt(values, "rain");
        loadedSettings.snowProbability = parseInt(values, "snow");
        loadedSettings.bombingProbability = parseInt(values, "bombing");
        loadedSettings.civilians = parseInt(values, "civilians");
        loadedSettings.armedCiviliansProbability =
            parseInt(values, "armed_civilians");
        if (loadedSettings.width < profile->minimumWidth ||
            loadedSettings.width > profile->maximumWidth ||
            loadedSettings.height < profile->minimumHeight ||
            loadedSettings.height > profile->maximumHeight ||
            loadedSettings.rainProbability < 0 ||
            loadedSettings.rainProbability > 100 ||
            loadedSettings.snowProbability < 0 ||
            loadedSettings.snowProbability > 100 ||
            loadedSettings.bombingProbability < 0 ||
            loadedSettings.bombingProbability > 100 ||
            loadedSettings.civilians < 0 ||
            loadedSettings.armedCiviliansProbability < 0 ||
            loadedSettings.armedCiviliansProbability > 100) {
            throw std::runtime_error("PXLP settings are out of range");
        }

        std::array<RGB, 256> palette{};
        for (RGB& color : palette) {
            const int red = input.get();
            const int green = input.get();
            const int blue = input.get();
            if (red == std::char_traits<char>::eof() ||
                green == std::char_traits<char>::eof() ||
                blue == std::char_traits<char>::eof()) {
                throw std::runtime_error("unexpected end of PXLP palette");
            }
            color = {static_cast<std::uint8_t>(red),
                     static_cast<std::uint8_t>(green),
                     static_cast<std::uint8_t>(blue)};
        }
        if (loadedSettings.game == GameId::Auts &&
            palette != defaultAutsPalette()) {
            throw std::runtime_error(
                "AUTS PXLP project does not use the fixed game palette");
        }
        const std::uint32_t documentCount = readU32(input);
        if (documentCount == 0 || documentCount > 8) {
            throw std::runtime_error("invalid PXLP document count");
        }
        std::unique_ptr<Level> loadedLevel;
        std::unique_ptr<Level> loadedBackground;
        for (std::uint32_t index = 0; index < documentCount; ++index) {
            auto [role, document] = readDocument(input);
            document->palette = palette;
            if (role == "level" && !loadedLevel) {
                loadedLevel = std::move(document);
            } else if (role == "background" && !loadedBackground) {
                loadedBackground = std::move(document);
            } else {
                throw std::runtime_error("invalid or duplicate PXLP document");
            }
        }
        if (!loadedLevel || loadedLevel->width != std::size_t(loadedSettings.width) ||
            loadedLevel->height != std::size_t(loadedSettings.height)) {
            throw std::runtime_error("PXLP main document dimensions mismatch");
        }
        const bool parallax =
            loadedSettings.backgroundMode == BackgroundMode::Parallax;
        if (parallax) {
            const auto expected = wingsParallaxSize(loadedSettings.width,
                                                    loadedSettings.height);
            if (loadedSettings.game != GameId::Wings || !loadedBackground ||
                loadedBackground->width != std::size_t(expected.first) ||
                loadedBackground->height != std::size_t(expected.second)) {
                throw std::runtime_error("PXLP parallax dimensions mismatch");
            }
        } else if (loadedBackground) {
            throw std::runtime_error("unexpected PXLP background document");
        }
        if (input.peek() != std::char_traits<char>::eof()) {
            throw std::runtime_error("unexpected data after PXLP project");
        }
        loadedLevel->name = loadedSettings.name;
        if (loadedBackground) {
            loadedBackground->name = "Background";
        }
        level = std::move(*loadedLevel);
        background = std::move(loadedBackground);
        settings = std::move(loadedSettings);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
