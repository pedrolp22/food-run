#include "TestSupport.h"
#include "game/MenuIconAnimation.h"
#include <limits>

void menuIconTests() {
    game::MenuIconAnimation icon, other;
    const auto atRest = [](const game::MenuIconAnimation& animation) {
        const auto pose = animation.pose();
        return pose.offsetX == 0 && pose.roll == 0 && pose.yaw == 0;
    };
    icon.advance(100);
    CHECK(atRest(icon) && !icon.spinning() && !icon.shaking());
    icon.click(false);
    icon.advance(.03f);
    const auto shake = icon.pose();
    CHECK(std::abs(shake.offsetX) > .01f && std::abs(shake.roll) > .01f);
    CHECK(!icon.spinning() && icon.shaking() && atRest(other));
    icon.advance(1);
    CHECK(atRest(icon) && !icon.shaking());
    icon.click(true);
    float previous = 0;
    for (int frame = 0; frame < 80; ++frame) {
        icon.advance(.01f);
        CHECK(icon.pose().yaw > previous && icon.pose().yaw < 2 * engine::math::kPi);
        CHECK(icon.pose().offsetX == 0 && icon.pose().roll == 0);
        previous = icon.pose().yaw;
    }
    // Double-clicking again extends the turn without teleporting its orientation.
    icon.click(true);
    CHECK(near(icon.pose().yaw, previous));
    icon.advance(.01f);
    CHECK(icon.pose().yaw > previous);
    icon.advance(1);
    CHECK(atRest(icon) && !icon.spinning());
    icon.click(true);
    icon.advance(.2f);
    previous = icon.pose().yaw;
    icon.click(false); // A shake can accompany a turn without cancelling it.
    CHECK(near(icon.pose().yaw, previous));
    icon.advance(-1);
    icon.advance(std::numeric_limits<float>::quiet_NaN());
    CHECK(near(icon.pose().yaw, previous));
    icon.advance(2);
    CHECK(atRest(icon));
    icon.click(true);
    icon = {}; // Leaving Credits resets all reactions.
    CHECK(atRest(icon) && !icon.spinning() && !icon.shaking());
}
