#include "game_profile.h"

#include <stdexcept>
#include <vector>

namespace {

const std::vector<GameProfile> kProfiles{
    {GameId::VWing, "vwing", "V-Wing", 640, 800, 640, 800, 640, 800,
     GameFeature::None},
    {GameId::Wings, "wings", "Wings", 157, 90, 1000, 1000, 400, 400,
     GameFeature::VariableLevelSize | GameFeature::ParallaxBackground |
         GameFeature::Stars | GameFeature::Weather |
         GameFeature::Civilians},
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
