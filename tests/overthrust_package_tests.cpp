#include "canonical_png.h"
#include "layer_model.h"
#include "overthrust_package.h"
#include "overthrust_profile.h"
#include "pcx_reader.h"
#include "pcx_writer.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

bool expect(const bool condition, const char* message)
{
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    const auto size = input.tellg();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
    return bytes;
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes,
                      const std::size_t offset)
{
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("truncated OVL field");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::uint64_t readU64(const std::vector<std::uint8_t>& bytes,
                      const std::size_t offset)
{
    return static_cast<std::uint64_t>(readU32(bytes, offset)) |
           (static_cast<std::uint64_t>(readU32(bytes, offset + 4)) << 32U);
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

std::map<std::string, std::vector<std::uint8_t>> readOvlChunks(
    const std::filesystem::path& path)
{
    const std::vector<std::uint8_t> bytes = readBytes(path);
    const std::array<std::uint8_t, 8> magic{
        {'O', 'V', 'L', 'P', 'K', 'G', '1', 0}};
    if (bytes.size() < 32 ||
        !std::equal(magic.begin(), magic.end(), bytes.begin()) ||
        readU32(bytes, 8) != 1) {
        throw std::runtime_error("invalid OVL header");
    }
    const std::uint32_t count = readU32(bytes, 12);
    const std::uint64_t tableOffset = readU64(bytes, 16);
    const std::uint64_t dataOffset = readU64(bytes, 24);
    if (count == 0 || count > 256 || tableOffset != 32 ||
        dataOffset != 32ULL + 40ULL * count || dataOffset > bytes.size()) {
        throw std::runtime_error("invalid OVL chunk table");
    }
    std::map<std::string, std::vector<std::uint8_t>> chunks;
    std::uint64_t expectedOffset = dataOffset;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::size_t entry =
            static_cast<std::size_t>(tableOffset + 40ULL * index);
        std::string id(reinterpret_cast<const char*>(bytes.data() + entry), 8);
        const std::size_t terminator = id.find('\0');
        if (terminator != std::string::npos) id.resize(terminator);
        const std::uint64_t offset = readU64(bytes, entry + 8);
        const std::uint64_t size = readU64(bytes, entry + 16);
        const std::uint64_t unpackedSize = readU64(bytes, entry + 24);
        const std::uint32_t checksum = readU32(bytes, entry + 32);
        const std::uint32_t flags = readU32(bytes, entry + 36);
        if (id.empty() || offset != expectedOffset || size != unpackedSize ||
            flags != 0 || offset > bytes.size() || size > bytes.size() - offset) {
            throw std::runtime_error("invalid OVL chunk entry");
        }
        std::vector<std::uint8_t> contents(
            bytes.begin() + static_cast<std::ptrdiff_t>(offset),
            bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
        if (crc32(contents) != checksum ||
            !chunks.emplace(id, std::move(contents)).second) {
            throw std::runtime_error("invalid OVL chunk payload");
        }
        expectedOffset += size;
    }
    if (expectedOffset != bytes.size()) {
        throw std::runtime_error("unexpected data after OVL chunks");
    }
    return chunks;
}

void writeBytes(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes)
{
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(bytes.data()),
                 static_cast<std::streamsize>(bytes.size()));
}

void writeU32(std::ostream& output, const std::uint32_t value)
{
    for (int shift = 0; shift < 32; shift += 8)
        output.put(static_cast<char>(value >> shift));
}

void writeU64(std::ostream& output, const std::uint64_t value)
{
    writeU32(output, static_cast<std::uint32_t>(value));
    writeU32(output, static_cast<std::uint32_t>(value >> 32U));
}

void writeOvlChunks(
    const std::filesystem::path& path,
    const std::map<std::string, std::vector<std::uint8_t>>& chunks)
{
    const std::vector<std::string> order{
        "MANIFEST", "MATERIAL", "BACKGRND", "PRESENT", "OBJECTS"};
    std::vector<std::string> present;
    for (const std::string& id : order) {
        if (chunks.count(id)) present.push_back(id);
    }
    const std::uint64_t dataOffset = 32 + 40 * present.size();
    std::uint64_t offset = dataOffset;
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    const std::array<char, 8> magic{{'O', 'V', 'L', 'P', 'K', 'G', '1', 0}};
    output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
    writeU32(output, 1);
    writeU32(output, static_cast<std::uint32_t>(present.size()));
    writeU64(output, 32);
    writeU64(output, dataOffset);
    for (const std::string& idValue : present) {
        std::array<char, 8> id{};
        std::copy(idValue.begin(), idValue.end(), id.begin());
        const auto& contents = chunks.at(idValue);
        output.write(id.data(), static_cast<std::streamsize>(id.size()));
        writeU64(output, offset);
        writeU64(output, contents.size());
        writeU64(output, contents.size());
        writeU32(output, crc32(contents));
        writeU32(output, 0);
        offset += contents.size();
    }
    for (const std::string& id : present) {
        const auto& contents = chunks.at(id);
        output.write(reinterpret_cast<const char*>(contents.data()),
                     static_cast<std::streamsize>(contents.size()));
    }
    if (!output) throw std::runtime_error("could not write test OVL");
}

void replaceOnce(std::string& value, const std::string& oldText,
                 const std::string& newText)
{
    const std::size_t offset = value.find(oldText);
    if (offset == std::string::npos)
        throw std::runtime_error("test JSON replacement target is missing");
    value.replace(offset, oldText.size(), newText);
}

} // namespace

int main()
{
    bool ok = true;
    const auto first = std::filesystem::current_path() / "package-a.OVL";
    const auto second = std::filesystem::current_path() / "package-b.OVL";
    std::error_code ignored;
    std::filesystem::remove_all(first, ignored);
    std::filesystem::remove_all(second, ignored);

    Level level;
    level.resize(4, 4);
    level.palette = defaultOverThrustPalette();
    level.pixels = Level::PixelBuffer(16);
    for (std::size_t index = 0; index < level.pixels.size(); ++index) {
        level.pixels[index] = static_cast<std::uint8_t>(index % 4);
    }
    Level background;
    background.resize(4, 4);
    background.palette = defaultOverThrustPalette();
    background.palette[1] = {12, 34, 56};
    background.pixels = Level::PixelBuffer(16);
    background.pixels[5] = 1;
    initializeBackgroundLayer(background);
    background.layers.emplace_back();
    background.layers.back().name = "Middle scenery";
    background.layers.back().pixels.resize(background.pixelCount());
    background.layers.back().mask.resize(background.pixelCount());
    background.layers.back().pixels[10] = 2;
    background.layers.back().mask[10] = 1;
    background.activeLayer = 1;
    flattenLayers(background);

    OverThrustProjectData data;
    data.displayName = "Package test";
    data.levelId = data.displayName;
    data.author = "PCX Level Tool";
    data.description = "Deterministic package test";
    data.backgroundParallax = true;
    data.allowedGameModes = {"core:sandbox"};
    data.background.width = 8;
    data.background.height = 3;
    data.background.rgba.resize(8 * 3 * 4);
    for (std::size_t index = 0; index < data.background.rgba.size(); ++index) {
        data.background.rgba[index] = static_cast<std::uint8_t>(index * 7);
    }
    OverThrustObject durationDoor;
    durationDoor.id = "duration_door";
    durationDoor.type = "sliding_door";
    durationDoor.transform = {1, 1, 1, 1, 0};
    durationDoor.openOffsetX = -1;
    durationDoor.travelDurationSeconds = 0.75;
    durationDoor.activationMode = "channel";
    durationDoor.activationChannel = "level:duration_door";
    durationDoor.access.everyone = false;
    durationDoor.access.playerSlots = {1, 3};
    durationDoor.materialId = "core:aluminium";
    durationDoor.appearanceId = "appearance:door/aluminium";
    data.channels = {{"level:duration_door", true}};
    data.objects = {durationDoor};

    std::string error;
    ok &= expect(publishOverThrustPackage(first, level, background, data, error),
                 "first package should publish");
    ok &= expect(publishOverThrustPackage(second, level, background, data, error),
                 "second package should publish");
    const std::vector<std::string> names{
        "MANIFEST", "MATERIAL", "BACKGRND", "PRESENT", "OBJECTS"};
    const auto firstEntries = readOvlChunks(first);
    const auto secondEntries = readOvlChunks(second);
    ok &= expect(std::filesystem::is_regular_file(first) &&
                     firstEntries.size() == 5,
                 "package should be one file containing exactly five chunks");
    for (const std::string& name : names) {
        ok &= expect(firstEntries.count(name) == 1 &&
                         firstEntries.at(name) == secondEntries.at(name),
                     "publishing twice should produce identical bytes");
    }
    ok &= expect(readBytes(first) == readBytes(second),
                 "publishing twice should produce identical archives");
    ok &= expect(publishOverThrustPackage(first, level, background, data,
                                           error) &&
                     readOvlChunks(first) == firstEntries,
                 "publishing should atomically replace an existing OVL file");
    const auto& manifestBytes = firstEntries.at("MANIFEST");
    const std::string manifestText(manifestBytes.begin(), manifestBytes.end());
    ok &= expect(manifestText.find("\"schema_version\": 3") !=
                         std::string::npos &&
                     manifestText.find("\"profile_id\": \"overthrust:authoring-v3\"") !=
                         std::string::npos &&
                     manifestText.find(
                         "\"registry_id\": \"overthrust:authoring-registry-v3\"") !=
                         std::string::npos &&
                     manifestText.find("\"level_id\": \"Package test\"") !=
                         std::string::npos &&
                     manifestText.find("capture_the_flag_player_count") ==
                         std::string::npos &&
                     manifestText.find("\"parallax\": true") !=
                         std::string::npos &&
                     manifestText.find("\"presentation_background\"") !=
                         std::string::npos &&
                     manifestText.find("\"chunk\": \"BACKGRND\"") !=
                         std::string::npos &&
                     manifestText.find(
                         "\"format_id\": \"overthrust:layered-pcx-v1\"") !=
                         std::string::npos &&
                     manifestText.find("\"parallax\": false") !=
                         std::string::npos &&
                     manifestText.find(
                         "\"presentation_background_scroll_factor\"") !=
                         std::string::npos &&
                     manifestText.find("\"background_scroll_factor\"") ==
                         std::string::npos &&
                     manifestText.find("minimum_players") == std::string::npos &&
                     manifestText.find("maximum_players") == std::string::npos &&
                     manifestText.find("random_player_spawns") == std::string::npos &&
                     manifestText.find("boundary_policy") == std::string::npos &&
                     manifestText.find("\"world_dimensions_pixels\"") !=
                         std::string::npos &&
                     manifestText.find("cells_per_world_unit") ==
                         std::string::npos &&
                     manifestText.find("\"index\": 255") !=
                         std::string::npos &&
                     manifestText.find("\"material_id\": \"core:acidic_gas\"") !=
                         std::string::npos,
                 "manifest should publish simplified v3 settings and the complete registry");
    const auto& initialObjectBytes = firstEntries.at("OBJECTS");
    const std::string initialObjects(initialObjectBytes.begin(),
                                     initialObjectBytes.end());
    ok &= expect(initialObjects.find("\"schema_version\": 4") !=
                         std::string::npos &&
                     initialObjects.find(
                         "\"travel_duration_seconds\": 0.75") !=
                         std::string::npos &&
                     initialObjects.find(
                         "\"slide_direction\": \"negative_local_x\"") !=
                         std::string::npos &&
                     initialObjects.find("speed_pixels_per_second") ==
                         std::string::npos,
                 "new OBJECTS chunks should use v4 door duration fields");
    Level loaded;
    const auto extractedMaterials =
        std::filesystem::current_path() / "extracted-materials.pcx";
    const auto extractedBackground =
        std::filesystem::current_path() / "extracted-background.opcx";
    writeBytes(extractedMaterials, firstEntries.at("MATERIAL"));
    writeBytes(extractedBackground, firstEntries.at("BACKGRND"));
    ok &= expect(loadPcx(extractedMaterials, loaded, error) &&
                     loaded.width == 4 && loaded.height == 4 &&
                     loaded.pixels == level.pixels,
                 "published PCX should preserve exact indices");
    ok &= expect(loadOverThrustLayeredPcx(extractedBackground, loaded, error) &&
                     loaded.width == 4 && loaded.height == 4 &&
                     loaded.pixels == background.pixels &&
                     loaded.palette == background.palette,
                 "background OPCX should preserve its independent palette and indices");
    ok &= expect(loaded.layers.size() == 2 && loaded.activeLayer == 1 &&
                     loaded.layers[1].name == "Middle scenery" &&
                     loaded.layers[1].pixels ==
                         background.layers[1].pixels &&
                     loaded.layers[1].mask == background.layers[1].mask,
                 "background OPCX should preserve separate layer records");
    Level openedMaterials;
    Level openedBackground;
    OverThrustProjectData openedData;
    ok &= expect(loadOverThrustPackage(first, openedMaterials,
                                        openedBackground, openedData, error),
                 "published OVL should open through the production reader");
    ok &= expect(openedMaterials.pixels == level.pixels &&
                     openedBackground.layers.size() == 2 &&
                     openedBackground.layers[1].name == "Middle scenery" &&
                     openedData.displayName == data.displayName &&
                     openedData.author == data.author &&
                     openedData.description == data.description &&
                     openedData.background.rgba == data.background.rgba &&
                     openedData.backgroundParallax == data.backgroundParallax &&
                     openedData.allowedGameModes == data.allowedGameModes &&
                     openedData.objects.size() == 1 &&
                     openedData.objects[0].travelDurationSeconds == 0.75,
                 "opening OVL should restore editable raster and manifest data");
    const auto v4RoundTrip =
        std::filesystem::current_path() / "v4-round-trip.OVL";
    std::filesystem::remove(v4RoundTrip, ignored);
    ok &= expect(publishOverThrustPackage(
                     v4RoundTrip, openedMaterials, openedBackground,
                     openedData, error) &&
                     readOvlChunks(v4RoundTrip).at("OBJECTS") ==
                         firstEntries.at("OBJECTS"),
                 "v4 OBJECTS JSON should survive open and republish unchanged");

    for (const int legacyVersion : {2, 3}) {
        auto legacyChunks = firstEntries;
        std::string legacyObjects(legacyChunks.at("OBJECTS").begin(),
                                  legacyChunks.at("OBJECTS").end());
        replaceOnce(legacyObjects, "\"schema_version\": 4",
                    "\"schema_version\": " +
                        std::to_string(legacyVersion));
        if (legacyVersion == 2) {
            replaceOnce(
                legacyObjects,
                "      \"slide_direction\": \"negative_local_x\",\n",
                "");
        }
        replaceOnce(legacyObjects,
                    "\"travel_duration_seconds\": 0.75",
                    "\"speed_pixels_per_second\": 2");
        legacyChunks["OBJECTS"] = std::vector<std::uint8_t>(
            legacyObjects.begin(), legacyObjects.end());
        const auto legacyPath = std::filesystem::current_path() /
            ("legacy-v" + std::to_string(legacyVersion) + ".OVL");
        const auto migratedPath = std::filesystem::current_path() /
            ("migrated-v" + std::to_string(legacyVersion) + ".OVL");
        std::filesystem::remove(legacyPath, ignored);
        std::filesystem::remove(migratedPath, ignored);
        writeOvlChunks(legacyPath, legacyChunks);
        OverThrustProjectData migratedData;
        ok &= expect(loadOverThrustPackage(legacyPath, openedMaterials,
                                            openedBackground, migratedData,
                                            error) &&
                         migratedData.objects.size() == 1 &&
                         migratedData.objects[0].travelDurationSeconds == 0.5,
                     "legacy v2/v3 speed should migrate to door duration");
        ok &= expect(publishOverThrustPackage(
                         migratedPath, openedMaterials, openedBackground,
                         migratedData, error),
                     "migrated v2/v3 OVL should republish as v4");
        const auto migratedChunks = readOvlChunks(migratedPath);
        const std::string migratedObjects(
            migratedChunks.at("OBJECTS").begin(),
            migratedChunks.at("OBJECTS").end());
        ok &= expect(migratedObjects.find("\"schema_version\": 4") !=
                             std::string::npos &&
                         migratedObjects.find(
                             "\"travel_duration_seconds\": 0.5") !=
                             std::string::npos &&
                         migratedObjects.find(
                             legacyVersion == 2
                                 ? "\"slide_direction\": \"positive_local_x\""
                                 : "\"slide_direction\": \"negative_local_x\"") !=
                             std::string::npos &&
                         migratedObjects.find("speed_pixels_per_second") ==
                             std::string::npos,
                     "migrated objects should emit strict v4 JSON");
        std::filesystem::remove(legacyPath, ignored);
        std::filesystem::remove(migratedPath, ignored);
    }
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
    ok &= expect(decodeCanonicalRgbaPng(firstEntries.at("PRESENT"),
                                        width, height, rgba, error) &&
                     width == 8 && height == 3 && rgba == data.background.rgba,
                 "different-aspect background PNG should round-trip exact RGBA bytes");

    OverThrustProjectData noBackground = data;
    noBackground.background = {};
    const auto noBackgroundPackage =
        std::filesystem::current_path() / "no-background.OVL";
    std::filesystem::remove_all(noBackgroundPackage, ignored);
    ok &= expect(publishOverThrustPackage(noBackgroundPackage, level,
                                          background, noBackground, error),
                 "package without a presentation PNG should publish");
    const auto noBackgroundEntries = readOvlChunks(noBackgroundPackage);
    ok &= expect(noBackgroundEntries.count("PRESENT") == 0 &&
                     noBackgroundEntries.count("BACKGRND") == 1,
                 "presentation PNG should be optional while the OPCX background is always published");

    OverThrustObject teamBase;
    teamBase.id = "team_base_1";
    teamBase.type = "team_base";
    teamBase.transform = {0, 0, 4, 4, 0};
    teamBase.appearanceId = "appearance:team-base/default";
    teamBase.teamSlot = 1;
    teamBase.baseProfileId = "core:team-base/heal-and-weapons";
    teamBase.areaPolygon = {{0, 0}, {4, 0}, {3, 4}, {0, 3}};
    OverThrustObject captureFlag;
    captureFlag.id = "capture_flag_1";
    captureFlag.type = "capture_flag";
    captureFlag.transform = {1, 1, 1, 1, 0};
    captureFlag.teamSlot = 1;
    data.objects = {teamBase, captureFlag};
    const auto polygonPackage =
        std::filesystem::current_path() / "polygon.OVL";
    std::filesystem::remove_all(polygonPackage, ignored);
    ok &= expect(publishOverThrustPackage(polygonPackage, level, background,
                                          data, error),
                 "polygonal team base should publish");
    const auto objectsBytes = readOvlChunks(polygonPackage).at("OBJECTS");
    const std::string objectsText(objectsBytes.begin(), objectsBytes.end());
    ok &= expect(objectsText.find("\"schema_version\": 4") !=
                         std::string::npos &&
                     objectsText.find("\"area_polygon_pixels\"") !=
                         std::string::npos &&
                     objectsText.find(
                         "\"area_polygon_pixels\": [{ \"x\": 0, \"y\": 0 }, "
                         "{ \"x\": 4, \"y\": 0 }, { \"x\": 3, \"y\": 4 }, "
                         "{ \"x\": 0, \"y\": 3 }]") != std::string::npos &&
                     objectsText.find("\"position_pixels\"") !=
                         std::string::npos &&
                     objectsText.find("\"type\": \"capture_flag\"") !=
                         std::string::npos &&
                     objectsText.find("objective_anchor_offset_pixels") ==
                         std::string::npos &&
                     objectsText.find("position_cells") == std::string::npos,
                 "published objects should use the pixel-coordinate schema");
    OverThrustObject secondBase = teamBase;
    secondBase.id = "team_base_2";
    secondBase.teamSlot = 2;
    OverThrustObject secondFlag = captureFlag;
    secondFlag.id = "capture_flag_2";
    secondFlag.teamSlot = 2;
    secondFlag.transform = {2, 2, 1, 1, 0};
    data.objects = {teamBase, secondBase, captureFlag, secondFlag};
    data.allowedGameModes = {"core:capture_the_flag"};
    data.captureTheFlagPlayers = 3;
    data.teamModeSettings = {
        {"core:capture_the_flag", {{1, teamBase.id}, {2, secondBase.id}}}};
    const auto capturePackage =
        std::filesystem::current_path() / "capture-the-flag.OVL";
    std::filesystem::remove_all(capturePackage, ignored);
    ok &= expect(publishOverThrustPackage(capturePackage, level, background,
                                          data, error),
                 "capture-the-flag package should publish");
    const auto captureManifestBytes =
        readOvlChunks(capturePackage).at("MANIFEST");
    const std::string captureManifest(captureManifestBytes.begin(),
                                      captureManifestBytes.end());
    ok &= expect(captureManifest.find(
                     "\"capture_the_flag_player_count\": 3") !=
                     std::string::npos &&
                     captureManifest.find(
                         "\"flag_object_id\": \"capture_flag_1\"") !=
                         std::string::npos &&
                     captureManifest.find(
                         "\"flag_object_id\": \"capture_flag_2\"") !=
                         std::string::npos,
                 "capture-the-flag manifest should bind each team's separate flag");
    ok &= expect(loadOverThrustPackage(capturePackage, openedMaterials,
                                        openedBackground, openedData, error) &&
                     openedData.objects.size() == 4 &&
                     openedData.objects[0].type == "team_base" &&
                     openedData.objects[0].areaPolygon ==
                         teamBase.areaPolygon &&
                     openedData.objects[2].type == "capture_flag" &&
                     openedData.teamModeSettings.size() == 1 &&
                     openedData.captureTheFlagPlayers == 3,
                 "opening OVL should restore objects and team-mode settings");
    const auto captureRoundTrip =
        std::filesystem::current_path() / "capture-round-trip.OVL";
    std::filesystem::remove(captureRoundTrip, ignored);
    ok &= expect(publishOverThrustPackage(
                     captureRoundTrip, openedMaterials, openedBackground,
                     openedData, error) &&
                     readOvlChunks(captureRoundTrip).at("OBJECTS") ==
                         readOvlChunks(capturePackage).at("OBJECTS"),
                 "v4 transforms and polygons should round-trip unchanged");
    data.allowedGameModes = {"core:sandbox"};
    data.teamModeSettings.clear();
    data.objects.clear();

    const auto invalid = std::filesystem::current_path() / "invalid.OVL";
    level.pixels[0] = 255;
    const auto flyThroughPackage =
        std::filesystem::current_path() / "fly-through.OVL";
    std::filesystem::remove_all(flyThroughPackage, ignored);
    std::filesystem::remove_all(noBackgroundPackage, ignored);
    ok &= expect(publishOverThrustPackage(flyThroughPackage, level,
                                          background, data, error),
                 "fly-through color 255 should publish");
    level.pixels[0] = 0;
    const auto expectInvalid = [&](const char* label) {
        std::filesystem::remove_all(invalid, ignored);
        const bool rejected =
            !publishOverThrustPackage(invalid, level, background, data,
                                      error) &&
            !std::filesystem::exists(invalid);
        ok &= expect(rejected, label);
    };
    OverThrustObject teleporter;
    teleporter.id = "teleporter_a";
    teleporter.type = "teleporter";
    teleporter.transform = {0, 0, 2, 2, 0};
    teleporter.radiusPixels = 4;
    teleporter.targetObjectId = "missing";
    teleporter.appearanceId = "appearance:teleporter/blue";
    data.objects = {teleporter};
    expectInvalid("missing teleporter target should be rejected");
    teleporter.targetObjectId = teleporter.id;
    data.objects = {teleporter};
    expectInvalid("self teleporter target should be rejected");
    OverThrustObject duplicate = teleporter;
    duplicate.targetObjectId = "teleporter_b";
    data.objects = {duplicate, duplicate};
    expectInvalid("duplicate object IDs should be rejected");
    OverThrustObject door;
    door.id = "door";
    door.type = "sliding_door";
    door.transform = {3, 3, 2, 2, 45000};
    door.openOffsetX = 1;
    door.travelDurationSeconds = 0.75;
    door.activationRadiusPixels = 4;
    door.materialId = "core:aluminium";
    door.appearanceId = "appearance:door/aluminium";
    data.objects = {door};
    expectInvalid("rotated footprint outside the level should be rejected");
    door.transform = {1, 1, 1, 1, 0};
    door.access.everyone = false;
    door.access.playerSlots = {5};
    data.objects = {door};
    expectInvalid("inaccessible player slot should be rejected");
    door.access = {};
    door.travelDurationSeconds = 3600.001;
    data.objects = {door};
    expectInvalid("door duration above the v4 schema maximum should be rejected");
    data.objects.clear();
    data.allowedGameModes = {"core:capture_the_flag"};
    data.teamModeSettings = {
        {"core:capture_the_flag", {{1, teamBase.id}, {2, secondBase.id}}}};
    data.objects = {teamBase, secondBase};
    expectInvalid("capture-the-flag teams without flag objects should be rejected");
    data.allowedGameModes = {"core:sandbox"};
    data.teamModeSettings.clear();
    teamBase.areaPolygon.clear();
    data.objects = {teamBase};
    expectInvalid("team base without a polygon should be rejected");
    teamBase.areaPolygon = {{0, 0}, {4, 4}, {0, 4}, {4, 0}};
    data.objects = {teamBase};
    expectInvalid("self-intersecting team-base polygon should be rejected");
    data.objects.clear();

#ifdef PCXLVLTOOL_SOURCE_DIR
    const auto nativeFixture =
        std::filesystem::path(PCXLVLTOOL_SOURCE_DIR) / "Everything.OVL";
    if (std::filesystem::is_regular_file(nativeFixture)) {
        const auto fixtureChunks = readOvlChunks(nativeFixture);
        ok &= expect(fixtureChunks.size() == 5 &&
                         fixtureChunks.count("MANIFEST") == 1 &&
                         fixtureChunks.count("MATERIAL") == 1 &&
                         fixtureChunks.count("BACKGRND") == 1 &&
                         fixtureChunks.count("PRESENT") == 1 &&
                         fixtureChunks.count("OBJECTS") == 1,
                     "Everything.OVL should use the native five-chunk format");
        const std::string fixtureManifest(
            fixtureChunks.at("MANIFEST").begin(),
            fixtureChunks.at("MANIFEST").end());
        ok &= expect(fixtureManifest.find("\"chunk\": \"BACKGRND\"") !=
                             std::string::npos &&
                         fixtureManifest.find("background.opcx") ==
                             std::string::npos,
                     "Everything.OVL manifest should address native chunks");
        const auto fixtureOpcx =
            std::filesystem::current_path() / "everything-background.opcx";
        writeBytes(fixtureOpcx, fixtureChunks.at("BACKGRND"));
        Level fixtureBackground;
        const bool fixtureBackgroundOpened = loadOverThrustLayeredPcx(
            fixtureOpcx, fixtureBackground, error);
        if (!fixtureBackgroundOpened)
            std::cerr << "Everything OPCX open error: " << error << '\n';
        ok &= expect(fixtureBackgroundOpened &&
                         fixtureBackground.width == 1000 &&
                         fixtureBackground.height == 1000 &&
                         !fixtureBackground.layers.empty(),
                     "Everything.OVL should contain a valid layered OPCX background");
        Level fixtureMaterials;
        OverThrustProjectData fixtureData;
        const bool fixtureOpened = loadOverThrustPackage(
            nativeFixture, fixtureMaterials, fixtureBackground, fixtureData,
            error);
        if (!fixtureOpened)
            std::cerr << "Everything.OVL open error: " << error << '\n';
        ok &= expect(fixtureOpened &&
                         fixtureMaterials.width == 1000 &&
                         fixtureMaterials.height == 1000 &&
                         fixtureData.displayName == "Everything",
                     "Everything.OVL should open as an editable OverThrust level");
        std::filesystem::remove(fixtureOpcx, ignored);
    }

    const auto fixture = std::filesystem::path(PCXLVLTOOL_SOURCE_DIR) /
                         "overthrust-authoring-v1";
    if (std::filesystem::exists(fixture / "background.png")) {
        const auto fixturePng = readBytes(fixture / "background.png");
        ok &= expect(decodeCanonicalRgbaPng(fixturePng, width, height, rgba,
                                            error),
                     "authoring fixture PNG should be canonical");
        std::vector<std::uint8_t> reencoded;
        ok &= expect(encodeCanonicalRgbaPng(width, height, rgba, reencoded,
                                            error) &&
                         reencoded == fixturePng,
                     "fixture canonical PNG should reproduce byte-for-byte");
        Level fixtureLevel;
        ok &= expect(loadPcx(fixture / "materials.pcx", fixtureLevel, error),
                     "authoring fixture PCX should load");
        const auto fixtureRoundTrip =
            std::filesystem::current_path() / "fixture-round-trip.pcx";
        ok &= expect(savePcx(fixtureRoundTrip, fixtureLevel, error) &&
                         readBytes(fixtureRoundTrip) ==
                             readBytes(fixture / "materials.pcx"),
                     "fixture PCX should reproduce byte-for-byte");
        std::filesystem::remove(fixtureRoundTrip, ignored);
    }
#endif

    std::filesystem::remove_all(first, ignored);
    std::filesystem::remove_all(second, ignored);
    std::filesystem::remove_all(polygonPackage, ignored);
    std::filesystem::remove_all(capturePackage, ignored);
    std::filesystem::remove_all(flyThroughPackage, ignored);
    std::filesystem::remove_all(v4RoundTrip, ignored);
    std::filesystem::remove_all(captureRoundTrip, ignored);
    std::filesystem::remove(extractedMaterials, ignored);
    std::filesystem::remove(extractedBackground, ignored);
    return ok ? 0 : 1;
}
