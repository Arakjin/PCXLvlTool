#include "lev_reader.h"
#include "lev_writer.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

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
        return false;
    }
    return true;
}

std::vector<std::uint8_t> readBytes(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input),
            std::istreambuf_iterator<char>()};
}

} // namespace

int main()
{
    const TestFile file(std::filesystem::current_path() / "lev-writer-test.lev");
    auto source = std::make_unique<Level>();
    source->name = "12345678901234567890";
    for (std::size_t y = 0; y < Level::Height; ++y) {
        for (std::size_t x = 0; x < Level::Width; ++x) {
            source->pixels[y * Level::Width + x] =
                static_cast<std::uint8_t>((x + y) % 256);
        }
    }
    for (std::size_t index = 0; index < source->palette.size(); ++index) {
        source->palette[index] = RGB{
            static_cast<std::uint8_t>(index),
            static_cast<std::uint8_t>(255 - index),
            static_cast<std::uint8_t>(index / 2),
        };
    }

    std::string error;
    bool ok = expect(saveLev(file.path(), *source, error),
                     "valid LEV could not be written: " + error);

    const std::vector<std::uint8_t> bytes = readBytes(file.path());
    ok &= expect(bytes.size() > 128 && bytes[0] == 0x76 && bytes[1] == 0x07,
                 "classic LEV signature is missing");
    if (bytes.size() > 128) {
        ok &= expect(bytes[22] == 0 && bytes[23] == 0 && bytes[24] == 0 &&
                         bytes[25] == 0x08,
                     "20-character name boundary is incorrect");
    }

    auto decoded = std::make_unique<Level>();
    ok &= expect(loadLev(file.path(), *decoded, error),
                 "written LEV could not be loaded: " + error);
    ok &= expect(decoded->name == source->name, "level name did not round-trip");
    ok &= expect(decoded->pixels == source->pixels, "pixels did not round-trip");
    ok &= expect(decoded->palette == source->palette,
                 "palette did not round-trip");

    source->name = "123456789012345678901";
    error.clear();
    ok &= expect(!saveLev(file.path(), *source, error),
                 "writer accepted a 21-byte level name");
    ok &= expect(error.find("longer than 20") != std::string::npos,
                 "writer reported an unexpected long-name error");

    source->name = std::string("BAD\nNAME");
    error.clear();
    ok &= expect(!saveLev(file.path(), *source, error),
                 "writer accepted a non-printable level name");

    source->name = std::string("PÄIVÄ");
    error.clear();
    ok &= expect(!saveLev(file.path(), *source, error),
                 "writer accepted non-ASCII letters in a level name");

    if (!ok) {
        return 1;
    }
    std::cout << "All LEV writer tests passed\n";
    return 0;
}
