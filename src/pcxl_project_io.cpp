#include "pcxl_project_io.h"

#include "default_palette.h"
#include "layer_model.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace {

constexpr std::array<char, 8> kMagic{{'P', 'X', 'L', 'P', 'R', 'J', '1', 0}};
constexpr std::uint32_t kVersion1 = 1;
constexpr std::uint32_t kVersion2 = 2;
constexpr std::uint32_t kVersion3 = 3;
constexpr std::uint32_t kMaximumStringLength = 4096;
constexpr std::uint32_t kMaximumDimension = 4096;
constexpr std::uint32_t kMaximumSettings = 128;
constexpr std::uint32_t kMaximumObjects = 4096;
constexpr std::uint32_t kMaximumChannels = 256;
constexpr std::uint32_t kMaximumListItems = 256;
constexpr std::uint32_t kMaximumBackgroundBytes = 128U * 1024U * 1024U;

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

void writeI32(std::ostream& output, const int value)
{
    writeU32(output, static_cast<std::uint32_t>(value));
}

int readI32(std::istream& input)
{
    return static_cast<int>(static_cast<std::int32_t>(readU32(input)));
}

void writeBool(std::ostream& output, const bool value)
{
    output.put(static_cast<char>(value ? 1 : 0));
}

bool readBool(std::istream& input)
{
    const int value = input.get();
    if (value != 0 && value != 1) {
        throw std::runtime_error("invalid PXLP boolean");
    }
    return value != 0;
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

void writeStringList(std::ostream& output,
                     const std::vector<std::string>& values)
{
    if (values.size() > kMaximumListItems) {
        throw std::runtime_error("PXLP list has too many values");
    }
    writeU32(output, static_cast<std::uint32_t>(values.size()));
    for (const std::string& value : values) {
        writeString(output, value);
    }
}

std::vector<std::string> readStringList(std::istream& input)
{
    const std::uint32_t count = readU32(input);
    if (count > kMaximumListItems) {
        throw std::runtime_error("PXLP list has too many values");
    }
    std::vector<std::string> values;
    values.reserve(count);
    for (std::uint32_t index = 0; index < count; ++index) {
        values.push_back(readString(input));
    }
    return values;
}

void writeDouble(std::ostream& output, const double value)
{
    writeString(output, std::to_string(value));
}

double readDouble(std::istream& input)
{
    const std::string encoded = readString(input);
    std::size_t parsed = 0;
    const double value = std::stod(encoded, &parsed);
    if (parsed != encoded.size() || !std::isfinite(value)) {
        throw std::runtime_error("invalid PXLP decimal");
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
    for (std::size_t layerIndex = 0;
         layerIndex < document.layers.size(); ++layerIndex) {
        const Level::Layer& layer = document.layers[layerIndex];
        writeString(output, layer.name);
        output.put(static_cast<char>((layer.visible ? 1 : 0) |
                                     (layer.locked ? 2 : 0)));
        output.write(reinterpret_cast<const char*>(layer.pixels.data()),
                     static_cast<std::streamsize>(layer.pixels.size()));
        std::vector<std::uint8_t> canonicalMask = layer.mask;
        for (std::size_t pixel = 0; pixel < canonicalMask.size(); ++pixel) {
            canonicalMask[pixel] =
                layerIndex > 0 && layer.pixels[pixel] == 0
                    ? 0
                    : canonicalMask[pixel] == 0 ? 0 : 1;
        }
        output.write(reinterpret_cast<const char*>(canonicalMask.data()),
                     static_cast<std::streamsize>(canonicalMask.size()));
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
    for (std::size_t layerIndex = 0; layerIndex < document->layers.size();
         ++layerIndex) {
        Level::Layer& layer = document->layers[layerIndex];
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
        for (std::size_t pixel = 0; pixel < layer.mask.size(); ++pixel) {
            layer.mask[pixel] =
                layerIndex > 0 && layer.pixels[pixel] == 0
                    ? 0
                    : layer.mask[pixel] == 0 ? 0 : 1;
        }
    }
    document->layers.front().name = "Background";
    document->layers.front().visible = true;
    document->layers.front().mask.fill(1);
    flattenLayers(*document);
    return {std::move(role), std::move(document)};
}

void writeOverThrustData(std::ostream& output,
                         const OverThrustProjectData& data)
{
    writeString(output, "overthrust-data-v4");
    writeString(output, data.displayName);
    writeString(output, data.author);
    writeString(output, data.description);
    writeBool(output, data.backgroundParallax);
    writeDouble(output, data.backgroundScrollX);
    writeDouble(output, data.backgroundScrollY);
    writeStringList(output, data.allowedGameModes);
    writeI32(output, data.captureTheFlagPlayers);

    writeBool(output, !data.background.empty());
    if (!data.background.empty()) {
        const std::uint64_t expected =
            static_cast<std::uint64_t>(data.background.width) *
            data.background.height * 4ULL;
        if (data.background.width == 0 || data.background.height == 0 ||
            data.background.width > 8192 || data.background.height > 8192 ||
            expected != data.background.rgba.size() ||
            expected > kMaximumBackgroundBytes) {
            throw std::runtime_error("invalid PXLP OverThrust background");
        }
        writeU32(output, data.background.width);
        writeU32(output, data.background.height);
        writeU32(output,
                 static_cast<std::uint32_t>(data.background.rgba.size()));
        output.write(
            reinterpret_cast<const char*>(data.background.rgba.data()),
            static_cast<std::streamsize>(data.background.rgba.size()));
        output.write(
            reinterpret_cast<const char*>(data.background.contentHash.data()),
            static_cast<std::streamsize>(data.background.contentHash.size()));
        writeString(output, data.background.sourceProvenance);
    }

    if (data.channels.size() > kMaximumChannels) {
        throw std::runtime_error("too many PXLP OverThrust channels");
    }
    writeU32(output, static_cast<std::uint32_t>(data.channels.size()));
    for (const OverThrustChannel& channel : data.channels) {
        writeString(output, channel.id);
        writeBool(output, channel.initiallyActive);
    }

    if (data.objects.size() > kMaximumObjects) {
        throw std::runtime_error("too many PXLP OverThrust objects");
    }
    writeU32(output, static_cast<std::uint32_t>(data.objects.size()));
    for (const OverThrustObject& object : data.objects) {
        writeString(output, object.id);
        writeString(output, object.type);
        writeI32(output, object.transform.x);
        writeI32(output, object.transform.y);
        writeI32(output, object.transform.width);
        writeI32(output, object.transform.height);
        writeI32(output, object.transform.rotationMillidegrees);
        writeString(output, object.appearanceId);
        writeI32(output, object.openOffsetX);
        writeI32(output, object.openOffsetY);
        writeDouble(output, object.speedPixelsPerSecond);
        writeString(output, object.initialState);
        writeString(output, object.activationMode);
        writeDouble(output, object.activationRadiusPixels);
        writeString(output, object.activationChannel);
        writeString(output, object.materialId);
        writeDouble(output, object.radiusPixels);
        writeString(output, object.targetObjectId);
        writeString(output, object.velocityMode);
        writeString(output, object.serviceProfileId);
        writeStringList(output, object.services);
        writeBool(output, object.access.everyone);
        if (object.access.playerSlots.size() > 4) {
            throw std::runtime_error("too many PXLP access slots");
        }
        writeU32(output,
                 static_cast<std::uint32_t>(object.access.playerSlots.size()));
        for (const int slot : object.access.playerSlots) {
            writeI32(output, slot);
        }
        writeI32(output, object.teamSlot);
        writeString(output, object.baseProfileId);
        writeI32(output, object.objectiveAnchorX);
        writeI32(output, object.objectiveAnchorY);
        if (object.areaPolygon.size() > kMaximumListItems) {
            throw std::runtime_error("too many PXLP team-base polygon points");
        }
        writeU32(output,
                 static_cast<std::uint32_t>(object.areaPolygon.size()));
        for (const OverThrustPoint& point : object.areaPolygon) {
            if (point.x < 0 || point.x > 4096 || point.y < 0 ||
                point.y > 4096) {
                throw std::runtime_error(
                    "invalid PXLP team-base polygon point");
            }
            writeI32(output, point.x);
            writeI32(output, point.y);
        }
        if ((object.type == "team_base" && object.areaPolygon.size() < 3) ||
            (object.type != "team_base" && !object.areaPolygon.empty())) {
            throw std::runtime_error(
                "PXLP team-base polygon is missing or unexpected");
        }
        if (object.unknownProperties.size() > kMaximumListItems) {
            throw std::runtime_error("too many unknown PXLP object properties");
        }
        writeU32(output, static_cast<std::uint32_t>(
                             object.unknownProperties.size()));
        for (const OverThrustTaggedProperty& property :
             object.unknownProperties) {
            writeString(output, property.name);
            writeString(output, property.type);
            writeString(output, property.value);
        }
    }

    if (data.teamModeSettings.size() > 2) {
        throw std::runtime_error("too many PXLP team-mode settings");
    }
    writeU32(output,
             static_cast<std::uint32_t>(data.teamModeSettings.size()));
    for (const OverThrustTeamModeSetting& mode : data.teamModeSettings) {
        writeString(output, mode.gameModeId);
        if (mode.teams.size() > 4) {
            throw std::runtime_error("too many PXLP team bindings");
        }
        writeU32(output, static_cast<std::uint32_t>(mode.teams.size()));
        for (const OverThrustTeamBinding& team : mode.teams) {
            writeI32(output, team.teamSlot);
            writeString(output, team.baseObjectId);
        }
    }
}

std::unique_ptr<OverThrustProjectData> readOverThrustData(std::istream& input)
{
    const std::string sectionVersion = readString(input);
    const bool hasAreaPolygons = sectionVersion == "overthrust-data-v2" ||
                                 sectionVersion == "overthrust-data-v3" ||
                                 sectionVersion == "overthrust-data-v4";
    const bool hasParallaxFlag = sectionVersion == "overthrust-data-v3" ||
                                 sectionVersion == "overthrust-data-v4";
    const bool hasUnifiedName = sectionVersion == "overthrust-data-v4";
    if (!hasAreaPolygons && sectionVersion != "overthrust-data-v1") {
        throw std::runtime_error("unsupported PXLP OverThrust data section");
    }
    auto data = std::make_unique<OverThrustProjectData>();
    if (hasUnifiedName) {
        data->displayName = readString(input);
        data->levelId = data->displayName;
    } else {
        data->levelId = readString(input);
        data->displayName = readString(input);
    }
    data->author = readString(input);
    data->description = readString(input);
    int legacyMinimumPlayers = 1;
    int legacyMaximumPlayers = 4;
    if (hasParallaxFlag) {
        data->backgroundParallax = readBool(input);
    } else {
        legacyMinimumPlayers = readI32(input);
        legacyMaximumPlayers = readI32(input);
    }
    data->backgroundScrollX = readDouble(input);
    data->backgroundScrollY = readDouble(input);
    data->allowedGameModes = readStringList(input);
    if (hasUnifiedName) {
        data->captureTheFlagPlayers = readI32(input);
    } else {
        data->levelId = data->displayName;
    }
    if (data->displayName.empty() || data->displayName.size() > 256 ||
        data->author.size() > 256 || data->description.size() > 4096 ||
        legacyMinimumPlayers < 1 || legacyMinimumPlayers > 4 ||
        legacyMaximumPlayers < 1 || legacyMaximumPlayers > 4 ||
        legacyMinimumPlayers > legacyMaximumPlayers ||
        data->backgroundScrollX < 0 || data->backgroundScrollX > 4 ||
        data->backgroundScrollY < 0 || data->backgroundScrollY > 4 ||
        data->captureTheFlagPlayers < 2 ||
        data->captureTheFlagPlayers > 4 ||
        data->allowedGameModes.empty()) {
        throw std::runtime_error("invalid PXLP OverThrust settings");
    }

    if (readBool(input)) {
        data->background.width = readU32(input);
        data->background.height = readU32(input);
        const std::uint32_t size = readU32(input);
        const std::uint64_t expected =
            static_cast<std::uint64_t>(data->background.width) *
            data->background.height * 4ULL;
        if (data->background.width == 0 || data->background.height == 0 ||
            data->background.width > 8192 ||
            data->background.height > 8192 || size != expected ||
            size > kMaximumBackgroundBytes) {
            throw std::runtime_error("invalid PXLP OverThrust background");
        }
        data->background.rgba.resize(size);
        requireRead(input,
                    reinterpret_cast<char*>(data->background.rgba.data()),
                    data->background.rgba.size());
        requireRead(input,
                    reinterpret_cast<char*>(data->background.contentHash.data()),
                    data->background.contentHash.size());
        data->background.sourceProvenance = readString(input);
    }

    const std::uint32_t channelCount = readU32(input);
    if (channelCount > kMaximumChannels) {
        throw std::runtime_error("too many PXLP OverThrust channels");
    }
    data->channels.resize(channelCount);
    for (OverThrustChannel& channel : data->channels) {
        channel.id = readString(input);
        channel.initiallyActive = readBool(input);
    }

    const std::uint32_t objectCount = readU32(input);
    if (objectCount > kMaximumObjects) {
        throw std::runtime_error("too many PXLP OverThrust objects");
    }
    data->objects.resize(objectCount);
    for (OverThrustObject& object : data->objects) {
        object.id = readString(input);
        object.type = readString(input);
        object.transform.x = readI32(input);
        object.transform.y = readI32(input);
        object.transform.width = readI32(input);
        object.transform.height = readI32(input);
        object.transform.rotationMillidegrees = readI32(input);
        object.appearanceId = readString(input);
        object.openOffsetX = readI32(input);
        object.openOffsetY = readI32(input);
        if (object.type == "sliding_door") {
            const double distance = std::hypot(object.openOffsetX,
                                               object.openOffsetY);
            const double radians = object.transform.rotationMillidegrees *
                3.14159265358979323846 / 180000.0;
            object.openOffsetX = static_cast<int>(std::round(
                distance * std::cos(radians)));
            object.openOffsetY = static_cast<int>(std::round(
                distance * std::sin(radians)));
        }
        object.speedPixelsPerSecond = readDouble(input);
        object.initialState = readString(input);
        object.activationMode = readString(input);
        object.activationRadiusPixels = readDouble(input);
        object.activationChannel = readString(input);
        object.materialId = readString(input);
        object.radiusPixels = readDouble(input);
        if (!hasParallaxFlag) {
            // v1/v2 stored these three measurements in world units. The v3
            // authoring model uses source pixels and leaves runtime cell
            // conversion to the game.
            object.speedPixelsPerSecond *= 4.0;
            object.activationRadiusPixels *= 4.0;
            object.radiusPixels *= 4.0;
        }
        object.targetObjectId = readString(input);
        object.velocityMode = readString(input);
        object.serviceProfileId = readString(input);
        object.services = readStringList(input);
        object.access.everyone = readBool(input);
        const std::uint32_t slotCount = readU32(input);
        if (slotCount > 4) {
            throw std::runtime_error("too many PXLP access slots");
        }
        for (std::uint32_t index = 0; index < slotCount; ++index) {
            object.access.playerSlots.push_back(readI32(input));
        }
        object.teamSlot = readI32(input);
        object.baseProfileId = readString(input);
        object.objectiveAnchorX = readI32(input);
        object.objectiveAnchorY = readI32(input);
        if (hasAreaPolygons) {
            const std::uint32_t pointCount = readU32(input);
            if (pointCount > kMaximumListItems) {
                throw std::runtime_error(
                    "too many PXLP team-base polygon points");
            }
            object.areaPolygon.resize(pointCount);
            for (OverThrustPoint& point : object.areaPolygon) {
                point.x = readI32(input);
                point.y = readI32(input);
                if (point.x < 0 || point.x > 4096 || point.y < 0 ||
                    point.y > 4096) {
                    throw std::runtime_error(
                        "invalid PXLP team-base polygon point");
                }
            }
        }
        const std::uint32_t unknownCount = readU32(input);
        if (unknownCount > kMaximumListItems) {
            throw std::runtime_error("too many unknown PXLP object properties");
        }
        object.unknownProperties.resize(unknownCount);
        for (OverThrustTaggedProperty& property : object.unknownProperties) {
            property.name = readString(input);
            property.type = readString(input);
            property.value = readString(input);
        }
        if (object.id.empty() || object.id.size() > 256 ||
            object.type.empty() || object.type.size() > 256 ||
            object.transform.x < 0 || object.transform.x > 4095 ||
            object.transform.y < 0 || object.transform.y > 4095 ||
            object.transform.width < 1 || object.transform.width > 4096 ||
            object.transform.height < 1 || object.transform.height > 4096 ||
            object.transform.rotationMillidegrees < -359999 ||
            object.transform.rotationMillidegrees > 359999 ||
            object.openOffsetX < -4096 || object.openOffsetX > 4096 ||
            object.openOffsetY < -4096 || object.openOffsetY > 4096) {
            throw std::runtime_error("invalid PXLP OverThrust object bounds");
        }
        if (!hasAreaPolygons && object.type == "team_base") {
            const int right = object.transform.x + object.transform.width;
            const int bottom = object.transform.y + object.transform.height;
            object.areaPolygon = {
                {object.transform.x, object.transform.y},
                {right, object.transform.y},
                {right, bottom},
                {object.transform.x, bottom},
            };
        } else if (hasAreaPolygons &&
                   ((object.type == "team_base" &&
                     object.areaPolygon.size() < 3) ||
                    (object.type != "team_base" &&
                     !object.areaPolygon.empty()))) {
            throw std::runtime_error(
                "PXLP team-base polygon is missing or unexpected");
        }
    }

    const std::uint32_t modeCount = readU32(input);
    if (modeCount > 2) {
        throw std::runtime_error("too many PXLP team-mode settings");
    }
    data->teamModeSettings.resize(modeCount);
    for (OverThrustTeamModeSetting& mode : data->teamModeSettings) {
        mode.gameModeId = readString(input);
        const std::uint32_t teamCount = readU32(input);
        if (teamCount > 4) {
            throw std::runtime_error("too many PXLP team bindings");
        }
        mode.teams.resize(teamCount);
        for (OverThrustTeamBinding& team : mode.teams) {
            team.teamSlot = readI32(input);
            team.baseObjectId = readString(input);
        }
    }
    return data;
}

} // namespace

bool savePxlProject(const std::filesystem::path& path, const Level& level,
                    const Level* background,
                    const LevelCreationSettings& settings,
                    std::string& error)
{
    return savePxlProject(path, level, background, settings, nullptr, error);
}

bool savePxlProject(const std::filesystem::path& path, const Level& level,
                    const Level* background,
                    const LevelCreationSettings& settings,
                    const OverThrustProjectData* overthrust,
                    std::string& error)
{
    error.clear();
    try {
        const GameProfile& profile = gameProfile(settings.game);
        if (settings.width < profile.minimumWidth ||
            settings.width > profile.maximumWidth ||
            settings.height < profile.minimumHeight ||
            settings.height > profile.maximumHeight ||
            static_cast<std::uint64_t>(settings.width) * settings.height >
                profile.maximumCells ||
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
        const bool isOverThrust = settings.game == GameId::OverThrust;
        if (parallax) {
            const auto size = wingsParallaxSize(settings.width, settings.height);
            if (settings.game != GameId::Wings || background == nullptr ||
                background->width != std::size_t(size.first) ||
                background->height != std::size_t(size.second)) {
                throw std::runtime_error("invalid PXLP parallax document");
            }
        } else if (isOverThrust) {
            if (background == nullptr ||
                background->width != level.width ||
                background->height != level.height) {
                throw std::runtime_error(
                    "invalid PXLP OverThrust background document");
            }
        } else if (background != nullptr) {
            throw std::runtime_error(
                "PXLP project has an unexpected background document");
        }
        if (isOverThrust != (overthrust != nullptr)) {
            throw std::runtime_error(
                "PXLP OverThrust project data is missing or unexpected");
        }

        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("could not create PXLP project");
        }
        output.write(kMagic.data(), static_cast<std::streamsize>(kMagic.size()));
        writeU32(output, isOverThrust ? kVersion3 : kVersion1);
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
        if (isOverThrust) {
            for (const RGB& color : background->palette) {
                output.put(static_cast<char>(color.r));
                output.put(static_cast<char>(color.g));
                output.put(static_cast<char>(color.b));
            }
        }
        writeU32(output, (parallax || isOverThrust) ? 2U : 1U);
        writeDocument(output, isOverThrust ? "materials" : "level", level);
        if (parallax || isOverThrust) {
            writeDocument(output, "background", *background);
        }
        if (isOverThrust) {
            writeOverThrustData(output, *overthrust);
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
    std::unique_ptr<OverThrustProjectData> ignored;
    return loadPxlProject(path, level, background, settings, ignored, error);
}

bool loadPxlProject(const std::filesystem::path& path, Level& level,
                    std::unique_ptr<Level>& background,
                    LevelCreationSettings& settings,
                    std::unique_ptr<OverThrustProjectData>& overthrust,
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
        const std::uint32_t version = readU32(input);
        if (magic != kMagic ||
            (version != kVersion1 && version != kVersion2 &&
             version != kVersion3)) {
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
            static_cast<std::uint64_t>(loadedSettings.width) *
                    loadedSettings.height >
                profile->maximumCells ||
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
        std::array<RGB, 256> backgroundPalette = palette;
        if (version == kVersion3) {
            for (RGB& color : backgroundPalette) {
                const int red = input.get();
                const int green = input.get();
                const int blue = input.get();
                if (red == std::char_traits<char>::eof() ||
                    green == std::char_traits<char>::eof() ||
                    blue == std::char_traits<char>::eof()) {
                    throw std::runtime_error(
                        "unexpected end of PXLP background palette");
                }
                color = {static_cast<std::uint8_t>(red),
                         static_cast<std::uint8_t>(green),
                         static_cast<std::uint8_t>(blue)};
            }
        }
        const std::uint32_t documentCount = readU32(input);
        if (documentCount == 0 || documentCount > 8) {
            throw std::runtime_error("invalid PXLP document count");
        }
        std::unique_ptr<Level> loadedLevel;
        std::unique_ptr<Level> loadedBackground;
        for (std::uint32_t index = 0; index < documentCount; ++index) {
            auto [role, document] = readDocument(input);
            document->palette = role == "background" && version == kVersion3
                ? backgroundPalette
                : palette;
            if ((role == "level" ||
                 (version >= kVersion2 && role == "materials")) &&
                !loadedLevel) {
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
        } else if (loadedSettings.game == GameId::OverThrust) {
            if (version == kVersion3 &&
                (!loadedBackground ||
                 loadedBackground->width != loadedLevel->width ||
                 loadedBackground->height != loadedLevel->height)) {
                throw std::runtime_error(
                    "PXLP OverThrust background dimensions mismatch");
            }
            if (version == kVersion2 && loadedBackground) {
                throw std::runtime_error(
                    "unexpected PXLP v2 background document");
            }
        } else if (loadedBackground) {
            throw std::runtime_error("unexpected PXLP background document");
        }
        std::unique_ptr<OverThrustProjectData> loadedOverThrust;
        if (version == kVersion2 || version == kVersion3) {
            if (loadedSettings.game != GameId::OverThrust) {
                throw std::runtime_error(
                    "invalid PXLP OverThrust profile documents");
            }
            loadedOverThrust = readOverThrustData(input);
            loadedSettings.backgroundMode =
                loadedOverThrust->background.empty()
                    ? BackgroundMode::None
                    : BackgroundMode::ExternalTrueColor;
        } else if (loadedSettings.game == GameId::OverThrust) {
            throw std::runtime_error("OverThrust requires PXLP v2 or newer");
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
        overthrust = std::move(loadedOverThrust);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
