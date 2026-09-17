#include "default_palette.h"
#include "palette_io.h"
#include "palette_groups.h"
#include "palette_rules.h"

#include <array>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <utility>

namespace {

class TestFile
{
  public:
    explicit TestFile(std::filesystem::path path) : path_(std::move(path)) {}

    ~TestFile()
    {
        std::error_code ignored;
        std::filesystem::remove(path_, ignored);
    }

    const std::filesystem::path &path() const
    {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

bool expect(const bool condition, const std::string &message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        return false;
    }
    return true;
}

} // namespace

int main()
{
    bool ok = true;
    const auto palette = defaultVWingPalette();
    ok &= expect(palette[0] == RGB{0, 0, 0},
                 "default background should be black");
    ok &= expect(!(palette[15] == palette[16]) &&
                     !(palette[56] == palette[148]),
               "default material ranges should contain useful color choices");
    ok &= expect(isReservedPaletteIndex(1) && isReservedPaletteIndex(55) &&
                     isReservedPaletteIndex(15) &&
                     !isReservedPaletteIndex(16) &&
                     !isReservedPaletteIndex(56),
                 "reserved Color Chart index rules are incorrect");
    ok &= expect(colorChartNumber(0) == 0 &&
                     colorChartNumber(255) == 255 &&
                     paletteIndexFromColorChart(57) == 57,
                 "material index conversion is incorrect");
    const auto vwingAll =
        paletteIndices(PaletteGroup::AllUsable, GameId::VWing);
    const auto vwingSpecial =
        paletteIndices(PaletteGroup::Special, GameId::VWing);
    ok &= expect(vwingAll.size() == 232 &&
                     std::none_of(vwingAll.begin(), vwingAll.end(),
                                  [](const std::uint8_t index) {
                                      return isReservedPaletteIndex(
                                          GameId::VWing, index);
                                  }) &&
                     std::find(vwingAll.begin(), vwingAll.end(), 46) !=
                         vwingAll.end() &&
                     std::find(vwingSpecial.begin(), vwingSpecial.end(), 46) !=
                         vwingSpecial.end(),
                 "V-Wing palette groups must contain every usable index");
    const auto wingsPalette = defaultWingsPalette();
    ok &= expect(wingsPalette[0] == RGB{0, 0, 0} &&
                     wingsPalette[16] == RGB{112, 112, 168} &&
                     wingsPalette[32] == RGB{236, 236, 236} &&
                     wingsPalette[48] == RGB{0, 0, 252} &&
                     wingsPalette[128] == RGB{0, 248, 0} &&
                     wingsPalette[255] == RGB{252, 252, 252},
                 "Wings palette must match COLORS.PCX");
    ok &= expect(isReservedPaletteIndex(GameId::Wings, 1) &&
                     !isReservedPaletteIndex(GameId::Wings, 16) &&
                     isReservedPaletteIndex(GameId::Wings, 17) &&
                     !isReservedPaletteIndex(GameId::Wings, 56) &&
                     isReservedPaletteIndex(GameId::Wings, 57) &&
                     !isReservedPaletteIndex(GameId::Wings, 64),
                 "Wings reserved palette index rules are incorrect");
    ok &= expect(!isPaletteColorEditable(GameId::Wings, 16) &&
                     isPaletteColorEditable(GameId::Wings, 48) &&
                     !isPaletteColorEditable(GameId::Wings, 57) &&
                     isPaletteColorEditable(GameId::VWing, 46) &&
                     !isPaletteColorEditable(GameId::Auts, 108),
                 "game palette color editing rules are incorrect");
    const auto wingsAll =
        paletteIndices(PaletteGroup::AllUsable, GameId::Wings);
    ok &= expect(wingsAll.size() == 219 &&
                     std::none_of(wingsAll.begin(), wingsAll.end(),
                                  [](const std::uint8_t index) {
                                      return isReservedPaletteIndex(
                                          GameId::Wings, index);
                                  }) &&
                     std::find(wingsAll.begin(), wingsAll.end(), 16) !=
                         wingsAll.end() &&
                     std::find(wingsAll.begin(), wingsAll.end(), 32) !=
                         wingsAll.end(),
                 "Wings palette groups must contain every usable index");
    const auto autsPalette = defaultAutsPalette();
    ok &= expect(autsPalette[0] == RGB{0, 0, 0} &&
                     autsPalette[7] == RGB{92, 92, 92} &&
                     autsPalette[39] == RGB{36, 36, 252} &&
                     autsPalette[92] == RGB{132, 132, 132} &&
                     autsPalette[95] == RGB{252, 252, 252},
                 "AUTS palette must match BLANK.BMP");
    ok &= expect(!isReservedPaletteIndex(GameId::Auts, 0) &&
                     !isReservedPaletteIndex(GameId::Auts, 255),
                 "AUTS palette indices should remain paintable");
    ok &= expect(paletteIndices(PaletteGroup::AllUsable, GameId::Auts).size() ==
                     256,
                 "AUTS must expose all palette indices for drawing");
    const std::array<std::pair<std::size_t, RGB>, 20> reservedDefaults{{
        {1, {0, 0, 171}},      {2, {0, 171, 0}},      {3, {0, 171, 171}},
        {4, {171, 0, 0}},      {5, {171, 0, 171}},    {6, {171, 87, 0}},
        {7, {171, 171, 171}},  {8, {87, 87, 87}},     {9, {87, 87, 255}},
        {10, {87, 255, 87}},   {11, {87, 255, 255}},  {12, {255, 87, 87}},
        {13, {255, 87, 255}},  {14, {255, 255, 87}},  {30, {227, 227, 227}},
        {37, {11, 47, 11}},    {46, {95, 71, 71}},    {52, {195, 255, 255}},
        {53, {0, 191, 255}},   {54, {0, 127, 255}},
    }};
    for (const auto &[index, color] : reservedDefaults) {
        ok &= expect(palette[index] == color,
                     "reserved palette index " + std::to_string(index) +
                         " should use its most common reference color");
    }

    const TestFile valid(std::filesystem::current_path() / "palette-test.pal");
    std::string error;
    ok &= expect(saveJascPalette(valid.path(), palette, error),
                 "could not save JASC palette: " + error);
    std::array<RGB, 256> loaded{};
    ok &= expect(loadJascPalette(valid.path(), loaded, error),
                 "could not load saved JASC palette: " + error);
    ok &= expect(loaded == palette, "JASC palette did not round-trip");

    const TestFile invalid(std::filesystem::current_path() /
                           "invalid-palette-test.pal");
    {
        std::ofstream output(invalid.path());
        output << "JASC-PAL\n0100\n2\n0 0 0\n255 255 255\n";
    }
    error.clear();
    ok &= expect(!loadJascPalette(invalid.path(), loaded, error),
                 "loader accepted a palette with fewer than 256 colors");
    ok &= expect(!error.empty(), "invalid palette did not report an error");

    if (!ok) {
        return 1;
    }
    std::cout << "All palette tests passed\n";
    return 0;
}
