#include "pcx_writer.h"

#include <array>
#include <cstdint>
#include <exception>
#include <fstream>
#include <stdexcept>

namespace {

void putLittleEndian16(std::array<std::uint8_t, 128>& header,
                       const std::size_t offset, const std::uint16_t value)
{
    header[offset] = static_cast<std::uint8_t>(value & 0xff);
    header[offset + 1] = static_cast<std::uint8_t>((value >> 8) & 0xff);
}

void writeRun(std::ofstream& output, const std::uint8_t value,
              const std::size_t length)
{
    if (length > 1 || value >= 0xc0) {
        output.put(static_cast<char>(0xc0 | length));
    }
    output.put(static_cast<char>(value));
}

} // namespace

bool savePcx(const std::filesystem::path& path, const Level& level,
             std::string& error)
{
    error.clear();

    try {
        std::ofstream output(path, std::ios::binary);
        if (!output) {
            throw std::runtime_error("could not create output file");
        }

        std::array<std::uint8_t, 128> header{};
        header[0] = 0x0a;
        header[1] = 5;
        header[2] = 1;
        header[3] = 8;
        putLittleEndian16(header, 8,
                          static_cast<std::uint16_t>(Level::Width - 1));
        putLittleEndian16(header, 10,
                          static_cast<std::uint16_t>(Level::Height - 1));
        putLittleEndian16(header, 12,
                          static_cast<std::uint16_t>(Level::Width));
        putLittleEndian16(header, 14,
                          static_cast<std::uint16_t>(Level::Height));
        header[65] = 1;
        putLittleEndian16(header, 66,
                          static_cast<std::uint16_t>(Level::Width));
        putLittleEndian16(header, 68, 1);
        putLittleEndian16(header, 70,
                          static_cast<std::uint16_t>(Level::Width));
        putLittleEndian16(header, 72,
                          static_cast<std::uint16_t>(Level::Height));

        output.write(reinterpret_cast<const char*>(header.data()),
                     static_cast<std::streamsize>(header.size()));

        for (std::size_t y = 0; y < Level::Height; ++y) {
            const std::size_t rowStart = y * Level::Width;
            std::size_t x = 0;
            while (x < Level::Width) {
                const std::uint8_t value = level.pixels[rowStart + x];
                std::size_t length = 1;
                while (x + length < Level::Width && length < 63 &&
                       level.pixels[rowStart + x + length] == value) {
                    ++length;
                }
                writeRun(output, value, length);
                x += length;
            }
        }

        output.put(static_cast<char>(0x0c));
        for (const RGB& color : level.palette) {
            output.put(static_cast<char>(color.r));
            output.put(static_cast<char>(color.g));
            output.put(static_cast<char>(color.b));
        }

        output.close();
        if (!output) {
            throw std::runtime_error("could not write the complete PCX file");
        }
        return true;
    } catch (const std::exception& exception) {
        error = exception.what();
        return false;
    }
}
