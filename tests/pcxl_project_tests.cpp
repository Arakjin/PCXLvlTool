#include "default_palette.h"
#include "layer_model.h"
#include "overthrust_profile.h"
#include "pcxl_project_io.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

bool expect(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    bool ok = true;
    const std::filesystem::path path =
        std::filesystem::current_path() / "project-test.pxlp";
    std::error_code ignored;

    Level level;
    level.resize(400, 600);
    level.name = "WINGS TEST";
    level.palette = defaultWingsPalette();
    initializeBackgroundLayer(level);
    level.layers[0].pixels[1234] = 128;
    level.layers.emplace_back();
    level.layers.back().name = "Upper";
    level.layers.back().pixels.resize(level.pixelCount());
    level.layers.back().mask.resize(level.pixelCount());
    level.layers.back().pixels[5678] = 80;
    level.layers.back().mask[5678] = 1;
    level.activeLayer = 1;
    flattenLayers(level);

    const auto parallaxSize = wingsParallaxSize(400, 600);
    Level background;
    background.resize(parallaxSize.first, parallaxSize.second);
    background.name = "Background";
    background.palette = level.palette;
    initializeBackgroundLayer(background);
    background.layers[0].pixels[321] = 200;
    flattenLayers(background);

    LevelCreationSettings settings;
    settings.game = GameId::Wings;
    settings.name = level.name;
    settings.width = 400;
    settings.height = 600;
    settings.backgroundMode = BackgroundMode::Parallax;
    settings.stars = true;
    settings.rainProbability = 17;
    settings.snowProbability = 4;
    settings.bombingProbability = 3;
    settings.civilians = 42;
    settings.armedCiviliansProbability = 51;

    std::string error;
    ok &= expect(savePxlProject(path, level, &background, settings, error),
                 "PXLP project should save");
    Level loaded;
    std::unique_ptr<Level> loadedBackground;
    LevelCreationSettings loadedSettings;
    const bool loadedOk = loadPxlProject(path, loaded, loadedBackground,
                                         loadedSettings, error);
    if (!loadedOk) {
        std::cerr << "PXLP load error: " << error << '\n';
    }
    ok &= expect(loadedOk, "PXLP project should load");
    ok &= expect(loadedSettings.game == GameId::Wings &&
                     loadedSettings.name == "WINGS TEST" &&
                     loadedSettings.width == 400 &&
                     loadedSettings.height == 600 &&
                     loadedSettings.backgroundMode ==
                         BackgroundMode::Parallax &&
                     loadedSettings.stars &&
                     loadedSettings.rainProbability == 17 &&
                     loadedSettings.civilians == 42 &&
                     loadedSettings.armedCiviliansProbability == 51,
                 "PXLP settings should round-trip");
    ok &= expect(loaded.layers.size() == 2 && loaded.activeLayer == 1 &&
                     loaded.layers[0].pixels[1234] == 128 &&
                     loaded.layers[1].pixels[5678] == 80 &&
                     loaded.layers[1].mask[5678] == 1 &&
                     loaded.pixels[5678] == 80,
                 "PXLP main layers should round-trip");
    ok &= expect(loadedBackground &&
                     loadedBackground->width ==
                         static_cast<std::size_t>(parallaxSize.first) &&
                     loadedBackground->height ==
                         static_cast<std::size_t>(parallaxSize.second) &&
                     loadedBackground->layers[0].pixels[321] == 200 &&
                     loadedBackground->palette == loaded.palette,
                 "PXLP parallax document should round-trip");

    Level vwing;
    vwing.name = "V-WING TEST";
    vwing.palette = defaultVWingPalette();
    initializeBackgroundLayer(vwing);
    vwing.layers[0].pixels[999] = 57;
    flattenLayers(vwing);
    LevelCreationSettings vwingSettings;
    vwingSettings.game = GameId::VWing;
    vwingSettings.name = vwing.name;
    ok &= expect(savePxlProject(path, vwing, nullptr, vwingSettings, error),
                 "V-Wing PXLP project should save");
    loadedBackground.reset();
    ok &= expect(loadPxlProject(path, loaded, loadedBackground, loadedSettings,
                                error) &&
                     loadedSettings.game == GameId::VWing &&
                     loadedSettings.name == "V-WING TEST" &&
                     loaded.layers[0].pixels[999] == 57 &&
                     !loadedBackground,
                 "V-Wing PXLP project should round-trip");

    Level auts;
    auts.resize(320, 400);
    auts.name = "AUTSTEST";
    auts.palette = defaultAutsPalette();
    initializeBackgroundLayer(auts);
    auts.layers[0].pixels[12345] = 39;
    flattenLayers(auts);
    LevelCreationSettings autsSettings;
    autsSettings.game = GameId::Auts;
    autsSettings.name = auts.name;
    autsSettings.width = 320;
    autsSettings.height = 400;
    ok &= expect(savePxlProject(path, auts, nullptr, autsSettings, error),
                 "AUTS PXLP project should save");
    loadedBackground.reset();
    ok &= expect(loadPxlProject(path, loaded, loadedBackground, loadedSettings,
                                error) &&
                     loadedSettings.game == GameId::Auts &&
                     loadedSettings.name == "AUTSTEST" &&
                     loaded.width == 320 && loaded.height == 400 &&
                     loaded.layers[0].pixels[12345] == 39 &&
                     !loadedBackground,
                 "AUTS PXLP project should round-trip");
    auts.palette[0] = RGB{1, 2, 3};
    ok &= expect(!savePxlProject(path, auts, nullptr, autsSettings, error),
                 "AUTS PXLP project should reject a non-game palette");

    Level overthrust;
    overthrust.resize(400, 400);
    overthrust.name = "OverThrust test";
    overthrust.palette = defaultOverThrustPalette();
    initializeBackgroundLayer(overthrust);
    overthrust.layers[0].pixels[42] = 69;
    flattenLayers(overthrust);
    LevelCreationSettings overthrustSettings;
    overthrustSettings.game = GameId::OverThrust;
    overthrustSettings.name = overthrust.name;
    overthrustSettings.width = 400;
    overthrustSettings.height = 400;
    overthrustSettings.backgroundMode = BackgroundMode::ExternalTrueColor;
    OverThrustProjectData overthrustData;
    overthrustData.levelId = "test:round-trip";
    overthrustData.displayName = "Round trip";
    overthrustData.author = "PCX Level Tool";
    overthrustData.description = "PXLP v3 data test";
    overthrustData.backgroundParallax = true;
    overthrustData.background.width = 2;
    overthrustData.background.height = 2;
    overthrustData.background.rgba = {
        1, 2, 3, 255, 4, 5, 6, 255,
        7, 8, 9, 255, 10, 11, 12, 255};
    OverThrustObject door;
    door.id = "door_1";
    door.type = "sliding_door";
    door.transform = {10, 20, 3, 8, 45000};
    door.openOffsetY = -10;
    door.materialId = "core:aluminium";
    door.appearanceId = "appearance:door/aluminium";
    door.unknownProperties.push_back({"future", "string", "retained"});
    overthrustData.objects.push_back(door);
    OverThrustObject teamBase;
    teamBase.id = "team_base_1";
    teamBase.type = "team_base";
    teamBase.transform = {30, 40, 12, 9, 0};
    teamBase.appearanceId = "appearance:team-base/default";
    teamBase.baseProfileId = "core:team-base/heal-and-weapons";
    teamBase.objectiveAnchorX = 5;
    teamBase.objectiveAnchorY = 4;
    teamBase.areaPolygon = {{30, 40}, {42, 41}, {39, 49}, {31, 47}};
    overthrustData.objects.push_back(teamBase);
    ok &= expect(savePxlProject(path, overthrust, nullptr,
                               overthrustSettings, &overthrustData, error),
                 "OverThrust PXLP project should save");
    std::unique_ptr<OverThrustProjectData> loadedOverThrust;
    loadedBackground.reset();
    ok &= expect(loadPxlProject(path, loaded, loadedBackground, loadedSettings,
                               loadedOverThrust, error),
                 "OverThrust PXLP project should load");
    ok &= expect(loadedSettings.game == GameId::OverThrust &&
                     loaded.width == 400 && loaded.height == 400 &&
                     loaded.layers[0].pixels[42] == 69 &&
                     loadedOverThrust &&
                     loadedOverThrust->levelId == "test:round-trip" &&
                     loadedOverThrust->backgroundParallax &&
                     loadedOverThrust->background.rgba ==
                         overthrustData.background.rgba &&
                     loadedOverThrust->objects.size() == 2 &&
                     loadedOverThrust->objects[0].transform.rotationMillidegrees ==
                         45000 &&
                     loadedOverThrust->objects[0].unknownProperties.size() == 1 &&
                     loadedOverThrust->objects[1].areaPolygon ==
                         teamBase.areaPolygon,
                 "OverThrust PXLP v3 data should round-trip");

    // Corrupt the first object's serialized X coordinate without disturbing
    // the surrounding framing. The bounded v2 reader must reject it before
    // exposing a partially loaded project.
    std::ifstream projectInput(path, std::ios::binary);
    std::vector<char> projectBytes(
        (std::istreambuf_iterator<char>(projectInput)),
        std::istreambuf_iterator<char>());
    const std::string objectType = "sliding_door";
    const auto typePosition = std::search(projectBytes.begin(),
                                          projectBytes.end(),
                                          objectType.begin(), objectType.end());
    const auto malformedPath =
        std::filesystem::current_path() / "project-malformed.pxlp";
    if (typePosition != projectBytes.end() &&
        std::distance(typePosition, projectBytes.end()) >=
            static_cast<std::ptrdiff_t>(objectType.size() + 4)) {
        auto xPosition = typePosition +
                         static_cast<std::ptrdiff_t>(objectType.size());
        std::fill_n(xPosition, 4, static_cast<char>(0xff)); // x = -1
        std::ofstream malformedOutput(malformedPath, std::ios::binary);
        malformedOutput.write(projectBytes.data(),
                              static_cast<std::streamsize>(projectBytes.size()));
        malformedOutput.close();
        loadedOverThrust.reset();
        ok &= expect(!loadPxlProject(malformedPath, loaded, loadedBackground,
                                    loadedSettings, loadedOverThrust, error),
                     "malformed OverThrust object bounds should be rejected");
    } else {
        ok &= expect(false, "serialized OverThrust object should be locatable");
    }

    std::filesystem::remove(path, ignored);
    std::filesystem::remove(malformedPath, ignored);
    return ok ? 0 : 1;
}
