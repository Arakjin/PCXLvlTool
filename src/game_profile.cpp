#include "game_profile.h"

#include <stdexcept>
#include <vector>

namespace {

const std::vector<GameProfile> kProfiles{
    {GameId::VWing, "vwing", "V-Wing", 640, 800, 640, 800, 640, 800,
     640ULL * 800ULL,
     GameFeature::None},
    {GameId::Wings, "wings", "Wings", 157, 90, 1000, 1000, 400, 400,
     1000ULL * 1000ULL,
     GameFeature::VariableLevelSize | GameFeature::ParallaxBackground |
         GameFeature::Stars | GameFeature::Weather |
         GameFeature::Civilians},
    {GameId::Auts, "auts", "AUTS", 320, 400, 320, 400, 320, 400,
     320ULL * 400ULL,
     GameFeature::None},
    {GameId::OverThrust, "overthrust", "OverThrust", 1, 1, 4096, 4096,
     400, 400, 4194304ULL,
     GameFeature::VariableLevelSize | GameFeature::ParallaxBackground |
         GameFeature::ExternalTrueColorBackground |
         GameFeature::LevelObjects | GameFeature::GameSpecificSettings |
         GameFeature::PackagePublishing},
};

} // namespace

const GameProfile& gameProfile(const GameId id)
{
    for (const GameProfile& profile : kProfiles) {
        if (profile.id == id) {
            return profile;
        }
    }
    throw std::invalid_argument("unknown game profile");
}

const GameProfile* gameProfileByKey(const std::string_view key)
{
    for (const GameProfile& profile : kProfiles) {
        if (profile.key == key) {
            return &profile;
        }
    }
    return nullptr;
}

const std::vector<GameProfile>& availableGameProfiles()
{
    return kProfiles;
}
