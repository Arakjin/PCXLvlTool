#include "overthrust_profile.h"

#include <utility>

namespace {

const std::array<OverThrustMaterial, 88> kOriginalMaterials{{
    {0, "Empty", "empty", "core:empty", "appearance:empty", RGB{8, 10, 18}},
    {1, "Rock A", "structural", "core:rock", "appearance:rock/a", RGB{105, 98, 92}},
    {2, "Rock B", "structural", "core:rock", "appearance:rock/b", RGB{126, 113, 99}},
    {3, "Bedrock Natural A", "structural", "core:bedrock", "appearance:bedrock/natural-a", RGB{49, 53, 62}},
    {4, "Bedrock Natural B", "structural", "core:bedrock", "appearance:bedrock/natural-b", RGB{61, 57, 70}},
    {5, "Bedrock Technical", "structural", "core:bedrock", "appearance:bedrock/technical", RGB{49, 72, 79}},
    {6, "Soil A", "structural", "core:soil", "appearance:soil/a", RGB{99, 66, 42}},
    {7, "Soil B", "structural", "core:soil", "appearance:soil/b", RGB{126, 79, 45}},
    {8, "Fertile Soil A", "structural", "core:fertile_soil", "appearance:fertile-soil/a", RGB{77, 57, 34}},
    {9, "Fertile Soil B", "structural", "core:fertile_soil", "appearance:fertile-soil/b", RGB{91, 66, 38}},
    {10, "Grass A", "organic", "core:grass", "appearance:grass/a", RGB{70, 154, 65}},
    {11, "Grass B", "organic", "core:grass", "appearance:grass/b", RGB{91, 176, 71}},
    {12, "Grass C", "organic", "core:grass", "appearance:grass/c", RGB{54, 132, 61}},
    {13, "Dry Grass A", "organic", "core:dry_grass", "appearance:dry-grass/a", RGB{181, 153, 70}},
    {14, "Dry Grass B", "organic", "core:dry_grass", "appearance:dry-grass/b", RGB{201, 174, 82}},
    {15, "Dry Grass C", "organic", "core:dry_grass", "appearance:dry-grass/c", RGB{157, 127, 52}},
    {16, "Wood A", "organic", "core:wood", "appearance:wood/a", RGB{120, 76, 42}},
    {17, "Wood B", "organic", "core:wood", "appearance:wood/b", RGB{145, 91, 48}},
    {18, "Wood C", "organic", "core:wood", "appearance:wood/c", RGB{100, 61, 36}},
    {19, "Dry Wood A", "organic", "core:dry_wood", "appearance:dry-wood/a", RGB{155, 108, 59}},
    {20, "Dry Wood B", "organic", "core:dry_wood", "appearance:dry-wood/b", RGB{176, 126, 71}},
    {21, "Dry Wood C", "organic", "core:dry_wood", "appearance:dry-wood/c", RGB{135, 91, 51}},
    {22, "Tree A", "organic", "core:tree", "appearance:tree/a", RGB{92, 59, 32}},
    {23, "Tree B", "organic", "core:tree", "appearance:tree/b", RGB{109, 67, 34}},
    {24, "Tree C", "organic", "core:tree", "appearance:tree/c", RGB{79, 48, 28}},
    {25, "Dead Tree A", "organic", "core:dead_tree", "appearance:dead-tree/a", RGB{117, 91, 68}},
    {26, "Dead Tree B", "organic", "core:dead_tree", "appearance:dead-tree/b", RGB{136, 103, 72}},
    {27, "Dead Tree C", "organic", "core:dead_tree", "appearance:dead-tree/c", RGB{94, 73, 57}},
    {28, "Broad Leaves", "organic", "core:leaves", "appearance:leaves/broad", RGB{48, 139, 69}},
    {29, "Conifer Leaves", "organic", "core:leaves", "appearance:leaves/conifer", RGB{35, 106, 69}},
    {30, "Dense Leaves", "organic", "core:leaves", "appearance:leaves/dense", RGB{62, 161, 77}},
    {31, "Dead Leaves A", "organic", "core:dead_leaves", "appearance:dead-leaves/a", RGB{154, 91, 38}},
    {32, "Dead Leaves B", "organic", "core:dead_leaves", "appearance:dead-leaves/b", RGB{177, 111, 42}},
    {33, "Dead Leaves C", "organic", "core:dead_leaves", "appearance:dead-leaves/c", RGB{127, 72, 31}},
    {34, "Vine A", "organic", "core:vine", "appearance:vine/a", RGB{49, 126, 55}},
    {35, "Vine B", "organic", "core:vine", "appearance:vine/b", RGB{62, 147, 61}},
    {36, "Vine C", "organic", "core:vine", "appearance:vine/c", RGB{39, 104, 48}},
    {37, "Dry Vine A", "organic", "core:dry_vine", "appearance:dry-vine/a", RGB{151, 124, 55}},
    {38, "Dry Vine B", "organic", "core:dry_vine", "appearance:dry-vine/b", RGB{174, 143, 66}},
    {39, "Dry Vine C", "organic", "core:dry_vine", "appearance:dry-vine/c", RGB{126, 101, 45}},
    {40, "Glass Clear", "structural", "core:glass", "appearance:glass/clear", RGB{152, 218, 226}},
    {41, "Glass Blue", "structural", "core:glass", "appearance:glass/blue", RGB{109, 183, 215}},
    {42, "Glass Green", "structural", "core:glass", "appearance:glass/green", RGB{124, 203, 177}},
    {43, "Aluminium A", "metal", "core:aluminium", "appearance:aluminium/a", RGB{184, 193, 204}},
    {44, "Aluminium B", "metal", "core:aluminium", "appearance:aluminium/b", RGB{161, 174, 188}},
    {45, "Aluminium C", "metal", "core:aluminium", "appearance:aluminium/c", RGB{207, 213, 220}},
    {46, "Titanium A", "metal", "core:titanium", "appearance:titanium/a", RGB{111, 123, 145}},
    {47, "Titanium B", "metal", "core:titanium", "appearance:titanium/b", RGB{129, 139, 160}},
    {48, "Titanium C", "metal", "core:titanium", "appearance:titanium/c", RGB{92, 105, 129}},
    {49, "Tungsten A", "metal", "core:tungsten", "appearance:tungsten/a", RGB{68, 72, 80}},
    {50, "Tungsten B", "metal", "core:tungsten", "appearance:tungsten/b", RGB{82, 85, 93}},
    {51, "Tungsten C", "metal", "core:tungsten", "appearance:tungsten/c", RGB{55, 59, 67}},
    {52, "Acid-resistant A", "structural", "core:acid_resistant", "appearance:acid-resistant/a", RGB{83, 112, 102}},
    {53, "Acid-resistant B", "structural", "core:acid_resistant", "appearance:acid-resistant/b", RGB{96, 130, 116}},
    {54, "Acid-resistant C", "structural", "core:acid_resistant", "appearance:acid-resistant/c", RGB{70, 96, 89}},
    {55, "Sand A", "granular", "core:sand", "appearance:sand/a", RGB{218, 190, 112}},
    {56, "Sand B", "granular", "core:sand", "appearance:sand/b", RGB{231, 204, 128}},
    {57, "Sand C", "granular", "core:sand", "appearance:sand/c", RGB{197, 166, 88}},
    {58, "Snow A", "granular", "core:snow", "appearance:snow/a", RGB{226, 239, 247}},
    {59, "Snow B", "granular", "core:snow", "appearance:snow/b", RGB{196, 220, 239}},
    {60, "Snow C", "granular", "core:snow", "appearance:snow/c", RGB{239, 246, 250}},
    {61, "Ash A", "granular", "core:ash", "appearance:ash/a", RGB{104, 104, 108}},
    {62, "Ash B", "granular", "core:ash", "appearance:ash/b", RGB{127, 124, 125}},
    {63, "Rubble A", "granular", "core:rubble", "appearance:rubble/a", RGB{118, 108, 99}},
    {64, "Rubble B", "granular", "core:rubble", "appearance:rubble/b", RGB{143, 129, 112}},
    {65, "Acid Neutralizer A", "granular", "core:acid_neutralizer", "appearance:neutralizer/a", RGB{221, 213, 181}},
    {66, "Acid Neutralizer B", "granular", "core:acid_neutralizer", "appearance:neutralizer/b", RGB{197, 191, 159}},
    {67, "Ice A", "structural", "core:ice", "appearance:ice/a", RGB{161, 220, 244}},
    {68, "Ice B", "structural", "core:ice", "appearance:ice/b", RGB{190, 236, 250}},
    {69, "Base Healing Pad", "gameplay", "core:base_pad", "appearance:base-pad/default", RGB{73, 236, 210}},
    {70, "Water A", "liquid", "core:water", "appearance:water/a", RGB{49, 132, 220}},
    {71, "Water B", "liquid", "core:water", "appearance:water/b", RGB{67, 161, 235}},
    {72, "Acid A", "liquid", "core:acid", "appearance:acid/a", RGB{112, 238, 67}},
    {73, "Acid B", "liquid", "core:acid", "appearance:acid/b", RGB{151, 255, 76}},
    {74, "Oil A", "liquid", "core:oil", "appearance:oil/a", RGB{54, 47, 42}},
    {75, "Oil B", "liquid", "core:oil", "appearance:oil/b", RGB{76, 63, 49}},
    {76, "Napalm A", "liquid", "core:napalm", "appearance:napalm/a", RGB{232, 84, 28}},
    {77, "Napalm B", "liquid", "core:napalm", "appearance:napalm/b", RGB{255, 119, 32}},
    {78, "Lava A", "liquid", "core:lava", "appearance:lava/a", RGB{245, 62, 20}},
    {79, "Lava B", "liquid", "core:lava", "appearance:lava/b", RGB{255, 148, 26}},
    {80, "Smoke A", "gas", "core:smoke", "appearance:smoke/a", RGB{88, 91, 101}},
    {81, "Smoke B", "gas", "core:smoke", "appearance:smoke/b", RGB{119, 121, 130}},
    {82, "Steam A", "gas", "core:steam", "appearance:steam/a", RGB{194, 211, 220}},
    {83, "Steam B", "gas", "core:steam", "appearance:steam/b", RGB{218, 228, 234}},
    {84, "Flammable Gas A", "gas", "core:flammable_gas", "appearance:flammable-gas/a", RGB{211, 181, 70}},
    {85, "Flammable Gas B", "gas", "core:flammable_gas", "appearance:flammable-gas/b", RGB{234, 205, 89}},
    {86, "Acidic Gas A", "gas", "core:acidic_gas", "appearance:acidic-gas/a", RGB{130, 79, 164}},
    {87, "Acidic Gas B", "gas", "core:acidic_gas", "appearance:acidic-gas/b", RGB{157, 98, 188}},
}};

std::vector<OverThrustMaterial> buildMaterials()
{
    std::vector<OverThrustMaterial> materials;
    materials.reserve(256);
    const auto append = [&](OverThrustMaterial material) {
        material.index = static_cast<std::uint8_t>(materials.size());
        materials.push_back(std::move(material));
    };
    for (const OverThrustMaterial& original : kOriginalMaterials) {
        append(original);
        if (original.index == 2) {
            append({0, "Rock C", "structural", "core:rock",
                    "appearance:rock/c", RGB{90, 84, 80}});
            append({0, "Rock D", "structural", "core:rock",
                    "appearance:rock/d", RGB{143, 128, 110}});
        } else if (original.index == 5) {
            append({0, "Bedrock Natural C", "structural", "core:bedrock",
                    "appearance:bedrock/natural-c", RGB{42, 46, 55}});
            append({0, "Bedrock Natural D", "structural", "core:bedrock",
                    "appearance:bedrock/natural-d", RGB{72, 67, 82}});
            append({0, "Bedrock Technical B", "structural", "core:bedrock",
                    "appearance:bedrock/technical-b", RGB{61, 84, 91}});
        } else if (original.index == 7) {
            append({0, "Soil C", "structural", "core:soil",
                    "appearance:soil/c", RGB{82, 53, 34}});
            append({0, "Soil D", "structural", "core:soil",
                    "appearance:soil/d", RGB{143, 91, 52}});
        } else if (original.index == 64) {
            append({0, "Rubble C", "granular", "core:rubble",
                    "appearance:rubble/c", RGB{99, 91, 84}});
            append({0, "Rubble D", "granular", "core:rubble",
                    "appearance:rubble/d", RGB{160, 145, 126}});
        }
    }
    for (int index = static_cast<int>(materials.size()); index < 256;
         ++index) {
        const int variant = index - 97;
        append({0,
                "Fly-through " + std::to_string(index),
                "fly_through",
                "core:empty",
                "appearance:empty/fly-through-" + std::to_string(index),
                RGB{static_cast<std::uint8_t>((53 + variant * 73) & 0xff),
                    static_cast<std::uint8_t>((97 + variant * 151) & 0xff),
                    static_cast<std::uint8_t>((193 + variant * 199) & 0xff)}});
    }
    return materials;
}

const std::vector<OverThrustMaterial> kMaterials = buildMaterials();

} // namespace

const std::vector<OverThrustMaterial>& overthrustMaterials()
{
    return kMaterials;
}

const OverThrustMaterial* overthrustMaterial(const std::uint8_t index)
{
    return index < kMaterials.size() ? &kMaterials[index] : nullptr;
}

std::array<RGB, 256> defaultOverThrustPalette()
{
    std::array<RGB, 256> palette{};
    // Initialize deterministically before applying every declared material or
    // fly-through preview color.
    for (std::size_t index = 0; index < palette.size(); ++index) {
        const auto value = static_cast<std::uint8_t>(index);
        palette[index] = RGB{value, value, value};
    }
    for (const OverThrustMaterial& material : kMaterials) {
        palette[material.index] = material.preview;
    }
    return palette;
}
