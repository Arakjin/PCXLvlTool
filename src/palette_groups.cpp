#include "palette_groups.h"

namespace {

void appendRange(std::vector<std::uint8_t>& indices, const int first,
                 const int last)
{
    for (int index = first; index <= last; ++index) {
        indices.push_back(static_cast<std::uint8_t>(index));
    }
}

} // namespace

std::vector<std::uint8_t> paletteIndices(const PaletteGroup group,
                                         const GameId game)
{
    std::vector<std::uint8_t> indices;
    indices.reserve(256);
    if (game == GameId::Auts) {
        switch (group) {
        case PaletteGroup::AllUsable:
            appendRange(indices, 0, 255);
            break;
        case PaletteGroup::Background:
            indices.push_back(0);
            break;
        case PaletteGroup::Indestructible:
            indices.push_back(7);
            break;
        case PaletteGroup::Water:
            indices.push_back(39);
            break;
        case PaletteGroup::Docking:
            appendRange(indices, 92, 95);
            break;
        case PaletteGroup::Other:
            for (int index = 0; index < 256; ++index) {
                if (index != 0 && index != 7 && index != 39 &&
                    (index < 92 || index > 95)) {
                    indices.push_back(static_cast<std::uint8_t>(index));
                }
            }
            break;
        default:
            break;
        }
        return indices;
    }
    if (game == GameId::Wings) {
        switch (group) {
        case PaletteGroup::AllUsable:
            indices.push_back(0);
            indices.push_back(16);
            appendRange(indices, 32, 56);
            appendRange(indices, 64, 255);
            break;
        case PaletteGroup::Background:
            indices.push_back(0);
            break;
        case PaletteGroup::Water:
            indices.push_back(16);
            appendRange(indices, 48, 53);
            break;
        case PaletteGroup::Bases:
            appendRange(indices, 32, 47);
            break;
        case PaletteGroup::Special:
            appendRange(indices, 54, 56);
            break;
        case PaletteGroup::FlyThrough:
            appendRange(indices, 64, 79);
            break;
        case PaletteGroup::Indestructible:
            appendRange(indices, 80, 95);
            break;
        case PaletteGroup::Soft:
            appendRange(indices, 96, 111);
            break;
        case PaletteGroup::BurningWings:
            appendRange(indices, 112, 127);
            break;
        case PaletteGroup::NormalTerrain:
            appendRange(indices, 128, 255);
            break;
        default:
            break;
        }
        return indices;
    }
    switch (group) {
    case PaletteGroup::AllUsable:
        indices.push_back(0);
        appendRange(indices, 16, 30);
        appendRange(indices, 32, 37);
        appendRange(indices, 39, 46);
        appendRange(indices, 48, 52);
        appendRange(indices, 56, 174);
        appendRange(indices, 176, 199);
        appendRange(indices, 201, 219);
        appendRange(indices, 221, 255);
        break;
    case PaletteGroup::Background:
        indices.push_back(0);
        break;
    case PaletteGroup::Water:
        appendRange(indices, 16, 19);
        break;
    case PaletteGroup::FlyThrough:
        appendRange(indices, 20, 30);
        break;
    case PaletteGroup::Font:
        appendRange(indices, 32, 37);
        break;
    case PaletteGroup::Special:
        appendRange(indices, 39, 46);
        appendRange(indices, 48, 52);
        indices.push_back(56);
        break;
    case PaletteGroup::NormalTerrain:
        appendRange(indices, 57, 149);
        break;
    case PaletteGroup::Burnable:
        appendRange(indices, 150, 174);
        appendRange(indices, 176, 199);
        break;
    case PaletteGroup::Underwater:
        appendRange(indices, 201, 219);
        break;
    case PaletteGroup::Indestructible:
        appendRange(indices, 221, 243);
        appendRange(indices, 248, 255);
        break;
    case PaletteGroup::Turrets:
        appendRange(indices, 244, 247);
        break;
    case PaletteGroup::Bases:
    case PaletteGroup::Soft:
    case PaletteGroup::BurningWings:
    case PaletteGroup::Docking:
    case PaletteGroup::Other:
        break;
    }
    return indices;
}

