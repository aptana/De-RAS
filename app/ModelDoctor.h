// De-RAS - app/ModelDoctor.h
// Pre-run validation ("Model Doctor", F7). Produces actionable bilingual
// messages; a run is refused while any Error remains.
#pragma once

#include "core/Messages.h"
#include "core/Model.h"

namespace deras::app {

MessageList checkCrossSection(const CrossSection& xs, const std::string& location, bool isLastInReach);
MessageList checkGeometry(const Geometry& g, UnitSystem units);
MessageList checkSteadyFlow(const SteadyFlow& f, const Geometry& g, FlowRegime regime = FlowRegime::Subcritical);
MessageList checkPlan(const Plan& p, bool geomExists, bool flowExists);

// Everything needed for a steady run of one plan.
MessageList checkSteadyRun(const Plan& p, const Geometry* g, const SteadyFlow* f, UnitSystem units);

// Unsteady flow data against the geometry, and everything for an unsteady run (M5).
MessageList checkUnsteadyFlow(const UnsteadyFlow& f, const Geometry& g, const Plan* p = nullptr);
MessageList checkUnsteadyRun(const Plan& p, const Geometry* g, const UnsteadyFlow* f, UnitSystem units);

}  // namespace deras::app
