// De-RAS - core/Units.h
// Explicit unit system and strongly typed physical quantities.
//
// HEC-RAS projects are stored either in US Customary units (ft, cfs) or in
// SI units (m, m3/s). De-RAS keeps every value in the *project* unit system
// and only converts when the caller asks for it explicitly. Quantities of
// different dimensions cannot be mixed by accident: Length + Discharge does
// not compile.
#pragma once

#include <cmath>
#include <string>
#include <type_traits>

namespace deras {

enum class UnitSystem { USCustomary, SI };

// Physical constants that depend on the unit system.
struct UnitConstants {
    double gravity;        // ft/s2 or m/s2
    double manningK;       // 1.486 (US) or 1.0 (SI), Manning's equation constant
    double defaultStdStepTol;   // 0.01 ft or 0.003 m
    double defaultCriticalTol;  // 0.01 ft or 0.003 m
    double defaultMaxErrorTol;  // 0.3 ft or 0.1 m
};

inline UnitConstants constantsFor(UnitSystem u) {
    if (u == UnitSystem::SI) return {9.81, 1.0, 0.003, 0.003, 0.1};
    return {32.174, 1.486, 0.01, 0.01, 0.3};
}

inline const char* lengthUnit(UnitSystem u) { return u == UnitSystem::SI ? "m" : "ft"; }
inline const char* flowUnit(UnitSystem u) { return u == UnitSystem::SI ? "m3/s" : "cfs"; }
inline const char* areaUnit(UnitSystem u) { return u == UnitSystem::SI ? "m2" : "ft2"; }
inline const char* velocityUnit(UnitSystem u) { return u == UnitSystem::SI ? "m/s" : "ft/s"; }

// Exact conversion factors (international foot).
constexpr double kMetersPerFoot = 0.3048;
constexpr double kCmsPerCfs = 0.3048 * 0.3048 * 0.3048;  // 0.028316846592

// Tag-typed quantity. The value is expressed in the unit system recorded
// alongside it by the owner (normally the project). Only same-dimension
// arithmetic is allowed.
template <class Tag>
struct Quantity {
    double v = 0.0;
    constexpr Quantity() = default;
    constexpr explicit Quantity(double x) : v(x) {}
    constexpr double value() const { return v; }
    // Two elevations cannot be added; their difference is a Length (see below).
    constexpr Quantity operator+(Quantity o) const requires(!std::is_same_v<Tag, struct ElevationTag>) { return Quantity(v + o.v); }
    constexpr Quantity operator-(Quantity o) const requires(!std::is_same_v<Tag, struct ElevationTag>) { return Quantity(v - o.v); }
    constexpr Quantity operator*(double s) const { return Quantity(v * s); }
    constexpr Quantity operator/(double s) const { return Quantity(v / s); }
    constexpr bool operator<(Quantity o) const { return v < o.v; }
    constexpr bool operator>(Quantity o) const { return v > o.v; }
    constexpr bool operator<=(Quantity o) const { return v <= o.v; }
    constexpr bool operator>=(Quantity o) const { return v >= o.v; }
    constexpr bool operator==(Quantity o) const { return v == o.v; }
};

struct LengthTag {};
struct DischargeTag {};
struct AreaTag {};
struct VelocityTag {};

using Length = Quantity<LengthTag>;        // horizontal distance, station, depth
using Elevation = Quantity<ElevationTag>;  // vertical datum-referenced level
using Discharge = Quantity<DischargeTag>;
using Area = Quantity<AreaTag>;
using Velocity = Quantity<VelocityTag>;

// Elevation - Elevation = Length (a depth/difference), Elevation + Length = Elevation.
inline Length operator-(Elevation a, Elevation b) { return Length(a.v - b.v); }
inline Elevation operator+(Elevation a, Length d) { return Elevation(a.v + d.v); }

// Explicit conversions between unit systems.
inline double convertLength(double x, UnitSystem from, UnitSystem to) {
    if (from == to) return x;
    return from == UnitSystem::USCustomary ? x * kMetersPerFoot : x / kMetersPerFoot;
}
inline double convertDischarge(double q, UnitSystem from, UnitSystem to) {
    if (from == to) return q;
    return from == UnitSystem::USCustomary ? q * kCmsPerCfs : q / kCmsPerCfs;
}

inline std::string unitSystemName(UnitSystem u) { return u == UnitSystem::SI ? "SI" : "US"; }

}  // namespace deras
