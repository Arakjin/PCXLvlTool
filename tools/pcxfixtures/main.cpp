#include <array>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr std::size_t kWidth = 640;
constexpr std::size_t kHeight = 800;
constexpr std::size_t kPixelCount = kWidth * kHeight;
using Pixels = std::vector<std::uint8_t>;
using Palette = std::array<std::uint8_t, 256 * 3>;

void putLittleEndian16(std::array<std::uint8_t, 128>& header,
                       const std::size_t offset, const std::uint16_t value)
{
    header[offset] = static_cast<std::uint8_t>(value & 0xff);
    header[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

Palette makePalette()
{
    Palette palette{};
    for (std::size_t index = 0; index < 256; ++index) {
        palette[index * 3] = static_cast<std::uint8_t>(index);
        palette[index * 3 + 1] = static_cast<std::uint8_t>(index);
        palette[index * 3 + 2] = static_cast<std::uint8_t>(index);
    }
    return palette;
}

void writeRun(std::ofstream& output, const std::uint8_t value,
              const std::size_t length)
{
    if (length > 1 || value >= 0xc0) {
        output.put(static_cast<char>(0xc0 | length));
    }
    output.put(static_cast<char>(value));
}

void writePcx(const std::filesystem::path& path, const Pixels& pixels,
              const Palette& palette)
{
    if (pixels.size() != kPixelCount) {
        throw std::runtime_error("fixture has an invalid pixel count");
    }

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("could not create " + path.string());
    }

    std::array<std::uint8_t, 128> header{};
    header[0] = 0x0a; // ZSoft PCX manufacturer byte
    header[1] = 5;    // PCX 3.0 with a 256-color palette
    header[2] = 1;    // RLE encoding
    header[3] = 8;    // bits per pixel per plane
    putLittleEndian16(header, 8, static_cast<std::uint16_t>(kWidth - 1));
    putLittleEndian16(header, 10, static_cast<std::uint16_t>(kHeight - 1));
    putLittleEndian16(header, 12, static_cast<std::uint16_t>(kWidth));
    putLittleEndian16(header, 14, static_cast<std::uint16_t>(kHeight));
    header[65] = 1; // color planes
    putLittleEndian16(header, 66, static_cast<std::uint16_t>(kWidth));
    putLittleEndian16(header, 68, 1); // color/BW palette type
    putLittleEndian16(header, 70, static_cast<std::uint16_t>(kWidth));
    putLittleEndian16(header, 72, static_cast<std::uint16_t>(kHeight));

    output.write(reinterpret_cast<const char*>(header.data()),
                 static_cast<std::streamsize>(header.size()));

    for (std::size_t y = 0; y < kHeight; ++y) {
        const std::size_t rowStart = y * kWidth;
        std::size_t x = 0;
        while (x < kWidth) {
            const std::uint8_t value = pixels[rowStart + x];
            std::size_t length = 1;
            while (x + length < kWidth && length < 63 &&
                   pixels[rowStart + x + length] == value) {
                ++length;
            }
            writeRun(output, value, length);
            x += length;
        }
    }

    output.put(static_cast<char>(0x0c));
    output.write(reinterpret_cast<const char*>(palette.data()),
                 static_cast<std::streamsize>(palette.size()));
    if (!output) {
        throw std::runtime_error("could not write " + path.string());
    }
}

void saveFixture(const std::filesystem::path& directory,
                 const std::string& name, const Pixels& pixels,
                 const Palette& palette)
{
    const std::filesystem::path path = directory / name;
    writePcx(path, pixels, palette);
    std::cout << "Created " << path.string() << '\n';
}

} // namespace

int main(int argc, char* argv[])
{
    if (argc != 2) {
        std::cerr << "Usage: pcxfixtures <output-directory>\n";
        return 2;
    }

    try {
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        const Palette palette = makePalette();

        for (const std::uint8_t value : {std::uint8_t{0}, std::uint8_t{1},
                                         std::uint8_t{57}}) {
            const std::string name = value == 0 ? "T00.PCX"
                                    : value == 1 ? "T01.PCX"
                                                 : "T57.PCX";
            saveFixture(directory, name, Pixels(kPixelCount, value), palette);
        }

        struct PixelFixture {
            const char* name;
            std::size_t x;
            std::size_t y;
        };
        constexpr std::array<PixelFixture, 6> pixelFixtures{{
            {"P000.PCX", 0, 0},
            {"P100.PCX", 1, 0},
            {"P6390.PCX", 639, 0},
            {"P001.PCX", 0, 1},
            {"P0799.PCX", 0, 799},
            {"P639799.PCX", 639, 799},
        }};
        for (const PixelFixture& fixture : pixelFixtures) {
            Pixels pixels(kPixelCount, 0);
            pixels[fixture.y * kWidth + fixture.x] = 1;
            saveFixture(directory, fixture.name, pixels, palette);
        }

        Pixels vertical(kPixelCount);
        Pixels horizontal(kPixelCount);
        Pixels checkerboard(kPixelCount);
        Pixels longRuns(kPixelCount);
        Pixels random(kPixelCount);
        std::uint32_t randomState = 0x564c4556;

        for (std::size_t y = 0; y < kHeight; ++y) {
            for (std::size_t x = 0; x < kWidth; ++x) {
                const std::size_t offset = y * kWidth + x;
                vertical[offset] = static_cast<std::uint8_t>((x / 8) % 2);
                horizontal[offset] = static_cast<std::uint8_t>((y / 8) % 2);
                checkerboard[offset] = static_cast<std::uint8_t>(
                    ((x / 8) + (y / 8)) % 2);
                longRuns[offset] = static_cast<std::uint8_t>(
                    x < 320 ? 57 : 221);
                randomState = randomState * 1664525u + 1013904223u;
                random[offset] = static_cast<std::uint8_t>(randomState >> 24);
            }
        }

        saveFixture(directory, "VSTRIPE.PCX", vertical, palette);
        saveFixture(directory, "HSTRIPE.PCX", horizontal, palette);
        saveFixture(directory, "CHECKER.PCX", checkerboard, palette);
        saveFixture(directory, "LONGRUN.PCX", longRuns, palette);
        saveFixture(directory, "RANDOM.PCX", random, palette);

        Palette changedPalette = palette;
        changedPalette[57 * 3] = 1;
        changedPalette[57 * 3 + 1] = 2;
        changedPalette[57 * 3 + 2] = 3;
        saveFixture(directory, "T00PAL.PCX", Pixels(kPixelCount, 0),
                    changedPalette);
    } catch (const std::exception& error) {
        std::cerr << "pcxfixtures: " << error.what() << '\n';
        return 1;
    }

    return 0;
}
