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
    return ok ? 0 : 1;
}
