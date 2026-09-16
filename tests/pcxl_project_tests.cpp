#include "default_palette.h"
#include "layer_model.h"
#include "pcxl_project_io.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

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

    std::filesystem::remove(path, ignored);
    return ok ? 0 : 1;
}
