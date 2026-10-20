#include "canonical_png.h"
#include "layer_model.h"
#include "overthrust_package.h"
#include "overthrust_profile.h"
#include "pcx_reader.h"
#include "pcx_writer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
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

} // namespace

int main()
{
    bool ok = true;
    const auto first = std::filesystem::current_path() / "package-a.OLV";
    const auto second = std::filesystem::current_path() / "package-b.OLV";
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

    std::string error;
    ok &= expect(publishOverThrustPackage(first, level, background, data, error),
                 "first package should publish");
    ok &= expect(publishOverThrustPackage(second, level, background, data, error),
                 "second package should publish");
    const std::vector<std::string> names{
        "manifest.json", "materials.pcx", "background.pcx", "background.png",
        "objects.json"};
    std::size_t fileCount = 0;
    for (const auto& entry : std::filesystem::directory_iterator(first)) {
        if (entry.is_regular_file()) ++fileCount;
    }
    ok &= expect(fileCount == 5, "package should contain exactly five files");
    for (const std::string& name : names) {
        ok &= expect(readBytes(first / name) == readBytes(second / name),
                     "publishing twice should produce identical bytes");
    }
    const auto manifestBytes = readBytes(first / "manifest.json");
    const std::string manifestText(manifestBytes.begin(), manifestBytes.end());
    ok &= expect(manifestText.find("\"profile_id\": \"overthrust:authoring-v5\"") !=
                         std::string::npos &&
                     manifestText.find("\"level_id\": \"Package test\"") !=
                         std::string::npos &&
                     manifestText.find("capture_the_flag_player_count") ==
                         std::string::npos &&
                     manifestText.find("\"parallax\": true") !=
                         std::string::npos &&
                     manifestText.find("\"presentation_background\"") !=
                         std::string::npos &&
                     manifestText.find("\"file\": \"background.pcx\"") !=
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
                 "manifest should publish simplified v4 settings and the complete registry");
    Level loaded;
    ok &= expect(loadPcx(first / "materials.pcx", loaded, error) &&
                     loaded.width == 4 && loaded.height == 4 &&
                     loaded.pixels == level.pixels,
                 "published PCX should preserve exact indices");
    ok &= expect(loadPcx(first / "background.pcx", loaded, error) &&
                     loaded.width == 4 && loaded.height == 4 &&
                     loaded.pixels == background.pixels &&
                     loaded.palette == background.palette,
                 "background PCX should preserve its independent palette and indices");
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<std::uint8_t> rgba;
    ok &= expect(decodeCanonicalRgbaPng(readBytes(first / "background.png"),
                                        width, height, rgba, error) &&
                     width == 8 && height == 3 && rgba == data.background.rgba,
                 "different-aspect background PNG should round-trip exact RGBA bytes");

    OverThrustProjectData noBackground = data;
    noBackground.background = {};
    const auto noBackgroundPackage =
        std::filesystem::current_path() / "no-background.OLV";
    std::filesystem::remove_all(noBackgroundPackage, ignored);
    ok &= expect(publishOverThrustPackage(noBackgroundPackage, level,
                                          background, noBackground, error) &&
                     !std::filesystem::exists(noBackgroundPackage /
                                              "background.png") &&
                     std::filesystem::exists(noBackgroundPackage /
                                             "background.pcx"),
                 "presentation PNG should be optional while the PCX background is always published");

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
        std::filesystem::current_path() / "polygon.OLV";
    std::filesystem::remove_all(polygonPackage, ignored);
    ok &= expect(publishOverThrustPackage(polygonPackage, level, background,
                                          data, error),
                 "polygonal team base should publish");
    const auto objectsBytes = readBytes(polygonPackage / "objects.json");
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
        std::filesystem::current_path() / "capture-the-flag.OLV";
    std::filesystem::remove_all(capturePackage, ignored);
    ok &= expect(publishOverThrustPackage(capturePackage, level, background,
                                          data, error),
                 "capture-the-flag package should publish");
    const auto captureManifestBytes =
        readBytes(capturePackage / "manifest.json");
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
    data.allowedGameModes = {"core:sandbox"};
    data.teamModeSettings.clear();
    data.objects.clear();

    const auto invalid = std::filesystem::current_path() / "invalid.OLV";
    level.pixels[0] = 255;
    const auto flyThroughPackage =
        std::filesystem::current_path() / "fly-through.OLV";
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
    door.speedPixelsPerSecond = 4;
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
    return ok ? 0 : 1;
}
