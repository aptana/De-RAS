// De-RAS - ui/XsUtil.h
// Small helpers shared by the geometry views.
#pragma once

#include <cmath>
#include <optional>

#include "core/Model.h"

namespace deras::ui {

inline double manningAt(const CrossSection& x, double station) {
    double n = x.manning.empty() ? NAN : x.manning.front().n;
    for (const auto& b : x.manning) if (b.station <= station + 1e-9) n = b.n;
    return n;
}

// Left overbank / channel / right overbank n when the section uses the usual
// three-value layout (breaks at the first station and both banks).
struct ThreeN { double lob = NAN, ch = NAN, rob = NAN; bool simple = false; };

inline ThreeN threeN(const CrossSection& x) {
    ThreeN t;
    if (x.manning.empty() || x.staElev.empty()) return t;
    const double s0 = x.staElev.front().station;
    t.lob = manningAt(x, s0);
    t.ch = x.bankLeft ? manningAt(x, *x.bankLeft) : t.lob;
    t.rob = x.bankRight ? manningAt(x, *x.bankRight) : t.ch;
    // simple = every break is at the first station or a bank station
    t.simple = true;
    for (const auto& b : x.manning) {
        const bool atStart = std::fabs(b.station - s0) < 1e-6 || b.station < s0;
        const bool atBank = (x.bankLeft && std::fabs(b.station - *x.bankLeft) < 1e-6) || (x.bankRight && std::fabs(b.station - *x.bankRight) < 1e-6);
        if (!atStart && !atBank) t.simple = false;
    }
    return t;
}

// Rewrites the n table as three values at the first station and the banks.
inline void setThreeN(CrossSection& x, double lob, double ch, double rob) {
    if (x.staElev.empty()) return;
    const double s0 = x.staElev.front().station;
    x.manning.clear();
    x.manning.push_back({s0, lob});
    if (x.bankLeft && *x.bankLeft > s0) x.manning.push_back({*x.bankLeft, ch});
    else x.manning.front().n = ch;
    if (x.bankRight) x.manning.push_back({*x.bankRight, rob});
}

}  // namespace deras::ui
