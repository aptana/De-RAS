// De-RAS - app/ModelDoctor.cpp
#include "ModelDoctor.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace deras::app {

namespace {
std::string num(double v) {
    char b[32];
    std::snprintf(b, sizeof b, "%g", v);
    return b;
}
bool nonAscii(const std::string& s) {
    for (unsigned char c : s)
        if (c >= 0x80) return true;
    return false;
}
}  // namespace

MessageList checkCrossSection(const CrossSection& xs, const std::string& loc, bool isLast) {
    MessageList m;
    if (xs.type != NodeType::CrossSection) return m;
    if (xs.staElev.size() < 3) m.push_back(makeMsg(Severity::Error, "XS_TOO_FEW_POINTS", {}, loc));
    for (size_t i = 1; i < xs.staElev.size(); ++i)
        if (xs.staElev[i].station < xs.staElev[i - 1].station)
            m.push_back(makeMsg(Severity::Error, "XS_STATION_ORDER",
                                {std::to_string(i + 1), num(xs.staElev[i].station), num(xs.staElev[i - 1].station)}, loc));
    if (!xs.bankLeft || !xs.bankRight) {
        m.push_back(makeMsg(Severity::Error, "XS_BANKS_MISSING", {}, loc));
    } else if (!xs.staElev.empty()) {
        double s0 = xs.staElev.front().station, s1 = xs.staElev.back().station;
        if (*xs.bankLeft >= *xs.bankRight)
            m.push_back(makeMsg(Severity::Error, "XS_BANKS_ORDER", {num(*xs.bankLeft), num(*xs.bankRight)}, loc));
        if (*xs.bankLeft < s0 || *xs.bankRight > s1)
            m.push_back(makeMsg(Severity::Error, "XS_BANKS_OUTSIDE", {num(*xs.bankLeft), num(*xs.bankRight), num(s0), num(s1)}, loc));
    }
    if (xs.manning.empty()) {
        m.push_back(makeMsg(Severity::Error, "XS_NO_MANNING", {}, loc));
    } else {
        if (!xs.staElev.empty() && xs.manning.front().station > xs.staElev.front().station + 1e-9)
            m.push_back(makeMsg(Severity::Error, "XS_MANNING_START", {num(xs.manning.front().station), num(xs.staElev.front().station)}, loc));
        for (const auto& b : xs.manning) {
            if (!(b.n > 0)) m.push_back(makeMsg(Severity::Error, "XS_MANNING_NONPOS", {num(b.n)}, loc));
            else if (b.n < 0.01 || b.n > 0.25) m.push_back(makeMsg(Severity::Warning, "XS_MANNING_RANGE", {num(b.n)}, loc));
        }
    }
    if (xs.lenLeft < 0 || xs.lenChannel < 0 || xs.lenRight < 0) m.push_back(makeMsg(Severity::Error, "XS_LENGTH_NEG", {}, loc));
    else if (!isLast && xs.lenChannel == 0) m.push_back(makeMsg(Severity::Error, "XS_LENGTH_ZERO", {}, loc));
    if (xs.contraction < 0 || xs.contraction > 1 || xs.expansion < 0 || xs.expansion > 1)
        m.push_back(makeMsg(Severity::Warning, "XS_COEF_RANGE", {num(xs.contraction), num(xs.expansion)}, loc));
    for (const auto& f : xs.ignoredFeatures) m.push_back(makeMsg(Severity::Warning, "XS_FEATURE_IGNORED", {f}, loc));
    return m;
}

static bool sameRsGeom(const std::string& a, const std::string& b) {
    if (a == b) return true;
    const double x = rsNumber(a), y = rsNumber(b);
    return !std::isnan(x) && !std::isnan(y) && std::fabs(x - y) < 1e-9;
}

MessageList checkGeometry(const Geometry& g, UnitSystem units) {
    MessageList m;
    if (g.reaches.empty() && g.storageAreas.empty()) m.push_back(makeMsg(Severity::Error, "MD_NO_REACH"));
    // storage areas, 2D areas, connections, BC lines, pumps (M6)
    auto areaExists = [&](const std::string& n) { return g.findArea(n) != nullptr; };
    bool sa1 = false, sa2 = false, lat = false;
    for (const auto& a : g.storageAreas) {
        (a.is2D ? sa2 : sa1) = true;
        if (nonAscii(a.name)) m.push_back(makeMsg(Severity::Warning, "NON_ASCII_NAME", {a.name}));
        if (a.is2D) {
            if (a.polygon.size() < 3) m.push_back(makeMsg(Severity::Error, "MD_AREA_POLYGON", {a.name}, a.name));
            if (!(a.manningN > 0)) m.push_back(makeMsg(Severity::Error, "MD_AREA_MANNING", {a.name}, a.name));
        } else if (a.volElev.size() < 2 && !(a.area > 0) && a.polygon.size() < 3) {
            m.push_back(makeMsg(Severity::Error, "MD_SA_VOLUME", {a.name}, a.name));
        } else if (a.volElev.size() < 2 && !(a.area > 0)) {
            m.push_back(makeMsg(Severity::Info, "MD_SA_TERRAIN", {a.name}, a.name));
        }
    }
    for (const auto& c : g.connections) {
        if (!areaExists(c.upName) || !areaExists(c.dnName)) m.push_back(makeMsg(Severity::Error, "MD_CONN_AREA", {c.name}, c.name));
        if (c.weir.size() < 2) m.push_back(makeMsg(Severity::Error, "MD_CONN_WEIR", {c.name}, c.name));
    }
    for (const auto& b : g.bcLines) {
        const StorageArea* a = g.findArea(b.area);
        if (!a || !a->is2D) m.push_back(makeMsg(Severity::Warning, "MD_BCLINE_AREA", {b.name, b.area}, b.name));
    }
    for (const auto& p : g.pumps) {
        bool ok = !p.groups.empty();
        for (const PumpEnd* e : {&p.from, &p.to}) {
            if (e->kind == 1 && !areaExists(e->area)) ok = false;
            if (e->kind == 0) {
                const Reach* r = g.findReach(e->river, e->reach);
                bool f = false;
                if (r) for (const auto* x : r->crossSections()) f = f || sameRsGeom(x->rs, e->rs);
                ok = ok && f;
            }
        }
        if (p.from.kind == 2) ok = false;
        if (!ok) m.push_back(makeMsg(Severity::Error, "MD_PUMP_DATA", {p.name}, p.name));
    }
    for (const auto& r : g.reaches) for (const auto& n : r.nodes) lat = lat || n.type == NodeType::LateralStructure;
    if (sa1) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Storage area", "Storage area"}));
    if (sa2) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"2D flow area", "Area aliran 2D"}));
    if (lat) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Lateral structure", "Struktur lateral"}));
    if (!g.connections.empty()) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"SA/2D connection", "Koneksi SA/2D"}));
    if (!g.pumps.empty()) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Pump station", "Stasiun pompa"}));
    for (const auto& j : g.junctions)
        for (const auto* side : {&j.up, &j.dn})
            for (const auto& rr : *side)
                if (!g.findReach(rr.first, rr.second))
                    m.push_back(makeMsg(Severity::Error, "MD_JUNCTION_UNKNOWN", {j.name, rr.first + " / " + rr.second}));
    if (!g.junctions.empty())
        m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Junction", "Junction"}));
    bool ineff = false, levee = false, block = false;
    for (const auto& r : g.reaches)
        for (const auto& n : r.nodes) {
            ineff = ineff || !n.ineffective.empty();
            levee = levee || n.leveeLeft.present || n.leveeRight.present;
            block = block || !n.obstructions.empty();
        }
    if (ineff) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Ineffective flow area", "Area tak efektif"}));
    if (levee) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Levee", "Tanggul (levee)"}));
    if (block) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Blocked obstruction", "Penghalang (obstruction)"}));
    bool br = false, cu = false, il = false;
    for (const auto& r : g.reaches)
        for (const auto& n : r.nodes) {
            br = br || n.type == NodeType::Bridge;
            cu = cu || n.type == NodeType::Culvert;
            il = il || n.type == NodeType::InlineStructure;
        }
    if (br) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Bridge", "Jembatan"}));
    if (cu) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Culvert", "Gorong-gorong"}));
    if (il) m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Inline structure", "Struktur inline"}));
    for (const auto& r : g.reaches) {
        if (nonAscii(r.river)) m.push_back(makeMsg(Severity::Warning, "NON_ASCII_NAME", {r.river}));
        if (nonAscii(r.reach)) m.push_back(makeMsg(Severity::Warning, "NON_ASCII_NAME", {r.reach}));
        for (size_t k = 0; k < r.nodes.size(); ++k) {
            const auto& n = r.nodes[k];
            if (n.type == NodeType::CrossSection) continue;
            const std::string loc = r.label() + " RS " + n.rs;
            if (n.type == NodeType::LateralStructure) {
                if (!n.structure.parsed || n.structure.weir.size() < 2)
                    m.push_back(makeMsg(Severity::Error, "MD_STRUCTURE_DATA", {"Struktur lateral", n.rs, "Lateral structure", "puncak pelimpah butuh minimal 2 titik.", "the weir crest needs at least 2 points."}, loc));
                if (!n.structure.lateral.toName.empty() && !areaExists(n.structure.lateral.toName))
                    m.push_back(makeMsg(Severity::Error, "MD_LATERAL_TARGET", {n.rs, n.structure.lateral.toName}, loc));
                continue;
            }
            const bool structure = n.type == NodeType::Bridge || n.type == NodeType::Culvert || n.type == NodeType::InlineStructure;
            if (!structure) {
                m.push_back(makeMsg(Severity::Error, "GEOM_NODE_UNSUPPORTED", {std::to_string(static_cast<int>(n.type)), nodeTypeName(n.type)}, loc));
                continue;
            }
            const std::string id = n.type == NodeType::Bridge ? "Jembatan" : n.type == NodeType::Culvert ? "Gorong-gorong" : "Struktur inline";
            const std::string en = n.type == NodeType::Bridge ? "Bridge" : n.type == NodeType::Culvert ? "Culvert" : "Inline structure";
            if (!n.structure.parsed) { m.push_back(makeMsg(Severity::Error, "MD_STRUCTURE_UNREAD", {id, n.rs, en}, loc)); continue; }
            bool before = false, after = false;
            for (size_t j = 0; j < k; ++j) before = before || r.nodes[j].type == NodeType::CrossSection;
            for (size_t j = k + 1; j < r.nodes.size(); ++j) after = after || r.nodes[j].type == NodeType::CrossSection;
            if (!before || !after) m.push_back(makeMsg(Severity::Error, "MD_STRUCTURE_PLACE", {id, n.rs, en}, loc));
            auto bad = [&](const char* tid, const char* ten) { m.push_back(makeMsg(Severity::Error, "MD_STRUCTURE_DATA", {id, n.rs, en, tid, ten}, loc)); };
            const StructureData& sd = n.structure;
            if (n.type != NodeType::InlineStructure) {
                if (sd.deckUp.size() < 2) bad("dek/jalan butuh minimal 2 titik.", "the deck/road needs at least 2 points.");
                for (const auto& d : sd.deckUp)
                    if (d.low > d.high + 1e-9) { bad("low chord lebih tinggi dari high chord.", "a low chord is above its high chord."); break; }
            }
            if (n.type == NodeType::Bridge && sd.width <= 0) bad("lebar dek harus lebih dari 0.", "the deck width must be greater than 0.");
            if (n.type == NodeType::Culvert) {
                if (sd.culverts.empty()) bad("belum ada barrel gorong-gorong.", "there is no culvert barrel.");
                for (const auto& c : sd.culverts) {
                    if (c.rise <= 0 || (c.shape == 2 && c.span <= 0)) bad("ukuran barrel harus lebih dari 0.", "barrel dimensions must be greater than 0.");
                    if (c.length <= 0 || c.n <= 0) bad("panjang dan n barrel harus lebih dari 0.", "barrel length and n must be greater than 0.");
                    if (c.shape != 1 && c.shape != 2) bad("hanya barrel bulat dan kotak yang dihitung.", "only circular and box barrels are computed.");
                    if (c.upStations.empty()) bad("barrel belum punya stasiun.", "a barrel has no station.");
                }
            }
            if (n.type == NodeType::InlineStructure) {
                if (sd.weir.size() < 2) bad("puncak pelimpah butuh minimal 2 titik.", "the weir crest needs at least 2 points.");
                for (const auto& gt : sd.gates)
                    if (gt.width <= 0 || gt.height <= 0 || gt.stations.empty()) { bad("pintu butuh lebar, tinggi, dan stasiun bukaan.", "a gate needs a width, a height and opening stations."); break; }
            }
        }
        auto xs = r.crossSections();
        if (xs.size() < 2) m.push_back(makeMsg(Severity::Error, "MD_REACH_FEW_XS", {}, r.label()));
        for (size_t i = 0; i < xs.size(); ++i) {
            const std::string loc = r.label() + " RS " + xs[i]->rs;
            auto c = checkCrossSection(*xs[i], loc, i + 1 == xs.size());
            m.insert(m.end(), c.begin(), c.end());
            if (i > 0) {
                double a = xs[i - 1]->rsValue(), b = xs[i]->rsValue();
                if (std::isfinite(a) && std::isfinite(b) && b >= a)
                    m.push_back(makeMsg(Severity::Error, "XS_ORDER", {xs[i]->rs, xs[i - 1]->rs}, loc));
                // spacing guidance: reach length much larger than flow width
                if (xs[i - 1]->staElev.size() >= 2) {
                    double width = xs[i - 1]->staElev.back().station - xs[i - 1]->staElev.front().station;
                    double len = xs[i - 1]->lenChannel;
                    if (width > 0 && len > 20 * width)
                        m.push_back(makeMsg(Severity::Warning, "XS_SPACING", {num(len), lengthUnit(units)}, r.label() + " RS " + xs[i - 1]->rs));
                }
            }
        }
    }
    return m;
}

MessageList checkSteadyFlow(const SteadyFlow& f, const Geometry& g, FlowRegime regime) {
    MessageList m;
    if (f.profileCount() == 0) { m.push_back(makeMsg(Severity::Error, "FLOW_NO_PROFILES")); return m; }
    for (const auto& c : f.changes) {
        const Reach* r = g.findReach(c.river, c.reach);
        bool found = false;
        if (r)
            for (const auto* x : r->crossSections())
                if (x->rs == c.rs || (std::isfinite(x->rsValue()) && x->rsValue() == rsNumber(c.rs))) found = true;
        if (!found) m.push_back(makeMsg(Severity::Error, "FLOW_UNKNOWN_RS", {c.rs, c.river + " / " + c.reach}));
        for (int p = 0; p < f.profileCount(); ++p) {
            double q = p < static_cast<int>(c.flows.size()) ? c.flows[static_cast<size_t>(p)] : 0;
            if (!(q > 0)) m.push_back(makeMsg(Severity::Error, "FLOW_NONPOS", {f.profileNames[static_cast<size_t>(p)], c.rs, num(q)}));
        }
    }
    for (const auto& r : g.reaches) {
        auto xs = r.crossSections();
        if (xs.empty()) continue;
        bool hasUp = false;
        for (const auto& c : f.changes)
            if (c.river == r.river && c.reach == r.reach &&
                (c.rs == xs[0]->rs || rsNumber(c.rs) == xs[0]->rsValue())) hasUp = true;
        if (!hasUp) m.push_back(makeMsg(Severity::Error, "FLOW_NO_UPSTREAM", {r.label(), xs[0]->rs}));
        bool dnJunction = false, upJunction = false;
        for (const auto& j : g.junctions) {
            for (const auto& u : j.up) if (u.first == r.river && u.second == r.reach) dnJunction = true;
            for (const auto& d : j.dn) if (d.first == r.river && d.second == r.reach) upJunction = true;
        }
        for (int p = 0; p < f.profileCount(); ++p) {
            const std::string pn = f.profileNames[static_cast<size_t>(p)];
            const ReachBoundary* b = f.boundary(r.river, r.reach, p);
            if (regime != FlowRegime::Subcritical && !upJunction) {
                const Boundary* u = b ? &b->upstream : nullptr;
                if (!u || u->type == BoundaryType::None || u->type == BoundaryType::RatingCurve)
                    m.push_back(makeMsg(Severity::Error, "BC_MISSING_UP", {pn, r.label()}));
                else if (u->type == BoundaryType::NormalDepth && !(u->slope > 0))
                    m.push_back(makeMsg(Severity::Error, "BC_BAD_SLOPE", {pn}, r.label()));
            }
            if (regime == FlowRegime::Supercritical || dnJunction) continue;
            if (!b || b->downstream.type == BoundaryType::None) {
                m.push_back(makeMsg(Severity::Error, "BC_MISSING_DN", {pn, r.label()}));
                continue;
            }
            const Boundary& d = b->downstream;
            if (d.type == BoundaryType::NormalDepth && !(d.slope > 0)) m.push_back(makeMsg(Severity::Error, "BC_BAD_SLOPE", {pn}, r.label()));
            if (d.type == BoundaryType::RatingCurve) {
                bool ok = d.rating.size() >= 2;
                for (size_t k = 1; ok && k < d.rating.size(); ++k)
                    if (d.rating[k].second <= d.rating[k - 1].second) ok = false;
                if (!ok) m.push_back(makeMsg(Severity::Error, "BC_BAD_RATING", {pn}, r.label()));
            }
            if (d.type == BoundaryType::KnownWS && d.knownWS < xs.back()->channelInvert())
                m.push_back(makeMsg(Severity::Error, "BC_KNOWN_BELOW_BED", {num(d.knownWS), num(xs.back()->channelInvert()), pn}, r.label()));
        }
    }
    return m;
}

MessageList checkPlan(const Plan& p, bool geomExists, bool flowExists) {
    MessageList m;
    if (!geomExists) m.push_back(makeMsg(Severity::Error, "MD_NO_GEOMETRY"));
    if (!flowExists || p.isUnsteady()) m.push_back(makeMsg(Severity::Error, "MD_NO_FLOW"));
    if (p.regime != FlowRegime::Subcritical)
        m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {std::string("Regime ") + regimeName(p.regime, false), std::string("Rezim ") + regimeName(p.regime, true)}));
    return m;
}

MessageList checkSteadyRun(const Plan& p, const Geometry* g, const SteadyFlow* f, UnitSystem units) {
    MessageList m = checkPlan(p, g != nullptr, f != nullptr);
    if (g) { auto a = checkGeometry(*g, units); m.insert(m.end(), a.begin(), a.end()); }
    if (g && p.regime != FlowRegime::Subcritical)
        for (const auto& r : g->reaches)
            for (const auto& n : r.nodes)
                if (n.type == NodeType::Bridge || n.type == NodeType::Culvert || n.type == NodeType::InlineStructure)
                    m.push_back(makeMsg(Severity::Error, "MD_STRUCTURE_REGIME",
                                        {n.type == NodeType::Bridge ? "Jembatan" : n.type == NodeType::Culvert ? "Gorong-gorong" : "Struktur inline", n.rs,
                                         n.type == NodeType::Bridge ? "Bridge" : n.type == NodeType::Culvert ? "Culvert" : "Inline structure"},
                                        r.label() + " RS " + n.rs));
    if (g && f) { auto b = checkSteadyFlow(*f, *g, p.regime); m.insert(m.end(), b.begin(), b.end()); }
    if (g) {
        bool lat = !g->storageAreas.empty() || !g->pumps.empty();
        for (const auto& r : g->reaches) for (const auto& n : r.nodes) lat = lat || n.type == NodeType::LateralStructure;
        if (lat) m.push_back(makeMsg(Severity::Warning, "MD_LATERAL_STEADY"));
    }
    int warnings = 0;
    for (const auto& x : m) if (x.severity == Severity::Warning) ++warnings;
    if (!hasErrors(m)) m.push_back(makeMsg(Severity::Info, "MD_OK", {std::to_string(warnings)}));
    return m;
}

namespace {
const char* bcName(UBoundaryType t, bool id) {
    switch (t) {
        case UBoundaryType::FlowHydrograph: return id ? "hidrograf debit" : "flow hydrograph";
        case UBoundaryType::StageHydrograph: return id ? "hidrograf muka air" : "stage hydrograph";
        case UBoundaryType::LateralInflow: return id ? "aliran lateral" : "lateral inflow";
        case UBoundaryType::UniformLateralInflow: return id ? "aliran lateral merata" : "uniform lateral inflow";
        case UBoundaryType::NormalDepth: return "normal depth";
        case UBoundaryType::RatingCurve: return "rating curve";
        case UBoundaryType::Precipitation: return id ? "hujan" : "precipitation";
        default: return id ? "lainnya" : "other";
    }
}
bool sameRsText(const std::string& a, const std::string& b) {
    if (a == b) return true;
    const double x = rsNumber(a), y = rsNumber(b);
    return !std::isnan(x) && !std::isnan(y) && std::fabs(x - y) < 1e-9;
}
}  // namespace

MessageList checkUnsteadyFlow(const UnsteadyFlow& f, const Geometry& g, const Plan* p) {
    MessageList m;
    double simHours = 0;
    if (p) {
        const double t0 = rasDateTimeSeconds(p->simStartDate, p->simStartTime), t1 = rasDateTimeSeconds(p->simEndDate, p->simEndTime);
        if (t1 > t0) simHours = (t1 - t0) / 3600.0;
    }
    std::vector<bool> used(f.boundaries.size(), false);
    for (const auto& r : g.reaches) {
        const auto xs = r.crossSections();
        if (xs.size() < 2) continue;
        bool upJ = false, dnJ = false;
        for (const auto& j : g.junctions) {
            for (const auto& u : j.up) if (u.first == r.river && u.second == r.reach) dnJ = true;
            for (const auto& d : j.dn) if (d.first == r.river && d.second == r.reach) upJ = true;
        }
        for (int side = 0; side < 2; ++side) {
            const bool up = side == 0;
            if (up ? upJ : dnJ) continue;
            const std::string& rs = up ? xs.front()->rs : xs.back()->rs;
            const UnsteadyBoundary* b = nullptr;
            for (size_t i = 0; i < f.boundaries.size(); ++i) {
                const auto& x = f.boundaries[i];
                if (x.river == r.river && x.reach == r.reach && sameRsText(x.rs, rs) && x.type != UBoundaryType::LateralInflow &&
                    x.type != UBoundaryType::UniformLateralInflow) { b = &x; used[i] = true; break; }
            }
            if (!b || b->type == UBoundaryType::None) {
                m.push_back(makeMsg(Severity::Error, "MD_U_BC_MISSING", {r.label(), up ? "hulu" : "hilir", up ? "upstream" : "downstream"}, r.label()));
                continue;
            }
            const bool ok = up ? (b->type == UBoundaryType::FlowHydrograph || b->type == UBoundaryType::StageHydrograph)
                               : (b->type == UBoundaryType::StageHydrograph || b->type == UBoundaryType::NormalDepth || b->type == UBoundaryType::RatingCurve ||
                                  b->type == UBoundaryType::FlowHydrograph);
            if (!ok) {
                m.push_back(makeMsg(Severity::Error, "MD_U_BC_TYPE", {r.label(), bcName(b->type, true), up ? "hulu" : "hilir", bcName(b->type, false), up ? "upstream" : "downstream"}, r.label()));
                continue;
            }
            if (b->type == UBoundaryType::NormalDepth && !(b->slope > 0)) m.push_back(makeMsg(Severity::Error, "BC_BAD_SLOPE", {"-"}, r.label()));
            if (b->type == UBoundaryType::RatingCurve) {
                bool okR = b->rating.size() >= 2;
                auto rt = b->rating;
                std::sort(rt.begin(), rt.end());
                for (size_t k = 1; okR && k < rt.size(); ++k) if (rt[k].second <= rt[k - 1].second) okR = false;
                if (!okR) m.push_back(makeMsg(Severity::Error, "BC_BAD_RATING", {"-"}, r.label()));
            }
            if (b->type == UBoundaryType::StageHydrograph && up) {
                bool init = false;
                for (const auto& x : f.initialFlows) if (x.river == r.river && x.reach == r.reach) init = true;
                if (!init) m.push_back(makeMsg(Severity::Error, "MD_U_INIT", {r.label()}, r.label()));
            }
        }
        for (const auto* x : xs)
            if (x != xs.back() && !(x->lenChannel > 0)) m.push_back(makeMsg(Severity::Error, "MD_U_LENGTH", {r.label(), x->rs}, r.label() + " RS " + x->rs));
        for (const auto& n : r.nodes)
            if (n.type != NodeType::CrossSection && n.type != NodeType::LateralStructure)
                m.push_back(makeMsg(Severity::Error, "MD_U_STRUCTURE", {r.label(), n.rs}, r.label() + " RS " + n.rs));
    }
    for (size_t i = 0; i < f.boundaries.size(); ++i) {
        const auto& b = f.boundaries[i];
        const bool hydro = b.type == UBoundaryType::FlowHydrograph || b.type == UBoundaryType::StageHydrograph || b.type == UBoundaryType::Precipitation ||
                           b.type == UBoundaryType::LateralInflow || b.type == UBoundaryType::UniformLateralInflow;
        if (!b.area.empty()) {
            const StorageArea* a = g.findArea(b.area);
            bool ok = a != nullptr;
            if (ok && !b.bcLine.empty()) {
                bool found = false;
                for (const auto& l : g.bcLines) found = found || (l.name == b.bcLine && (l.area.empty() || l.area == b.area));
                ok = found && a->is2D;
            }
            if (ok && a->is2D && b.bcLine.empty() && (b.type == UBoundaryType::StageHydrograph || b.type == UBoundaryType::NormalDepth)) ok = false;
            if (ok && b.type == UBoundaryType::RatingCurve) ok = false;
            used[i] = ok;
        }
        const Reach* r = g.findReach(b.river, b.reach);
        bool locOk = false;
        if (r) for (const auto* x : r->crossSections()) if (sameRsText(x->rs, b.rs)) locOk = true;
        if (b.area.empty() && (b.type == UBoundaryType::LateralInflow || b.type == UBoundaryType::UniformLateralInflow)) used[i] = locOk;
        if (!used[i]) {
            m.push_back(makeMsg(Severity::Warning, "MD_U_BC_UNUSED", {b.area.empty() ? b.river + " / " + b.reach : b.area, b.area.empty() ? (b.rs.empty() ? "-" : b.rs) : (b.bcLine.empty() ? "-" : b.bcLine)}));
            continue;
        }
        if (hydro) {
            const double step = intervalSeconds(b.interval);
            if (b.values.empty() || !(step > 0)) {
                m.push_back(makeMsg(Severity::Error, "MD_U_BC_EMPTY", {b.area.empty() ? b.river + " / " + b.reach : b.area, b.area.empty() ? b.rs : (b.bcLine.empty() ? "-" : b.bcLine)}));
                continue;
            }
            const double hrs = step / 3600.0 * double(b.values.size() - 1);
            if (simHours > 0 && hrs + 1e-9 < simHours)
                m.push_back(makeMsg(Severity::Warning, "MD_U_BC_SHORT", {b.river + " / " + b.reach, b.rs, num(hrs), num(simHours)}));
        }
    }
    return m;
}

MessageList checkUnsteadyRun(const Plan& p, const Geometry* g, const UnsteadyFlow* f, UnitSystem units) {
    MessageList m;
    if (!g) m.push_back(makeMsg(Severity::Error, "MD_NO_GEOMETRY"));
    if (!f) m.push_back(makeMsg(Severity::Error, "MD_U_NO_FILE"));
    const double t0 = rasDateTimeSeconds(p.simStartDate, p.simStartTime), t1 = rasDateTimeSeconds(p.simEndDate, p.simEndTime);
    if (!(t1 > t0)) m.push_back(makeMsg(Severity::Error, "MD_U_TIME", {p.simStartDate + " " + p.simStartTime, p.simEndDate + " " + p.simEndTime}));
    const double dt = intervalSeconds(p.compInterval), od = intervalSeconds(p.outputInterval);
    if (!(dt > 0)) m.push_back(makeMsg(Severity::Error, "MD_U_INTERVAL", {p.compInterval}));
    if (!(od > 0) || (dt > 0 && (od < dt || std::fabs(od / dt - std::round(od / dt)) > 1e-6))) m.push_back(makeMsg(Severity::Error, "MD_U_INTERVAL", {p.outputInterval}));
    if (p.theta < 0.6 || p.theta > 1) m.push_back(makeMsg(Severity::Warning, "MD_U_THETA", {num(p.theta)}));
    m.push_back(makeMsg(Severity::Info, "MD_FEATURE_PREVIEW", {"Unsteady flow", "Aliran unsteady"}));
    if (g) { auto a = checkGeometry(*g, units); m.insert(m.end(), a.begin(), a.end()); }
    if (g && f) { auto b = checkUnsteadyFlow(*f, *g, &p); m.insert(m.end(), b.begin(), b.end()); }
    int warnings = 0;
    for (const auto& x : m) if (x.severity == Severity::Warning) ++warnings;
    if (!hasErrors(m)) m.push_back(makeMsg(Severity::Info, "MD_OK", {std::to_string(warnings)}));
    return m;
}

}  // namespace deras::app
