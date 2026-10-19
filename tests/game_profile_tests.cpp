#include "game_profile.h"

#include <iostream>

namespace {

bool expect(const bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

} // namespace

int main()
{
    bool ok = true;
    const GameProfile& vwing = gameProfile(GameId::VWing);
    const GameProfile& wings = gameProfile(GameId::Wings);
    const GameProfile& auts = gameProfile(GameId::Auts);
    const GameProfile& overthrust = gameProfile(GameId::OverThrust);
    ok &= expect(vwing.defaultWidth == 640 && vwing.defaultHeight == 800,
                 "V-Wing profile dimensions");
    ok &= expect(hasFeature(wings.features,
                            GameFeature::ParallaxBackground),
                 "Wings parallax feature");
    ok &= expect(gameProfileByKey("wings") == &wings,
                 "profile lookup by stable key");
    ok &= expect(wingsParallaxSize(400, 600) == std::pair<int, int>{278, 345},
                 "documented Wings parallax example");
    ok &= expect(wingsParallaxSize(157, 90) == std::pair<int, int>{156, 90},
                 "integer division for odd dimensions");
    ok &= expect(auts.minimumWidth == 320 && auts.maximumWidth == 320 &&
                     auts.minimumHeight == 400 && auts.maximumHeight == 400 &&
                     gameProfileByKey("auts") == &auts,
                 "AUTS fixed-size profile");
    ok &= expect(overthrust.defaultWidth == 400 &&
                     overthrust.defaultHeight == 400 &&
                     overthrust.maximumWidth == 4096 &&
                     overthrust.maximumHeight == 4096 &&
                     overthrust.maximumCells == 4194304ULL &&
                     gameProfileByKey("overthrust") == &overthrust &&
                     hasFeature(overthrust.features,
                                GameFeature::ParallaxBackground) &&
                     hasFeature(overthrust.features,
                                GameFeature::ExternalTrueColorBackground) &&
                     hasFeature(overthrust.features,
                                GameFeature::PackagePublishing),
                 "OverThrust authoring profile");
    return ok ? 0 : 1;
}
