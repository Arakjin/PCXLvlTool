#include "overthrust_package.h"

#include "canonical_png.h"
#include "overthrust_profile.h"
#include "pcx_reader.h"
#include "pcx_writer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

constexpr std::uint64_t kMaximumCells = 4194304ULL;
constexpr std::uintmax_t kMaximumPcxBytes = 16777216ULL;
constexpr std::uintmax_t kMaximumLayeredPcxBytes = 536870912ULL;

std::string jsonString(const std::string_view value)
{
    std::ostringstream output;
    output << '"';
    for (const unsigned char byte : value) {
        switch (byte) {
        case '"': output << "\\\""; break;
        case '\\': output << "\\\\"; break;
        case '\b': output << "\\b"; break;
        case '\f': output << "\\f"; break;
        case '\n': output << "\\n"; break;
        case '\r': output << "\\r"; break;
        case '\t': output << "\\t"; break;
        default:
            if (byte < 0x20) {
                output << "\\u" << std::hex << std::setw(4)
                       << std::setfill('0') << static_cast<int>(byte)
                       << std::dec;
            } else {
                output << static_cast<char>(byte);
            }
        }
    }
    output << '"';
    return output.str();
}

std::string decimal(const double value)
{
    if (!std::isfinite(value)) {
        throw std::runtime_error("non-finite decimal value");
    }
    std::ostringstream output;
    output << std::fixed << std::setprecision(6) << value;
    std::string result = output.str();
    while (result.size() > 2 && result.back() == '0' &&
           result[result.size() - 2] != '.') {
        result.pop_back();
    }
    return result;
}

bool validId(const std::string& id, const std::size_t maximum = 256)
{
    if (id.empty() || id.size() > maximum ||
        !std::isalnum(static_cast<unsigned char>(id.front()))) {
        return false;
    }
    return std::all_of(id.begin(), id.end(), [](const char character) {
        const unsigned char byte = static_cast<unsigned char>(character);
        return std::isalnum(byte) || character == ':' || character == '.' ||
               character == '_' || character == '/' || character == '-';
    });
}

void validateTransform(const OverThrustTransform& transform,
                       const std::size_t levelWidth,
                       const std::size_t levelHeight, const int offsetX = 0,
                       const int offsetY = 0)
{
    if (transform.width <= 0 || transform.height <= 0 ||
        transform.rotationMillidegrees < -359999 ||
        transform.rotationMillidegrees > 359999) {
        throw std::runtime_error("object has an invalid transform");
    }
    const double centerX = transform.x + offsetX + transform.width / 2.0;
    const double centerY = transform.y + offsetY + transform.height / 2.0;
    const double radians = transform.rotationMillidegrees *
                           3.14159265358979323846 / 180000.0;
    const double sine = std::sin(radians);
    const double cosine = std::cos(radians);
    for (const int xSign : {-1, 1}) {
        for (const int ySign : {-1, 1}) {
            const double x = xSign * transform.width / 2.0;
            const double y = ySign * transform.height / 2.0;
            const double rotatedX = centerX + x * cosine - y * sine;
            const double rotatedY = centerY + x * sine + y * cosine;
            if (rotatedX < -0.000001 || rotatedY < -0.000001 ||
                rotatedX > levelWidth + 0.000001 ||
                rotatedY > levelHeight + 0.000001) {
                throw std::runtime_error(
                    "rotated object footprint is outside the level");
            }
        }
    }
}

void validateAccess(const OverThrustAccess& access)
{
    if (access.everyone) {
        if (!access.playerSlots.empty()) {
            throw std::runtime_error("everyone access has player slots");
        }
        return;
    }
    std::set<int> playerSlotSet;
    for (const int slot : access.playerSlots) {
        if (slot < 1 || slot > 4 ||
            !playerSlotSet.insert(slot).second) {
            throw std::runtime_error("invalid private player-slot access");
        }
    }
    if (playerSlotSet.empty()) {
        throw std::runtime_error("private access list is empty");
    }
}

std::int64_t polygonCross(const OverThrustPoint& first,
                          const OverThrustPoint& second,
                          const OverThrustPoint& third)
{
    return static_cast<std::int64_t>(second.x - first.x) *
               (third.y - first.y) -
           static_cast<std::int64_t>(second.y - first.y) *
               (third.x - first.x);
}

bool pointOnSegment(const OverThrustPoint& point,
                    const OverThrustPoint& first,
                    const OverThrustPoint& second)
{
    return polygonCross(first, second, point) == 0 &&
           point.x >= std::min(first.x, second.x) &&
           point.x <= std::max(first.x, second.x) &&
           point.y >= std::min(first.y, second.y) &&
           point.y <= std::max(first.y, second.y);
}

bool segmentsIntersect(const OverThrustPoint& a,
                       const OverThrustPoint& b,
                       const OverThrustPoint& c,
                       const OverThrustPoint& d)
{
    const std::int64_t abC = polygonCross(a, b, c);
    const std::int64_t abD = polygonCross(a, b, d);
    const std::int64_t cdA = polygonCross(c, d, a);
    const std::int64_t cdB = polygonCross(c, d, b);
    if (((abC > 0 && abD < 0) || (abC < 0 && abD > 0)) &&
        ((cdA > 0 && cdB < 0) || (cdA < 0 && cdB > 0))) {
        return true;
    }
    return (abC == 0 && pointOnSegment(c, a, b)) ||
           (abD == 0 && pointOnSegment(d, a, b)) ||
           (cdA == 0 && pointOnSegment(a, c, d)) ||
           (cdB == 0 && pointOnSegment(b, c, d));
}

void validateTeamBasePolygon(const OverThrustObject& object,
                             const std::size_t levelWidth,
                             const std::size_t levelHeight)
{
    const auto& polygon = object.areaPolygon;
    if (polygon.size() < 3 || polygon.size() > 256) {
        throw std::runtime_error(
            "team base polygon must contain 3 to 256 points");
    }
    std::int64_t doubledArea = 0;
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        const OverThrustPoint& point = polygon[index];
        const OverThrustPoint& next = polygon[(index + 1) % polygon.size()];
        if (point.x < 0 || point.y < 0 ||
            point.x > static_cast<int>(levelWidth) ||
            point.y > static_cast<int>(levelHeight) || point == next) {
            throw std::runtime_error("team base polygon point is invalid");
        }
        doubledArea += static_cast<std::int64_t>(point.x) * next.y -
                       static_cast<std::int64_t>(next.x) * point.y;
    }
    if (doubledArea == 0) {
        throw std::runtime_error("team base polygon has no area");
    }
    for (std::size_t first = 0; first < polygon.size(); ++first) {
        const std::size_t firstNext = (first + 1) % polygon.size();
        for (std::size_t second = first + 1; second < polygon.size();
             ++second) {
            const std::size_t secondNext = (second + 1) % polygon.size();
            if (first == second || firstNext == second ||
                secondNext == first) {
                continue;
            }
            if (segmentsIntersect(polygon[first], polygon[firstNext],
                                  polygon[second], polygon[secondNext])) {
                throw std::runtime_error(
                    "team base polygon intersects itself");
            }
        }
    }
}

void validate(const Level& level, const Level& background,
              const OverThrustProjectData& data)
{
    if (level.width == 0 || level.height == 0 || level.width > 4096 ||
        level.height > 4096 || level.width > kMaximumCells / level.height ||
        level.pixels.size() != level.pixelCount()) {
        throw std::runtime_error("material grid exceeds OverThrust limits");
    }
    if (background.width != level.width ||
        background.height != level.height ||
        background.pixels.size() != background.pixelCount()) {
        throw std::runtime_error(
            "background OPCX must match the material grid dimensions");
    }
    for (const std::uint8_t index : level.pixels) {
        if (overthrustMaterial(index) == nullptr) {
            throw std::runtime_error("material grid uses an unmapped index");
        }
    }
    if (data.displayName.empty() || data.displayName.size() > 256 ||
        data.levelId != data.displayName || data.author.empty() ||
        data.author.size() > 256 || data.description.size() > 4096) {
        throw std::runtime_error("level identity fields are invalid");
    }
    if (data.backgroundScrollX < 0 || data.backgroundScrollX > 4 ||
        data.backgroundScrollY < 0 || data.backgroundScrollY > 4) {
        throw std::runtime_error("OverThrust level settings are invalid");
    }
    const std::set<std::string> modes{
        "core:sandbox", "core:free_for_all", "core:team_deathmatch",
        "core:capture_the_flag"};
    std::set<std::string> selectedModes;
    for (const std::string& mode : data.allowedGameModes) {
        if (!modes.count(mode) || !selectedModes.insert(mode).second) {
            throw std::runtime_error("invalid or duplicate game mode");
        }
    }
    if (selectedModes.empty()) {
        throw std::runtime_error("at least one game mode is required");
    }
    if (selectedModes.count("core:capture_the_flag") &&
        (data.captureTheFlagPlayers < 2 ||
         data.captureTheFlagPlayers > 4)) {
        throw std::runtime_error(
            "capture-the-flag player count must be between 2 and 4");
    }
    if (!data.background.empty() &&
        !overthrustBackgroundDimensionsAllowed(
            data.background.width, data.background.height, level.width,
            level.height)) {
        throw std::runtime_error(
            "background dimensions are outside the allowed range for the world size");
    }

    std::set<std::string> channels;
    for (const OverThrustChannel& channel : data.channels) {
        if (channel.id.rfind("level:", 0) != 0 ||
            !validId(channel.id, 64) || !channels.insert(channel.id).second) {
            throw std::runtime_error("invalid or duplicate activation channel");
        }
    }
    std::set<std::string> objectIds;
    std::set<std::string> teleporterIds;
    std::set<std::string> teamBaseIds;
    std::set<int> teamBaseSlots;
    std::set<int> captureFlagSlots;
    for (const OverThrustObject& object : data.objects) {
        if (!validId(object.id) || !objectIds.insert(object.id).second) {
            throw std::runtime_error("invalid or duplicate object ID");
        }
        if (!object.unknownProperties.empty()) {
            throw std::runtime_error(
                "unknown object properties block this profile revision");
        }
        validateTransform(object.transform, level.width, level.height);
        if (object.type == "sliding_door") {
            const std::set<std::string> materials{
                "core:aluminium", "core:titanium", "core:tungsten"};
            const std::set<std::string> appearances{
                "appearance:door/aluminium", "appearance:door/titanium",
                "appearance:door/tungsten"};
            if ((object.openOffsetX == 0 && object.openOffsetY == 0) ||
                object.speedPixelsPerSecond <= 0 ||
                object.speedPixelsPerSecond > 4096 ||
                (object.initialState != "open" &&
                 object.initialState != "closed") ||
                !materials.count(object.materialId) ||
                !appearances.count(object.appearanceId)) {
                throw std::runtime_error("sliding door properties are invalid");
            }
            validateTransform(object.transform, level.width, level.height,
                              object.openOffsetX, object.openOffsetY);
            if (object.activationMode == "proximity") {
                if (object.activationRadiusPixels <= 0 ||
                    object.activationRadiusPixels > 4096) {
                    throw std::runtime_error("door proximity radius is invalid");
                }
            } else if (object.activationMode == "channel") {
                if (!channels.count(object.activationChannel)) {
                    throw std::runtime_error("door references an unknown channel");
                }
            } else {
                throw std::runtime_error("door activation mode is invalid");
            }
            validateAccess(object.access);
        } else if (object.type == "teleporter") {
            const std::set<std::string> appearances{
                "appearance:teleporter/blue",
                "appearance:teleporter/orange"};
            if (object.radiusPixels <= 0 ||
                object.radiusPixels > 4096 ||
                !appearances.count(object.appearanceId) ||
                (object.velocityMode != "preserve" &&
                 object.velocityMode != "zero" &&
                 object.velocityMode != "rotate_to_exit")) {
                throw std::runtime_error("teleporter properties are invalid");
            }
            validateAccess(object.access);
            teleporterIds.insert(object.id);
        } else if (object.type == "service_orb") {
            const std::set<std::string> appearances{
                "appearance:service-orb/default",
                "appearance:service-orb/private"};
            const std::vector<std::string> healOnly{"heal"};
            const std::vector<std::string> healWeapons{"heal",
                                                        "weapon_change"};
            const bool profileMatches =
                (object.serviceProfileId == "core:heal-only" &&
                 object.services == healOnly) ||
                (object.serviceProfileId == "core:heal-and-weapons" &&
                 object.services == healWeapons);
            if (object.radiusPixels <= 0 ||
                object.radiusPixels > 4096 || !profileMatches ||
                !appearances.count(object.appearanceId)) {
                throw std::runtime_error("service orb properties are invalid");
            }
            validateAccess(object.access);
        } else if (object.type == "team_base") {
            if (object.teamSlot < 1 ||
                object.teamSlot > 4 ||
                object.baseProfileId !=
                    "core:team-base/heal-and-weapons" ||
                object.appearanceId != "appearance:team-base/default" ||
                !teamBaseSlots.insert(object.teamSlot).second) {
                throw std::runtime_error("team base properties are invalid");
            }
            validateTeamBasePolygon(object, level.width, level.height);
            teamBaseIds.insert(object.id);
        } else if (object.type == "capture_flag") {
            if (object.teamSlot < 1 || object.teamSlot > 4 ||
                !captureFlagSlots.insert(object.teamSlot).second) {
                throw std::runtime_error("capture flag properties are invalid");
            }
        } else {
            throw std::runtime_error(
                "unknown object type blocks this profile revision");
        }
    }
    for (const OverThrustObject& object : data.objects) {
        if (object.type == "teleporter" &&
            (object.targetObjectId == object.id ||
             !teleporterIds.count(object.targetObjectId))) {
            throw std::runtime_error("teleporter target is missing or self");
        }
    }
    std::set<std::string> configuredTeamModes;
    for (const OverThrustTeamModeSetting& mode : data.teamModeSettings) {
        if (!selectedModes.count(mode.gameModeId) ||
            (mode.gameModeId != "core:team_deathmatch" &&
             mode.gameModeId != "core:capture_the_flag") ||
            mode.teams.size() < 2 || mode.teams.size() > 4 ||
            !configuredTeamModes.insert(mode.gameModeId).second) {
            throw std::runtime_error("team-mode settings are invalid");
        }
        std::set<int> teamSlotSet;
        for (const OverThrustTeamBinding& team : mode.teams) {
            if (team.teamSlot < 1 ||
                team.teamSlot > 4 ||
                !teamSlotSet.insert(team.teamSlot).second ||
                !teamBaseIds.count(team.baseObjectId)) {
                throw std::runtime_error("team binding is invalid");
            }
            if (mode.gameModeId == "core:capture_the_flag" &&
                !captureFlagSlots.count(team.teamSlot)) {
                throw std::runtime_error(
                    "capture-the-flag team is missing its flag object");
            }
        }
    }
    for (const std::string& mode : selectedModes) {
        if ((mode == "core:team_deathmatch" ||
             mode == "core:capture_the_flag") &&
            !configuredTeamModes.count(mode)) {
            throw std::runtime_error(
                "selected team mode is missing team-base bindings");
        }
    }
}

void writeFile(const std::filesystem::path& path, const std::string& contents)
{
    std::ofstream output(path, std::ios::binary);
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error("could not write package JSON");
    }
}

void writeFile(const std::filesystem::path& path,
               const std::vector<std::uint8_t>& contents)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(contents.data()),
                 static_cast<std::streamsize>(contents.size()));
    if (!output) {
        throw std::runtime_error("could not write package asset");
    }
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const std::streampos end = input.tellg();
    if (!input || end < 0) {
        throw std::runtime_error("could not read package asset");
    }
    std::vector<std::uint8_t> contents(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(contents.data()),
               static_cast<std::streamsize>(contents.size()));
    if (!input) {
        throw std::runtime_error("could not read package asset");
    }
    return contents;
}

std::uint32_t crc32(const std::vector<std::uint8_t>& contents)
{
    std::uint32_t crc = 0xffffffffU;
    for (const std::uint8_t byte : contents) {
        crc ^= byte;
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask = 0U - (crc & 1U);
            crc = (crc >> 1U) ^ (0xedb88320U & mask);
        }
    }
    return crc ^ 0xffffffffU;
}

void writeU32(std::ostream& output, const std::uint32_t value)
{
    const char bytes[]{static_cast<char>(value & 0xffU),
                       static_cast<char>((value >> 8U) & 0xffU),
                       static_cast<char>((value >> 16U) & 0xffU),
                       static_cast<char>((value >> 24U) & 0xffU)};
    output.write(bytes, sizeof(bytes));
}

void writeU64(std::ostream& output, const std::uint64_t value)
{
    writeU32(output, static_cast<std::uint32_t>(value));
    writeU32(output, static_cast<std::uint32_t>(value >> 32U));
}

struct OvlChunk {
    std::string id;
    std::vector<std::uint8_t> contents;
    std::uint32_t checksum = 0;
    std::uint64_t offset = 0;
};

void writeOvlContainer(const std::filesystem::path& path,
                       std::vector<OvlChunk>& chunks)
{
    constexpr std::uint64_t headerSize = 32;
    constexpr std::uint64_t tableEntrySize = 40;
    if (chunks.empty() || chunks.size() > 256) {
        throw std::runtime_error("OverThrust package has an invalid chunk count");
    }
    std::uint64_t nextOffset =
        headerSize + tableEntrySize * chunks.size();
    for (OvlChunk& chunk : chunks) {
        if (chunk.id.empty() || chunk.id.size() > 8) {
            throw std::runtime_error("OverThrust package chunk ID is invalid");
        }
        chunk.checksum = crc32(chunk.contents);
        chunk.offset = nextOffset;
        if (chunk.contents.size() >
            std::numeric_limits<std::uint64_t>::max() - nextOffset) {
            throw std::runtime_error("OverThrust package is too large");
        }
        nextOffset += chunk.contents.size();
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not create OverThrust package file");
    }
    const std::array<char, 8> magic{{'O', 'V', 'L', 'P', 'K', 'G', '1', 0}};
    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    writeU32(output, 1);
    writeU32(output, static_cast<std::uint32_t>(chunks.size()));
    writeU64(output, headerSize);
    writeU64(output, headerSize + tableEntrySize * chunks.size());

    for (const OvlChunk& chunk : chunks) {
        std::array<char, 8> id{};
        std::copy(chunk.id.begin(), chunk.id.end(), id.begin());
        output.write(id.data(), static_cast<std::streamsize>(id.size()));
        writeU64(output, chunk.offset);
        writeU64(output, chunk.contents.size());
        writeU64(output, chunk.contents.size());
        writeU32(output, chunk.checksum);
        writeU32(output, 0); // Reserved flags; payload is stored verbatim.
    }
    for (const OvlChunk& chunk : chunks)
        output.write(reinterpret_cast<const char*>(chunk.contents.data()),
                     static_cast<std::streamsize>(chunk.contents.size()));
    if (!output) {
        throw std::runtime_error("could not write OverThrust package file");
    }
}

std::string accessJson(const OverThrustAccess& access)
{
    if (access.everyone) {
        return "{ \"mode\": \"everyone\" }";
    }
    std::ostringstream output;
    output << "{ \"mode\": \"player_slots\", \"player_slots\": [";
    for (std::size_t index = 0; index < access.playerSlots.size(); ++index) {
        output << (index == 0 ? "" : ", ") << access.playerSlots[index];
    }
    output << "] }";
    return output.str();
}

std::string objectsJson(const OverThrustProjectData& data)
{
    std::ostringstream output;
    output << "{\n  \"schema_version\": 2,\n"
              "  \"coordinate_space\": \"world_pixels_top_left\",\n"
              "  \"channels\": [";
    for (std::size_t index = 0; index < data.channels.size(); ++index) {
        const OverThrustChannel& channel = data.channels[index];
        output << (index == 0 ? "" : ",") << "\n    { \"id\": "
               << jsonString(channel.id) << ", \"initial_state\": \""
               << (channel.initiallyActive ? "active" : "inactive") << "\" }";
    }
    if (!data.channels.empty()) output << '\n';
    output << "  ],\n  \"objects\": [";
    for (std::size_t index = 0; index < data.objects.size(); ++index) {
        const OverThrustObject& object = data.objects[index];
        output << (index == 0 ? "" : ",") << "\n    {\n"
               << "      \"id\": " << jsonString(object.id) << ",\n"
               << "      \"type\": " << jsonString(object.type) << ",\n"
               << "      \"transform\": {\n"
               << "        \"position_pixels\": { \"x\": "
               << object.transform.x << ", \"y\": " << object.transform.y
               << " },\n        \"size_pixels\": { \"width\": "
               << object.transform.width << ", \"height\": "
               << object.transform.height
               << " },\n        \"rotation_millidegrees\": "
               << object.transform.rotationMillidegrees << "\n      },\n";
        if (object.type == "sliding_door") {
            const double radians = object.transform.rotationMillidegrees *
                                   3.14159265358979323846 / 180000.0;
            const double localDirection = object.openOffsetX * std::cos(radians) +
                                          object.openOffsetY * std::sin(radians);
            output << "      \"slide_distance_pixels\": "
                   << decimal(std::hypot(object.openOffsetX,
                                         object.openOffsetY)) << ",\n"
                   << "      \"slide_direction\": "
                   << jsonString(localDirection < 0.0
                                     ? "negative_local_x"
                                     : "positive_local_x") << ",\n"
                   << "      \"speed_pixels_per_second\": "
                   << decimal(object.speedPixelsPerSecond) << ",\n"
                   << "      \"initial_state\": "
                   << jsonString(object.initialState) << ",\n"
                   << "      \"activation\": ";
            if (object.activationMode == "proximity") {
                output << "{ \"mode\": \"proximity\", \"radius_pixels\": "
                       << decimal(object.activationRadiusPixels) << " },\n";
            } else {
                output << "{ \"mode\": \"channel\", \"channel\": "
                       << jsonString(object.activationChannel) << " },\n";
            }
            output << "      \"access\": " << accessJson(object.access)
                   << ",\n      \"material_id\": "
                   << jsonString(object.materialId)
                   << ",\n      \"appearance_id\": "
                   << jsonString(object.appearanceId) << '\n';
        } else if (object.type == "teleporter") {
            output << "      \"radius_pixels\": "
                   << decimal(object.radiusPixels)
                   << ",\n      \"target_object_id\": "
                   << jsonString(object.targetObjectId)
                   << ",\n      \"velocity_mode\": "
                   << jsonString(object.velocityMode)
                   << ",\n      \"access\": " << accessJson(object.access)
                   << ",\n      \"appearance_id\": "
                   << jsonString(object.appearanceId) << '\n';
        } else if (object.type == "service_orb") {
            output << "      \"radius_pixels\": "
                   << decimal(object.radiusPixels)
                   << ",\n      \"service_profile_id\": "
                   << jsonString(object.serviceProfileId)
                   << ",\n      \"services\": [";
            for (std::size_t service = 0; service < object.services.size();
                 ++service) {
                output << (service == 0 ? "" : ", ")
                       << jsonString(object.services[service]);
            }
            output << "],\n      \"access\": " << accessJson(object.access)
                   << ",\n      \"appearance_id\": "
                   << jsonString(object.appearanceId) << '\n';
        } else if (object.type == "capture_flag") {
            output << "      \"team_slot\": " << object.teamSlot << '\n';
        } else {
            output << "      \"team_slot\": " << object.teamSlot
                   << ",\n      \"base_profile_id\": "
                   << jsonString(object.baseProfileId)
                   << ",\n      \"area_polygon_pixels\": [";
            for (std::size_t point = 0; point < object.areaPolygon.size();
                 ++point) {
                const OverThrustPoint& value = object.areaPolygon[point];
                output << (point == 0 ? "" : ", ") << "{ \"x\": "
                       << value.x << ", \"y\": " << value.y << " }";
            }
            output << "],\n      \"appearance_id\": "
                   << jsonString(object.appearanceId) << '\n';
        }
        output << "    }";
    }
    if (!data.objects.empty()) output << '\n';
    output << "  ]\n}\n";
    return output.str();
}

std::string manifestJson(const Level& level,
                         const OverThrustProjectData& data)
{
    std::ostringstream output;
    output << "{\n"
           << "  \"schema_version\": 3,\n"
           << "  \"profile_id\": \"overthrust:authoring-v3\",\n"
           << "  \"registry_id\": \"overthrust:authoring-registry-v3\",\n"
           << "  \"level_id\": " << jsonString(data.displayName) << ",\n"
           << "  \"display_name\": " << jsonString(data.displayName) << ",\n"
           << "  \"author\": " << jsonString(data.author) << ",\n"
           << "  \"description\": " << jsonString(data.description) << ",\n"
           << "  \"world_dimensions_pixels\": { \"width\": " << level.width
           << ", \"height\": " << level.height << " },\n"
           << "  \"coordinates\": { \"origin\": \"top_left\", \"positive_x\": \"right\", \"positive_y\": \"down\" },\n"
           << "  \"rasters\": {\n"
           << "    \"materials\": { \"chunk\": \"MATERIAL\", \"media_type\": \"image/vnd.zsoft.pcx\" },\n"
           << "    \"background\": {\n"
           << "      \"chunk\": \"BACKGRND\",\n"
           << "      \"media_type\": \"application/vnd.overthrust.layered-pcx\",\n"
           << "      \"format_id\": \"overthrust:layered-pcx-v1\",\n"
           << "      \"mapping\": \"world_bounds\",\n"
           << "      \"parallax\": false,\n"
           << "      \"pixel_dimensions\": { \"width\": " << level.width
           << ", \"height\": " << level.height << " }\n"
           << "    }";
    if (!data.background.empty()) {
        output << ",\n    \"presentation_background\": {\n"
               << "      \"chunk\": \"PRESENT\",\n"
               << "      \"media_type\": \"image/png\",\n"
               << "      \"mapping\": \"world_bounds\",\n"
               << "      \"parallax\": "
               << (data.backgroundParallax ? "true" : "false") << ",\n"
               << "      \"pixel_dimensions\": { \"width\": "
               << data.background.width << ", \"height\": "
               << data.background.height << " },\n"
               << "      \"color_space\": \"srgb\",\n"
               << "      \"alpha_mode\": \"straight\"\n"
               << "    }";
    }
    output << "\n  },\n"
           << "  \"objects\": \"OBJECTS\",\n"
           << "  \"settings\": {\n"
           << "    \"presentation_background_scroll_factor\": { \"x\": "
           << decimal(data.backgroundScrollX) << ", \"y\": "
           << decimal(data.backgroundScrollY) << " },\n";
    const bool captureTheFlag = std::find(
        data.allowedGameModes.begin(), data.allowedGameModes.end(),
        "core:capture_the_flag") != data.allowedGameModes.end();
    if (captureTheFlag) {
        output << "    \"capture_the_flag_player_count\": "
               << data.captureTheFlagPlayers << ",\n";
    }
    output << "    \"allowed_game_modes\": [";
    for (std::size_t index = 0; index < data.allowedGameModes.size(); ++index) {
        output << (index == 0 ? "" : ", ")
               << jsonString(data.allowedGameModes[index]);
    }
    output << "],\n    \"team_mode_settings\": [";
    for (std::size_t index = 0; index < data.teamModeSettings.size(); ++index) {
        const auto& mode = data.teamModeSettings[index];
        output << (index == 0 ? "" : ",")
               << "\n      {\n        \"game_mode_id\": "
               << jsonString(mode.gameModeId) << ",\n        \"teams\": [";
        for (std::size_t team = 0; team < mode.teams.size(); ++team) {
            output << (team == 0 ? "" : ", ")
                   << "{ \"team_slot\": " << mode.teams[team].teamSlot
                   << ", \"base_object_id\": "
                   << jsonString(mode.teams[team].baseObjectId);
            if (mode.gameModeId == "core:capture_the_flag") {
                const auto flag = std::find_if(
                    data.objects.begin(), data.objects.end(),
                    [&](const OverThrustObject& object) {
                        return object.type == "capture_flag" &&
                               object.teamSlot == mode.teams[team].teamSlot;
                    });
                if (flag != data.objects.end()) {
                    output << ", \"flag_object_id\": "
                           << jsonString(flag->id);
                }
            }
            output << " }";
        }
        output << "]\n      }";
    }
    if (!data.teamModeSettings.empty()) output << '\n';
    output << "    ]\n  },\n  \"index_mappings\": [\n";
    const auto& mappings = overthrustMaterials();
    for (std::size_t index = 0; index < mappings.size(); ++index) {
        const auto& material = mappings[index];
        const RGB color = level.palette[material.index];
        output << "    { \"index\": " << static_cast<int>(material.index)
               << ", \"label\": " << jsonString(material.label)
               << ", \"category\": " << jsonString(material.category)
               << ", \"material_id\": " << jsonString(material.materialId)
               << ", \"appearance_id\": "
               << jsonString(material.appearanceId)
               << ", \"preview_rgb\": [" << static_cast<int>(color.r)
               << ", " << static_cast<int>(color.g) << ", "
               << static_cast<int>(color.b) << "] }"
               << (index + 1 == mappings.size() ? "\n" : ",\n");
    }
    output << "  ]\n}\n";
    return output.str();
}

} // namespace

bool publishOverThrustPackage(const std::filesystem::path& file,
                              const Level& materials,
                              const Level& background,
                              const OverThrustProjectData& data,
                              std::string& error)
{
    error.clear();
    std::filesystem::path staging;
    std::filesystem::path temporaryArchive;
    try {
        validate(materials, background, data);
        std::vector<std::uint8_t> png;
        if (!data.background.empty() &&
            !encodeCanonicalRgbaPng(data.background.width,
                                    data.background.height,
                                    data.background.rgba, png, error)) {
            throw std::runtime_error(error);
        }
        if (std::filesystem::exists(file) &&
            !std::filesystem::is_regular_file(file)) {
            throw std::runtime_error(
                "publish destination exists and is not a regular file");
        }
        for (unsigned int attempt = 0; attempt < 1000; ++attempt) {
            staging = file;
            staging += ".tmp-dir-" + std::to_string(attempt);
            temporaryArchive = file;
            temporaryArchive += ".tmp-file-" + std::to_string(attempt);
            if (!std::filesystem::exists(temporaryArchive) &&
                std::filesystem::create_directory(staging)) {
                break;
            }
            staging.clear();
            temporaryArchive.clear();
        }
        if (staging.empty()) {
            throw std::runtime_error("could not create package staging directory");
        }
        writeFile(staging / "manifest.json", manifestJson(materials, data));
        std::string pcxError;
        if (!savePcx(staging / "materials.pcx", materials, pcxError)) {
            throw std::runtime_error(pcxError);
        }
        if (std::filesystem::file_size(staging / "materials.pcx") >
            kMaximumPcxBytes) {
            throw std::runtime_error("published material PCX is too large");
        }
        if (!saveOverThrustLayeredPcx(staging / "background.opcx",
                                      background, pcxError)) {
            throw std::runtime_error(pcxError);
        }
        if (std::filesystem::file_size(staging / "background.opcx") >
            kMaximumLayeredPcxBytes) {
            throw std::runtime_error(
                "published background OPCX is too large");
        }
        if (!data.background.empty()) {
            writeFile(staging / "background.png", png);
        }
        writeFile(staging / "objects.json", objectsJson(data));
        std::vector<OvlChunk> chunks;
        chunks.push_back({"MANIFEST", readFile(staging / "manifest.json")});
        chunks.push_back({"MATERIAL", readFile(staging / "materials.pcx")});
        chunks.push_back({"BACKGRND", readFile(staging / "background.opcx")});
        if (!data.background.empty()) {
            chunks.push_back({"PRESENT", readFile(staging / "background.png")});
        }
        chunks.push_back({"OBJECTS", readFile(staging / "objects.json")});
        writeOvlContainer(temporaryArchive, chunks);
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        staging.clear();
        if (std::filesystem::exists(file)) {
            std::filesystem::path backup;
            for (unsigned int attempt = 0; attempt < 1000; ++attempt) {
                backup = file;
                backup += ".replace-backup-" + std::to_string(attempt);
                if (!std::filesystem::exists(backup)) break;
                backup.clear();
            }
            if (backup.empty())
                throw std::runtime_error("could not reserve publish backup name");
            std::filesystem::rename(file, backup);
            try {
                std::filesystem::rename(temporaryArchive, file);
                temporaryArchive.clear();
                std::error_code cleanupError;
                std::filesystem::remove(backup, cleanupError);
            } catch (...) {
                std::error_code rollbackError;
                if (!std::filesystem::exists(file))
                    std::filesystem::rename(backup, file, rollbackError);
                throw;
            }
        } else {
            std::filesystem::rename(temporaryArchive, file);
            temporaryArchive.clear();
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        if (!staging.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(staging, ignored);
        }
        if (!temporaryArchive.empty()) {
            std::error_code ignored;
            std::filesystem::remove(temporaryArchive, ignored);
        }
        return false;
    }
}

namespace {

std::uint32_t readOvlU32(const std::vector<std::uint8_t>& bytes,
                         const std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::runtime_error("truncated OVL field");
    return static_cast<std::uint32_t>(bytes[offset]) |
           static_cast<std::uint32_t>(bytes[offset + 1]) << 8U |
           static_cast<std::uint32_t>(bytes[offset + 2]) << 16U |
           static_cast<std::uint32_t>(bytes[offset + 3]) << 24U;
}

std::uint64_t readOvlU64(const std::vector<std::uint8_t>& bytes,
                         const std::size_t offset)
{
    return static_cast<std::uint64_t>(readOvlU32(bytes, offset)) |
           static_cast<std::uint64_t>(readOvlU32(bytes, offset + 4)) << 32U;
}

std::map<std::string, std::vector<std::uint8_t>> readOvlContainer(
    const std::filesystem::path& file)
{
    constexpr std::uint64_t headerSize = 32;
    constexpr std::uint64_t entrySize = 40;
    const std::uintmax_t fileSize = std::filesystem::file_size(file);
    if (fileSize > 768ULL * 1024ULL * 1024ULL)
        throw std::runtime_error("OverThrust package is too large");
    const auto bytes = readFile(file);
    const std::array<std::uint8_t, 8> magic{
        {'O', 'V', 'L', 'P', 'K', 'G', '1', 0}};
    if (bytes.size() < headerSize ||
        !std::equal(magic.begin(), magic.end(), bytes.begin()) ||
        readOvlU32(bytes, 8) != 1) {
        throw std::runtime_error("not a supported native OverThrust OVL file");
    }
    const std::uint32_t count = readOvlU32(bytes, 12);
    const std::uint64_t tableOffset = readOvlU64(bytes, 16);
    const std::uint64_t dataOffset = readOvlU64(bytes, 24);
    if (count < 4 || count > 5 || tableOffset != headerSize ||
        dataOffset != headerSize + entrySize * count ||
        dataOffset > bytes.size()) {
        throw std::runtime_error("invalid OVL chunk table");
    }
    std::map<std::string, std::vector<std::uint8_t>> chunks;
    std::uint64_t expectedOffset = dataOffset;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t entry = static_cast<std::size_t>(
            tableOffset + entrySize * index);
        std::string id(reinterpret_cast<const char*>(bytes.data() + entry), 8);
        const auto zero = id.find('\0');
        if (zero != std::string::npos) id.resize(zero);
        const std::uint64_t offset = readOvlU64(bytes, entry + 8);
        const std::uint64_t storedSize = readOvlU64(bytes, entry + 16);
        const std::uint64_t unpackedSize = readOvlU64(bytes, entry + 24);
        const std::uint32_t checksum = readOvlU32(bytes, entry + 32);
        const std::uint32_t flags = readOvlU32(bytes, entry + 36);
        if (id.empty() || offset != expectedOffset ||
            storedSize != unpackedSize || flags != 0 ||
            offset > bytes.size() || storedSize > bytes.size() - offset) {
            throw std::runtime_error("invalid OVL chunk entry");
        }
        std::vector<std::uint8_t> contents(
            bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + storedSize));
        if (crc32(contents) != checksum ||
            !chunks.emplace(id, std::move(contents)).second) {
            throw std::runtime_error("invalid OVL chunk payload");
        }
        expectedOffset += storedSize;
    }
    if (expectedOffset != bytes.size())
        throw std::runtime_error("unexpected data after OVL chunks");
    const std::set<std::string> required{
        "MANIFEST", "MATERIAL", "BACKGRND", "OBJECTS"};
    for (const auto& id : required) {
        if (!chunks.count(id))
            throw std::runtime_error("OVL is missing required chunk " + id);
    }
    for (const auto& [id, ignored] : chunks) {
        (void)ignored;
        if (!required.count(id) && id != "PRESENT")
            throw std::runtime_error("OVL contains unsupported chunk " + id);
    }
    return chunks;
}

QJsonObject parseJsonObject(const std::vector<std::uint8_t>& bytes,
                            const char* label)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(
        QByteArray(reinterpret_cast<const char*>(bytes.data()),
                   static_cast<qsizetype>(bytes.size())), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        throw std::runtime_error(std::string("invalid ") + label + " JSON");
    return document.object();
}

QJsonObject requiredObject(const QJsonObject& parent, const char* key)
{
    const QJsonValue value = parent.value(QLatin1String(key));
    if (!value.isObject())
        throw std::runtime_error(std::string("missing OVL object field: ") + key);
    return value.toObject();
}

QJsonArray requiredArray(const QJsonObject& parent, const char* key)
{
    const QJsonValue value = parent.value(QLatin1String(key));
    if (!value.isArray())
        throw std::runtime_error(std::string("missing OVL array field: ") + key);
    return value.toArray();
}

std::string requiredString(const QJsonObject& parent, const char* key)
{
    const QJsonValue value = parent.value(QLatin1String(key));
    if (!value.isString())
        throw std::runtime_error(std::string("missing OVL string field: ") + key);
    return value.toString().toStdString();
}

int requiredInt(const QJsonObject& parent, const char* key)
{
    const QJsonValue value = parent.value(QLatin1String(key));
    if (!value.isDouble() || !std::isfinite(value.toDouble()) ||
        std::floor(value.toDouble()) != value.toDouble() ||
        value.toDouble() < std::numeric_limits<int>::min() ||
        value.toDouble() > std::numeric_limits<int>::max()) {
        throw std::runtime_error(std::string("invalid OVL integer field: ") + key);
    }
    return static_cast<int>(value.toDouble());
}

double requiredNumber(const QJsonObject& parent, const char* key)
{
    const QJsonValue value = parent.value(QLatin1String(key));
    if (!value.isDouble() || !std::isfinite(value.toDouble()))
        throw std::runtime_error(std::string("invalid OVL number field: ") + key);
    return value.toDouble();
}

OverThrustAccess parseAccess(const QJsonObject& value)
{
    OverThrustAccess access;
    const std::string mode = requiredString(value, "mode");
    if (mode == "everyone") return access;
    if (mode != "player_slots")
        throw std::runtime_error("invalid OVL access mode");
    access.everyone = false;
    for (const QJsonValue slot : requiredArray(value, "player_slots")) {
        if (!slot.isDouble() || std::floor(slot.toDouble()) != slot.toDouble())
            throw std::runtime_error("invalid OVL player slot");
        access.playerSlots.push_back(static_cast<int>(slot.toDouble()));
    }
    return access;
}

void parseManifest(const QJsonObject& manifest, OverThrustProjectData& data,
                   const bool hasPresentation)
{
    if (requiredInt(manifest, "schema_version") != 3 ||
        requiredString(manifest, "profile_id") != "overthrust:authoring-v3" ||
        requiredString(manifest, "registry_id") !=
            "overthrust:authoring-registry-v3") {
        throw std::runtime_error("unsupported OverThrust manifest schema");
    }
    data.levelId = requiredString(manifest, "level_id");
    data.displayName = requiredString(manifest, "display_name");
    data.author = requiredString(manifest, "author");
    data.description = requiredString(manifest, "description");
    const QJsonObject rasters = requiredObject(manifest, "rasters");
    if (requiredString(requiredObject(rasters, "materials"), "chunk") !=
            "MATERIAL" ||
        requiredString(requiredObject(rasters, "background"), "chunk") !=
            "BACKGRND" ||
        requiredString(manifest, "objects") != "OBJECTS") {
        throw std::runtime_error("OVL manifest references unsupported chunks");
    }
    const bool manifestPresentation =
        rasters.value(QStringLiteral("presentation_background")).isObject();
    if (manifestPresentation != hasPresentation)
        throw std::runtime_error("OVL presentation chunk does not match manifest");
    if (manifestPresentation) {
        const QJsonObject presentation =
            requiredObject(rasters, "presentation_background");
        if (requiredString(presentation, "chunk") != "PRESENT")
            throw std::runtime_error("invalid OVL presentation chunk reference");
        const QJsonValue parallax = presentation.value(QStringLiteral("parallax"));
        if (!parallax.isBool())
            throw std::runtime_error("invalid OVL presentation parallax setting");
        data.backgroundParallax = parallax.toBool();
    }
    const QJsonObject settings = requiredObject(manifest, "settings");
    const QJsonObject scroll = requiredObject(
        settings, "presentation_background_scroll_factor");
    data.backgroundScrollX = requiredNumber(scroll, "x");
    data.backgroundScrollY = requiredNumber(scroll, "y");
    if (settings.contains(QStringLiteral("capture_the_flag_player_count")))
        data.captureTheFlagPlayers =
            requiredInt(settings, "capture_the_flag_player_count");
    for (const QJsonValue mode : requiredArray(settings, "allowed_game_modes")) {
        if (!mode.isString()) throw std::runtime_error("invalid OVL game mode");
        data.allowedGameModes.push_back(mode.toString().toStdString());
    }
    data.teamModeSettings.clear();
    for (const QJsonValue modeValue :
         requiredArray(settings, "team_mode_settings")) {
        if (!modeValue.isObject())
            throw std::runtime_error("invalid OVL team-mode setting");
        const QJsonObject modeObject = modeValue.toObject();
        OverThrustTeamModeSetting mode;
        mode.gameModeId = requiredString(modeObject, "game_mode_id");
        for (const QJsonValue teamValue : requiredArray(modeObject, "teams")) {
            if (!teamValue.isObject())
                throw std::runtime_error("invalid OVL team binding");
            const QJsonObject team = teamValue.toObject();
            mode.teams.push_back({requiredInt(team, "team_slot"),
                                  requiredString(team, "base_object_id")});
        }
        data.teamModeSettings.push_back(std::move(mode));
    }
}

void parseObjects(const QJsonObject& root, OverThrustProjectData& data)
{
    if (requiredInt(root, "schema_version") != 2 ||
        requiredString(root, "coordinate_space") !=
            "world_pixels_top_left") {
        throw std::runtime_error("unsupported OverThrust objects schema");
    }
    for (const QJsonValue channelValue : requiredArray(root, "channels")) {
        if (!channelValue.isObject())
            throw std::runtime_error("invalid OVL activation channel");
        const QJsonObject channel = channelValue.toObject();
        const std::string state = requiredString(channel, "initial_state");
        if (state != "active" && state != "inactive")
            throw std::runtime_error("invalid OVL activation channel state");
        data.channels.push_back(
            {requiredString(channel, "id"), state == "active"});
    }
    for (const QJsonValue objectValue : requiredArray(root, "objects")) {
        if (!objectValue.isObject())
            throw std::runtime_error("invalid OVL level object");
        const QJsonObject value = objectValue.toObject();
        OverThrustObject object;
        object.id = requiredString(value, "id");
        object.type = requiredString(value, "type");
        const QJsonObject transform = requiredObject(value, "transform");
        const QJsonObject position = requiredObject(transform, "position_pixels");
        const QJsonObject size = requiredObject(transform, "size_pixels");
        object.transform = {requiredInt(position, "x"),
                            requiredInt(position, "y"),
                            requiredInt(size, "width"),
                            requiredInt(size, "height"),
                            requiredInt(transform, "rotation_millidegrees")};
        if (object.type == "sliding_door") {
            const double distance = requiredNumber(value, "slide_distance_pixels");
            const std::string direction = requiredString(value, "slide_direction");
            if (direction != "positive_local_x" &&
                direction != "negative_local_x")
                throw std::runtime_error("invalid OVL door slide direction");
            const double signedDistance =
                direction == "negative_local_x" ? -distance : distance;
            const double radians = object.transform.rotationMillidegrees *
                                   3.14159265358979323846 / 180000.0;
            object.openOffsetX = static_cast<int>(
                std::round(signedDistance * std::cos(radians)));
            object.openOffsetY = static_cast<int>(
                std::round(signedDistance * std::sin(radians)));
            object.speedPixelsPerSecond =
                requiredNumber(value, "speed_pixels_per_second");
            object.initialState = requiredString(value, "initial_state");
            const QJsonObject activation = requiredObject(value, "activation");
            object.activationMode = requiredString(activation, "mode");
            if (object.activationMode == "proximity")
                object.activationRadiusPixels = requiredNumber(activation, "radius_pixels");
            else if (object.activationMode == "channel")
                object.activationChannel = requiredString(activation, "channel");
            else
                throw std::runtime_error("invalid OVL door activation mode");
            object.access = parseAccess(requiredObject(value, "access"));
            object.materialId = requiredString(value, "material_id");
            object.appearanceId = requiredString(value, "appearance_id");
        } else if (object.type == "teleporter") {
            object.radiusPixels = requiredNumber(value, "radius_pixels");
            object.targetObjectId = requiredString(value, "target_object_id");
            object.velocityMode = requiredString(value, "velocity_mode");
            object.access = parseAccess(requiredObject(value, "access"));
            object.appearanceId = requiredString(value, "appearance_id");
        } else if (object.type == "service_orb") {
            object.radiusPixels = requiredNumber(value, "radius_pixels");
            object.serviceProfileId = requiredString(value, "service_profile_id");
            for (const QJsonValue service : requiredArray(value, "services")) {
                if (!service.isString())
                    throw std::runtime_error("invalid OVL service name");
                object.services.push_back(service.toString().toStdString());
            }
            object.access = parseAccess(requiredObject(value, "access"));
            object.appearanceId = requiredString(value, "appearance_id");
        } else if (object.type == "capture_flag") {
            object.teamSlot = requiredInt(value, "team_slot");
        } else if (object.type == "team_base") {
            object.teamSlot = requiredInt(value, "team_slot");
            object.baseProfileId = requiredString(value, "base_profile_id");
            object.appearanceId = requiredString(value, "appearance_id");
            for (const QJsonValue pointValue :
                 requiredArray(value, "area_polygon_pixels")) {
                if (!pointValue.isObject())
                    throw std::runtime_error("invalid OVL polygon point");
                const QJsonObject point = pointValue.toObject();
                object.areaPolygon.push_back(
                    {requiredInt(point, "x"), requiredInt(point, "y")});
            }
        } else {
            throw std::runtime_error("unsupported OVL object type");
        }
        data.objects.push_back(std::move(object));
    }
}

} // namespace

bool loadOverThrustPackage(const std::filesystem::path& file,
                           Level& materials, Level& background,
                           OverThrustProjectData& data, std::string& error)
{
    error.clear();
    try {
        const auto chunks = readOvlContainer(file);
        Level loadedMaterials;
        Level loadedBackground;
        OverThrustProjectData loadedData;
        loadedData.allowedGameModes.clear();
        std::string pcxError;
        if (!loadPcx(chunks.at("MATERIAL"), file.stem().string(),
                     loadedMaterials, pcxError))
            throw std::runtime_error("invalid OVL material image: " + pcxError);
        if (!loadOverThrustLayeredPcx(chunks.at("BACKGRND"),
                                      file.stem().string() + " background",
                                      loadedBackground, pcxError))
            throw std::runtime_error("invalid OVL layered background: " + pcxError);
        parseManifest(parseJsonObject(chunks.at("MANIFEST"), "manifest"),
                      loadedData, chunks.count("PRESENT") != 0);
        parseObjects(parseJsonObject(chunks.at("OBJECTS"), "objects"),
                     loadedData);
        if (const auto present = chunks.find("PRESENT"); present != chunks.end()) {
            if (!decodeCanonicalRgbaPng(present->second,
                                        loadedData.background.width,
                                        loadedData.background.height,
                                        loadedData.background.rgba, error))
                throw std::runtime_error("invalid OVL presentation background: " + error);
            loadedData.background.sourceProvenance = file.filename().string();
        }
        validate(loadedMaterials, loadedBackground, loadedData);
        materials = std::move(loadedMaterials);
        background = std::move(loadedBackground);
        data = std::move(loadedData);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
