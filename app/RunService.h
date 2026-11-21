// De-RAS - app/RunService.h
// Runs one steady plan from the files on disk (as the engine process does):
// open project -> Model Doctor -> solve -> write results atomically.
// A failed or cancelled run never leaves a results table behind; it only
// records status, messages and metadata.
#pragma once

#include <functional>
#include <string>

#include "core/Results.h"
#include "io/RasText.h"

namespace deras::app {

struct RunRequest {
    io::fs::path prjPath;
    std::string planExt;      // "p01"; empty = current plan
    std::function<bool()> cancelled;
    std::function<void(int, int)> progress;
    std::function<void(const Message&)> message;  // streamed messages
    std::function<void(const std::string&)> step;  // "preprocess" | "simulate" | "write" (Compute window steps)
};

RunResult runSteadyPlan(const RunRequest& req, std::string& planExtUsed);

std::string utcNowIso();
std::string hostName();

// CSV export of a results table (UTF-8, comma separated, '.' decimal).
std::string resultsCsv(const RunResult& r, Language lang);
// Self-contained bilingual HTML run report (logo embedded as data URI if provided).
std::string runReportHtml(const RunResult& r, Language lang, const std::string& projectTitle,
                          const std::string& planTitle, const std::string& logoDataUri);

}  // namespace deras::app
