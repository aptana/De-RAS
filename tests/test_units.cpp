// Unit-system constants and explicit conversions.
#include "TestMain.h"
#include "core/Units.h"
using namespace deras;

TEST(constants) {
    auto si = constantsFor(UnitSystem::SI), us = constantsFor(UnitSystem::USCustomary);
    CHECK_NEAR(si.manningK, 1.0, 0);
    CHECK_NEAR(us.manningK, 1.486, 0);
    CHECK_NEAR(si.gravity, 9.81, 0);
    CHECK_NEAR(us.gravity, 32.174, 0);
    CHECK_NEAR(si.defaultStdStepTol, 0.003, 0);
    CHECK_NEAR(us.defaultStdStepTol, 0.01, 0);
}
TEST(conversions) {
    CHECK_NEAR(convertLength(1.0, UnitSystem::USCustomary, UnitSystem::SI), 0.3048, 1e-15);
    CHECK_NEAR(convertLength(0.3048, UnitSystem::SI, UnitSystem::USCustomary), 1.0, 1e-15);
    CHECK_NEAR(convertDischarge(1.0, UnitSystem::USCustomary, UnitSystem::SI), 0.028316846592, 1e-12);
    CHECK_NEAR(convertDischarge(5.0, UnitSystem::SI, UnitSystem::SI), 5.0, 0);
}
TEST(typed_quantities) {
    Elevation a(105.0), b(100.0);
    Length d = a - b;
    CHECK_NEAR(d.value(), 5.0, 0);
    Elevation c = b + Length(2.5);
    CHECK_NEAR(c.value(), 102.5, 0);
    Discharge q1(10), q2(5);
    CHECK_NEAR((q1 + q2).value(), 15, 0);
    // Elevation + Discharge does not compile (checked by design, not at runtime).
}
