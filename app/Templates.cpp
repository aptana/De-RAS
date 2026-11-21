// De-RAS - app/Templates.cpp
#include "Templates.h"

#include <cmath>
#include <cstdio>

#include "io/RasText.h"

namespace deras::app {

CrossSection trapezoidXs(const PrismaticSpec& s, double invert, const std::string& rs, double len) {
    CrossSection xs;
    xs.uid = nextUid();
    xs.rs = rs;
    const double fp = s.floodplainWidth;
    const double top = s.sideSlope * s.bankHeight;
    const double x0 = 0;
    const double bl = x0 + fp;                     // left bank station
    const double t1 = bl + top;                    // toe left
    const double t2 = t1 + s.bottomWidth;          // toe right
    const double br = t2 + top;                    // right bank
    const double xe = br + fp;
    const double bankEl = invert + s.bankHeight;
    if (fp > 0) xs.staElev.push_back({x0, bankEl + s.floodplainRise});
    xs.staElev.push_back({bl, bankEl});
    xs.staElev.push_back({t1, invert});
    xs.staElev.push_back({t2, invert});
    xs.staElev.push_back({br, bankEl});
    if (fp > 0) xs.staElev.push_back({xe, bankEl + s.floodplainRise});
    xs.bankLeft = bl;
    xs.bankRight = br;
    if (fp > 0) {
        xs.manning = {{x0, s.nOverbank}, {bl, s.nChannel}, {br, s.nOverbank}};
    } else {
        xs.manning = {{bl, s.nChannel}};
    }
    xs.lenLeft = xs.lenChannel = xs.lenRight = len;
    xs.contraction = 0.1;
    xs.expansion = 0.3;
    return xs;
}

void addPrismaticChannel(io::Project& prj, const PrismaticSpec& s, bool exampleLabel) {
    const std::string tag = exampleLabel ? " (Contoh / Example)" : "";
    std::string g = prj.addGeometry("Geometri saluran prismatik" + tag);
    std::string f = prj.addSteadyFlow("Debit rencana" + tag);
    std::string p = prj.addPlan("Plan steady subkritis" + tag, exampleLabel ? "Contoh" : "Steady01", g, f);
    auto& geo = prj.geometries.at(g).model;
    Reach r;
    r.uid = nextUid();
    r.river = s.river;
    r.reach = s.reach;
    const double total = s.spacing * (s.count - 1);
    r.xy = {{0, total}, {0, 0}};  // schematic centreline (no GIS georeference)
    for (int i = 0; i < s.count; ++i) {
        double dist = total - i * s.spacing;  // distance from the downstream end
        double invert = s.downstreamInvert + s.bedSlope * dist;
        std::string rs = io::formatPlain(dist);
        double len = (i + 1 < s.count) ? s.spacing : 0;
        r.nodes.push_back(trapezoidXs(s, invert, rs, len));
    }
    geo.reaches.push_back(r);
    auto& fl = prj.steadyFlows.at(f).model;
    fl.profileNames = s.profileNames;
    FlowChange fc;
    fc.river = s.river;
    fc.reach = s.reach;
    fc.rs = r.nodes.front().rs;
    fc.flows = s.flows;
    fl.changes.push_back(fc);
    for (int k = 0; k < static_cast<int>(s.profileNames.size()); ++k) {
        auto& b = fl.boundaryRef(s.river, s.reach, k);
        b.downstream.type = s.dnType;
        b.downstream.slope = s.dnSlope;
        b.downstream.knownWS = s.dnKnownWS;
    }
    prj.info().currentPlan = p;
}

}  // namespace deras::app
