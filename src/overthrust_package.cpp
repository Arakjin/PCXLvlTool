#include "overthrust_package.h"

#include "canonical_png.h"
#include "overthrust_profile.h"
#include "pcx_writer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <exception>
#include <fstream>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

constexpr std::uint64_t kMaximumCells = 4194304ULL;
constexpr std::uintmax_t kMaximumPcxBytes = 16777216ULL;

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
    std::set<int> slots;
    for (const int slot : access.playerSlots) {
        if (slot < 1 || slot > 4 ||
            !slots.insert(slot).second) {
            throw std::runtime_error("invalid private player-slot access");
        }
    }
    if (slots.empty()) {
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

bool pointInPolygon(const OverThrustPoint& point,
                    const std::vector<OverThrustPoint>& polygon)
{
    bool inside = false;
    for (std::size_t current = 0, previous = polygon.size() - 1;
         current < polygon.size(); previous = current++) {
        const OverThrustPoint& a = polygon[previous];
        const OverThrustPoint& b = polygon[current];
        if (pointOnSegment(point, a, b)) return true;
        if ((a.y > point.y) != (b.y > point.y)) {
            const double crossingX = a.x +
                static_cast<double>(point.y - a.y) * (b.x - a.x) /
                    (b.y - a.y);
            if (crossingX > point.x) inside = !inside;
        }
    }
    return inside;
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
    const OverThrustPoint anchor{
        object.transform.x + object.objectiveAnchorX,
        object.transform.y + object.objectiveAnchorY};
    if (!pointInPolygon(anchor, polygon)) {
        throw std::runtime_error(
            "team base objective anchor is outside its polygon");
    }
}

void validate(const Level& level, const OverThrustProjectData& data)
{
    if (level.width == 0 || level.height == 0 || level.width > 4096 ||
        level.height > 4096 || level.width > kMaximumCells / level.height ||
        level.pixels.size() != level.pixelCount()) {
        throw std::runtime_error("material grid exceeds OverThrust limits");
    }
    for (const std::uint8_t index : level.pixels) {
        if (overthrustMaterial(index) == nullptr) {
            throw std::runtime_error("material grid uses an unmapped index");
        }
    }
    if (!validId(data.levelId) || data.displayName.empty() ||
        data.displayName.size() > 256 || data.author.empty() ||
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
    if (!data.background.empty() &&
        static_cast<std::uint64_t>(data.background.width) * level.height !=
        static_cast<std::uint64_t>(data.background.height) * level.width) {
        throw std::runtime_error(
            "background and material grid aspect ratios differ");
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
            const double slideDistance = std::hypot(object.openOffsetX,
                                                    object.openOffsetY);
            const double slideRadians =
                object.transform.rotationMillidegrees *
                3.14159265358979323846 / 180000.0;
            validateTransform(
                object.transform, level.width, level.height,
                static_cast<int>(std::round(
                    slideDistance * std::cos(slideRadians))),
                static_cast<int>(std::round(
                    slideDistance * std::sin(slideRadians))));
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
                object.objectiveAnchorX < 0 || object.objectiveAnchorY < 0 ||
                object.objectiveAnchorX > object.transform.width ||
                object.objectiveAnchorY > object.transform.height ||
                !teamBaseSlots.insert(object.teamSlot).second) {
                throw std::runtime_error("team base properties are invalid");
            }
            validateTeamBasePolygon(object, level.width, level.height);
            teamBaseIds.insert(object.id);
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
        std::set<int> slots;
        for (const OverThrustTeamBinding& team : mode.teams) {
            if (team.teamSlot < 1 ||
                team.teamSlot > 4 ||
                !slots.insert(team.teamSlot).second ||
                !teamBaseIds.count(team.baseObjectId)) {
                throw std::runtime_error("team binding is invalid");
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
    output << "{\n  \"schema_version\": 3,\n"
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
            output << "      \"slide_distance_pixels\": "
                   << decimal(std::hypot(object.openOffsetX,
                                         object.openOffsetY)) << ",\n"
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
            output << "],\n      \"objective_anchor_offset_pixels\": { \"x\": "
                   << object.objectiveAnchorX << ", \"y\": "
                   << object.objectiveAnchorY
                   << " },\n      \"appearance_id\": "
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
           << "  \"registry_id\": \"overthrust:authoring-registry-v2\",\n"
           << "  \"level_id\": " << jsonString(data.levelId) << ",\n"
           << "  \"display_name\": " << jsonString(data.displayName) << ",\n"
           << "  \"author\": " << jsonString(data.author) << ",\n"
           << "  \"description\": " << jsonString(data.description) << ",\n"
           << "  \"world_dimensions_pixels\": { \"width\": " << level.width
           << ", \"height\": " << level.height << " },\n"
           << "  \"coordinates\": { \"origin\": \"top_left\", \"positive_x\": \"right\", \"positive_y\": \"down\" },\n"
           << "  \"rasters\": {\n"
           << "    \"materials\": \"materials.pcx\"";
    if (!data.background.empty()) {
        output << ",\n    \"background\": {\n"
               << "      \"file\": \"background.png\",\n"
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
           << "  \"objects\": \"objects.json\",\n"
           << "  \"settings\": {\n"
           << "    \"background_scroll_factor\": { \"x\": "
           << decimal(data.backgroundScrollX) << ", \"y\": "
           << decimal(data.backgroundScrollY) << " },\n"
           << "    \"allowed_game_modes\": [";
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
                   << jsonString(mode.teams[team].baseObjectId) << " }";
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

bool publishOverThrustPackage(const std::filesystem::path& directory,
                              const Level& materials,
                              const OverThrustProjectData& data,
                              std::string& error)
{
    error.clear();
    std::filesystem::path staging;
    try {
        validate(materials, data);
        std::vector<std::uint8_t> png;
        if (!data.background.empty() &&
            !encodeCanonicalRgbaPng(data.background.width,
                                    data.background.height,
                                    data.background.rgba, png, error)) {
            throw std::runtime_error(error);
        }
        if (std::filesystem::exists(directory)) {
            throw std::runtime_error("publish directory already exists");
        }
        for (unsigned int attempt = 0; attempt < 1000; ++attempt) {
            staging = directory;
            staging += ".tmp-" + std::to_string(attempt);
            if (std::filesystem::create_directory(staging)) break;
            staging.clear();
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
        if (!data.background.empty()) {
            writeFile(staging / "background.png", png);
        }
        writeFile(staging / "objects.json", objectsJson(data));
        std::filesystem::rename(staging, directory);
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        if (!staging.empty()) {
            std::error_code ignored;
            std::filesystem::remove_all(staging, ignored);
        }
        return false;
    }
}
