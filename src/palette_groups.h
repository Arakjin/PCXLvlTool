#pragma once

#include "game_profile.h"

#include <cstdint>
#include <vector>

enum class PaletteGroup {
    AllUsable,
    Background,
    Water,
    FlyThrough,
    Font,
    Special,
    NormalTerrain,
    Burnable,
    Underwater,
    Indestructible,
    Turrets,
    Bases,
    Soft,
    BurningWings,
    Docking,
    Other,
    Structural,
    Organic,
    Metal,
    Granular,
    Liquid,
    Gas,
    Gameplay,
};

std::vector<std::uint8_t> paletteIndices(PaletteGroup group, GameId game);

