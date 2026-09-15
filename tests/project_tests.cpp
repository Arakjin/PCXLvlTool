#include "layer_model.h"
#include "project_io.h"

#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>

namespace {

class TestFile {
public:
    explicit TestFile(std::filesystem::path path) : path_(std::move(path)) {}
    ~TestFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

bool expect(const bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    const TestFile file(std::filesystem::current_path() / "project-test.vwp");
    auto source = std::make_unique<Level>();
    source->name = "LAYER TEST";
    source->pixels.fill(1);
    source->palette[57] = {12, 34, 56};
    initializeBackgroundLayer(*source);
    source->layers[0].pixels[123] = 57;
    Level::Layer upper;
    upper.name = "Foreground";
    upper.visible = false;
    upper.locked = true;
    upper.pixels[456] = 99;
    upper.mask[456] = 1;
    source->layers.push_back(std::move(upper));
    source->activeLayer = 1;
    flattenLayers(*source);

    std::string error;
    bool ok = expect(saveProject(file.path(), *source, error),
                     "project could not be saved: " + error);
    auto loaded = std::make_unique<Level>();
    ok &= expect(loadProject(file.path(), *loaded, error),
                 "saved project could not be loaded: " + error);
    ok &= expect(loaded->name == source->name &&
                     loaded->palette == source->palette,
                 "project metadata and palette did not round-trip");
    ok &= expect(loaded->layers.size() == 2 && loaded->activeLayer == 1 &&
                     loaded->layers[1].name == "Foreground" &&
                     !loaded->layers[1].visible && loaded->layers[1].locked,
                 "layer ordering and flags did not round-trip");
    ok &= expect(loaded->layers[0].pixels[123] == 57 &&
                     loaded->layers[1].pixels[456] == 99 &&
                     loaded->layers[1].mask[456] == 1 &&
                     loaded->pixels[456] == 1,
                 "indexed layer pixels, transparency, or compositing changed");
    ok &= expect(loaded->layers[0].name == "Background" &&
                     loaded->layers[0].visible &&
                     loaded->layers[0].mask[456] == 1,
                 "loader did not enforce Background invariants");

    if (!ok) {
        return 1;
    }
    std::cout << "All project tests passed\n";
    return 0;
}
