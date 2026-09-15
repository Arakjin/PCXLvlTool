#include "default_palette.h"
#include "palette_io.h"
#include "palette_rules.h"

#include <array>
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
