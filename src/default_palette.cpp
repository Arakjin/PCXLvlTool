#include "default_palette.h"

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

std::uint8_t interpolate(const std::uint8_t from, const std::uint8_t to,
                         const int position, const int count)
{
    if (count <= 1) {
        return from;
    }
    const int value = static_cast<int>(from) +
                      (static_cast<int>(to) - static_cast<int>(from)) *
                          position / (count - 1);
    return static_cast<std::uint8_t>(value);
}

void fillGradient(std::array<RGB, 256> &palette, const int first,
                  const int last, const RGB from, const RGB to)
{
    const int count = last - first + 1;
    for (int index = first; index <= last; ++index) {
        const int position = index - first;
        palette[static_cast<std::size_t>(index)] = RGB{
            interpolate(from.r, to.r, position, count),
            interpolate(from.g, to.g, position, count),
            interpolate(from.b, to.b, position, count),
        };
    }
}

} // namespace

std::array<RGB, 256> defaultVWingPalette()
{
    std::array<RGB, 256> palette{};
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto gray = static_cast<std::uint8_t>(index);
        palette[index] = RGB{gray, gray, gray};
    }

    palette[0] = RGB{0, 0, 0};
    // Per-index modes from the palettes embedded in LEVEL1.LEV-LEVEL11.LEV.
    // These entries remain unavailable for painting, but retain representative
    // game palette values instead of editor warning colors.
    palette[1] = RGB{0, 0, 171};
    palette[2] = RGB{0, 171, 0};
    palette[3] = RGB{0, 171, 171};
    palette[4] = RGB{171, 0, 0};
    palette[5] = RGB{171, 0, 171};
    palette[6] = RGB{171, 87, 0};
    palette[7] = RGB{171, 171, 171};
    palette[8] = RGB{87, 87, 87};
    palette[9] = RGB{87, 87, 255};
    palette[10] = RGB{87, 255, 87};
    palette[11] = RGB{87, 255, 255};
    palette[12] = RGB{255, 87, 87};
    palette[13] = RGB{255, 87, 255};
    palette[14] = RGB{255, 255, 87};
    palette[15] = RGB{20, 85, 190};
    palette[16] = RGB{45, 170, 235};
    palette[17] = RGB{20, 120, 220};
    palette[18] = RGB{15, 70, 165};
    fillGradient(palette, 19, 29, RGB{25, 35, 55}, RGB{115, 155, 190});
    palette[30] = RGB{227, 227, 227};
    fillGradient(palette, 31, 36, RGB{245, 245, 245}, RGB{125, 125, 125});
    palette[37] = RGB{11, 47, 11};
    palette[38] = RGB{175, 235, 255};
    fillGradient(palette, 39, 44, RGB{255, 210, 20}, RGB{205, 25, 15});
    palette[45] = RGB{135, 90, 45};
    palette[46] = RGB{95, 71, 71};
    palette[47] = RGB{155, 0, 20};
    palette[48] = RGB{145, 90, 55};
    palette[49] = RGB{70, 105, 145};
    palette[50] = RGB{75, 75, 75};
    palette[51] = RGB{245, 250, 255};
    palette[52] = RGB{195, 255, 255};
    palette[53] = RGB{0, 191, 255};
    palette[54] = RGB{0, 127, 255};
    palette[55] = RGB{90, 210, 240};

    fillGradient(palette, 56, 78, RGB{70, 45, 25}, RGB{185, 130, 70});
    fillGradient(palette, 79, 102, RGB{30, 65, 25}, RGB{125, 175, 70});
    fillGradient(palette, 103, 125, RGB{55, 55, 60}, RGB{185, 185, 190});
    fillGradient(palette, 126, 148, RGB{115, 85, 45}, RGB{230, 205, 130});
    palette[149] = RGB{95, 95, 100};
    fillGradient(palette, 150, 173, RGB{80, 40, 20}, RGB{225, 130, 35});
    palette[174] = RGB{83, 99, 115};
    fillGradient(palette, 175, 198, RGB{210, 70, 20}, RGB{65, 65, 65});
    palette[199] = RGB{0, 19, 67};
    palette[200] = RGB{65, 80, 90};
    palette[201] = RGB{105, 85, 70};
    palette[202] = RGB{145, 215, 235};
    fillGradient(palette, 203, 218, RGB{45, 100, 105}, RGB{35, 150, 190});
    palette[219] = RGB{35, 67, 59};
    fillGradient(palette, 220, 242, RGB{55, 60, 70}, RGB{175, 185, 200});
    palette[243] = RGB{255, 230, 80};
    palette[244] = RGB{95, 105, 115};
    palette[245] = RGB{220, 210, 125};
    palette[246] = RGB{70, 80, 90};
    fillGradient(palette, 247, 255, RGB{65, 65, 70}, RGB{210, 210, 215});
    return palette;
}

std::array<RGB, 256> defaultWingsPalette()
{
    // Exact 256-color palette from the Wings 1.40 COLORS.PCX reference file.
    constexpr std::string_view hex =
        "000000000044f0000000d000fcfc00fc9020cc0000dc4010e86c04e89c00ecb400f0c800f4e000fcfc00c0e8e80404fc7070a8cc8448e09c48ecbc7cfce4bc8080fcb000008c0000000000000000000000000000680000a4d0dcd4f4fcfcfcfc"
        "ecececd4d4d4bcbcbca4a4a48c8c8c7474745c5c5c484848dc0000ac000000c40000ac00e8e800d0d000e88414d470000000fc0000fc0000fc0000fcc0e8e8e8e8fcf4e000dc3c10ec580c000000000000000000000000000000000000000000"
        "e8e8fcd4d4f0c4c4e8b8b8dca8a8d09898c88c8cbc7c7cb47070a86464a05c5c9450508c4444803c3c7834346c2c2c649c9c9c9090908888888080807878787070706464645c5c5c5454544c4c4c444444383838303030282828202020181818"
        "d4fcd4a0fca068fc6800fc0000f00000e00000d00000bc0000a800009400008000007000005c00004c00003800002800fcd8b0f0c89ce4b888d8ac78d09c68c49058b8844cac783ca46c309864288c581c844c1478440c6c3c04603400582c00"
        "00f80000d80000bc00009c000080000060000040000024000000f80000dc0000bc00009c000080000060000040000024e4dcfcccbcf8bc9cf4b080f0a068e49450d88c38cc8428c07818b0740ca06c04906400845000683c004c240030100014"
        "c8fcfc80f8f83cf4f400f0f000e0e000d0d000c0c000b4b400a0a0008c8c007878006464005050003c3c002828001414fcfcf0fcfcbcfcfc8cfcfc00f0ec0ce4e020dcd408d0c000bca400a88c009478008464007050005c3c00482c00382000"
        "fcc8c8fcb0b0fc9898fc8484fc6c6cfc5454fc4040fc0000e00000c40000ac00009000007400005c0000400000280000fce8e0fcdcccfcd0b8fcc4a8fcb894f4ac84eca474e89c68e0945cdc8c50cc844cbc7c44b07040a0683c906038845834"
        "744c2c644428543820482c1838241428180c1810080c04040000000808081010101c1c1c2828283434344040404848485454546060606c6c6c7474748080808c8c8c989898a0a0a0acacacb8b8b8c4c4c4d0d0d0d8d8d8e4e4e4f0f0f0fcfcfc";
    const auto nibble = [](const char value) -> std::uint8_t {
        return static_cast<std::uint8_t>(value <= '9' ? value - '0'
                                                     : value - 'a' + 10);
    };
    std::array<RGB, 256> palette{};
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto channel = [&](const std::size_t component) {
            const std::size_t offset = (index * 3 + component) * 2;
            return static_cast<std::uint8_t>((nibble(hex[offset]) << 4) |
                                             nibble(hex[offset + 1]));
        };
        palette[index] = {channel(0), channel(1), channel(2)};
    }
    return palette;
}
