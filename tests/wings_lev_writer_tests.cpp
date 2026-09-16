#include "default_palette.h"
#include "wings_lev_writer.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
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

std::uint16_t u16(const std::vector<std::uint8_t>& bytes,
                  const std::size_t offset)
{
    return static_cast<std::uint16_t>(bytes[offset] |
                                      (bytes[offset + 1] << 8));
}

std::uint32_t u32(const std::vector<std::uint8_t>& bytes,
                  const std::size_t offset)
{
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

std::vector<std::uint8_t> decode(const std::vector<std::uint8_t>& bytes,
                                 std::size_t offset, const std::size_t size,
                                 const std::size_t expected)
{
    std::vector<std::uint8_t> pixels;
    const std::size_t end = offset + size;
    while (offset < end) {
        const std::uint8_t token = bytes[offset++];
        std::size_t count = 1;
        std::uint8_t value = token;
        if ((token & 0xc0) == 0xc0) {
            count = token & 0x3f;
            value = bytes[offset++];
        }
        pixels.insert(pixels.end(), count, value);
    }
    if (pixels.size() != expected) {
        pixels.clear();
    }
    return pixels;
}

std::vector<std::uint8_t> readFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

} // namespace

int main()
{
    bool ok = true;
    const std::filesystem::path path =
        std::filesystem::current_path() / "wings-writer-test.lev";
    std::error_code ignored;

    Level level;
    level.resize(157, 90);
    level.palette = defaultWingsPalette();
    level.pixels[0] = 0xc0;
    level.pixels[1] = 7;
    level.pixels[2] = 7;
    level.pixels[156] = 9;
    Level background;
    background.resize(157, 90);
    background.palette = level.palette;
    background.pixels[0] = 128;
    background.pixels[3] = 129;

    LevelCreationSettings settings;
    settings.game = GameId::Wings;
    settings.width = 157;
    settings.height = 90;
    settings.backgroundMode = BackgroundMode::None;
    settings.stars = true;
    settings.rainProbability = 11;
    settings.snowProbability = 12;
    settings.bombingProbability = 13;
    settings.civilians = 14;
    settings.armedCiviliansProbability = 15;
    std::string error;
    ok &= expect(saveWingsLev(path, level, nullptr, settings, error),
                 "Wings LEV without parallax should save");
    auto bytes = readFile(path);
    ok &= expect(bytes.size() > 768 + 8 + 13, "Wings LEV is too short");
    ok &= expect(bytes[16 * 3] == 28 && bytes[16 * 3 + 1] == 28 &&
                     bytes[16 * 3 + 2] == 42,
                 "palette should be converted to 6-bit VGA values");
    ok &= expect(u16(bytes, 768) == 157 && u16(bytes, 770) == 90,
                 "main image dimensions");
    const std::uint32_t mainSize = u32(bytes, 772);
    const auto mainPixels = decode(bytes, 776, mainSize, 157 * 90);
    ok &= expect(!mainPixels.empty() && mainPixels[0] == 0xc0 &&
                     mainPixels[1] == 7 && mainPixels[3] == 0,
                 "main image pixels should round-trip through Wings RLE");
    std::size_t tail = 776 + mainSize;
    ok &= expect(bytes[tail++] == 0, "no-parallax flag");
    ok &= expect(bytes[tail++] == 1 && u16(bytes, tail) == 2,
                 "stars and constant settings field");
    tail += 2;
    ok &= expect(u16(bytes, tail) == 11 && u16(bytes, tail + 2) == 12 &&
                     u16(bytes, tail + 4) == 13 &&
                     u16(bytes, tail + 6) == 14 &&
                     u16(bytes, tail + 8) == 15,
                 "Wings gameplay settings footer");

    const auto parallaxSize = wingsParallaxSize(157, 90);
    background.resize(parallaxSize.first, parallaxSize.second);
    background.palette = level.palette;
    background.pixels[0] = 64;
    background.pixels.back() = 79;
    settings.backgroundMode = BackgroundMode::Parallax;
    settings.stars = false;
    ok &= expect(saveWingsLev(path, level, &background, settings, error),
                 "parallax Wings LEV should save");
    bytes = readFile(path);
    const std::uint32_t parallaxMainSize = u32(bytes, 772);
    std::size_t parallax = 776 + parallaxMainSize;
    ok &= expect(bytes[parallax++] == 1, "parallax flag");
    ok &= expect(u16(bytes, parallax) == parallaxSize.first &&
                     u16(bytes, parallax + 2) == parallaxSize.second,
                 "parallax dimensions");
    const std::uint32_t encodedBackground = u32(bytes, parallax + 4);
    const auto backgroundPixels =
        decode(bytes, parallax + 8, encodedBackground,
               static_cast<std::size_t>(parallaxSize.first) *
                   parallaxSize.second);
    ok &= expect(!backgroundPixels.empty() && backgroundPixels.front() == 64 &&
                     backgroundPixels.back() == 79,
                 "parallax image pixels");

    background.resize(100, 100);
    ok &= expect(!saveWingsLev(path, level, &background, settings, error) &&
                     !error.empty(),
                 "invalid parallax size should be rejected");
    std::filesystem::remove(path, ignored);
    return ok ? 0 : 1;
}
