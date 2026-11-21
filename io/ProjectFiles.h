// De-RAS - io/ProjectFiles.h
// Readers/writers for the HEC-RAS project (.prj), steady flow (.f##) and
// plan (.p##) text files. Unknown lines are preserved; files that were not
// changed are written back byte for byte.
#pragma once

#include <string>
#include <vector>

#include "core/Messages.h"
#include "core/Model.h"
#include "core/Units.h"
#include "RasText.h"

namespace deras::io {

// Edits "Key=Value" lines in place, keeping every other line untouched.
class KeyedText {
public:
    TextDoc doc;
    std::optional<std::string> get(const std::string& key) const;   // key includes '='
    bool hasLine(const std::string& exact) const;
    // Set value; if missing insert after the last line matching any anchor key
    // (or at end).
    void set(const std::string& key, const std::string& value, const std::vector<std::string>& anchors = {});
    void removeKey(const std::string& key);
    int indexOf(const std::string& key) const;
};

// ---------------- .prj ----------------
struct ProjectFileData {
    std::string title;
    std::string currentPlan;  // e.g. "p01"
    UnitSystem units = UnitSystem::USCustomary;
    std::vector<std::string> geomFiles, flowFiles, unsteadyFiles, planFiles;  // "g01", ...
    std::string description;
};

class ProjectFile {
public:
    bool load(const fs::path& path, MessageList& msgs);
    bool parse(const std::string& bytes, const std::string& src, MessageList& msgs);
    static ProjectFile createNew(const std::string& title, UnitSystem units);
    ProjectFileData data;
    std::string serialize() const;
    bool save(const fs::path& path, MessageList& msgs);
    bool isModified() const;

private:
    KeyedText text_;
    ProjectFileData original_;
    bool isNew_ = false;
};

// ---------------- .f## (steady) ----------------
class SteadyFlowFile {
public:
    bool load(const fs::path& path, MessageList& msgs);
    bool parse(const std::string& bytes, const std::string& src, MessageList& msgs);
    static SteadyFlowFile createNew(const std::string& title, const std::string& programVersion);
    SteadyFlow model;
    std::string serialize() const;
    bool save(const fs::path& path, MessageList& msgs);
    bool isModified() const;

private:
    TextDoc doc_;
    std::vector<std::string> extra_;  // unrecognised lines, original order
    std::string originalBytes_;
    bool isNew_ = false;
    std::string generate() const;
};

// ---------------- .u## (unsteady, M5) ----------------
// Boundary blocks are rewritten only when their data changed; lines of a
// block that De-RAS does not interpret stay in the block, in order.
class UnsteadyFlowFile {
public:
    bool load(const fs::path& path, MessageList& msgs);
    bool parse(const std::string& bytes, const std::string& src, MessageList& msgs);
    static UnsteadyFlowFile createNew(const std::string& title, const std::string& programVersion);
    UnsteadyFlow model;
    std::string serialize() const;
    bool save(const fs::path& path, MessageList& msgs);
    bool isModified() const;

private:
    TextDoc doc_;
    std::vector<std::string> header_;   // header lines except title, version and initial flows
    int initialAt_ = -1;                // position of the initial flow lines in header_
    std::vector<std::string> rawHeader_;               // original header lines
    std::vector<std::vector<std::string>> rawBlocks_;  // original lines of each boundary block
    UnsteadyFlow original_;                            // model as parsed
    std::string originalBytes_;
    bool isNew_ = false;
    std::string generate() const;
};

// ---------------- .p## ----------------
class PlanFile {
public:
    bool load(const fs::path& path, MessageList& msgs);
    bool parse(const std::string& bytes, const std::string& src, MessageList& msgs);
    static PlanFile createNew(const Plan& p, UnitSystem units);
    Plan model;
    std::string serialize() const;
    bool save(const fs::path& path, MessageList& msgs);
    bool isModified() const;

private:
    void unsteadyLines(KeyedText& t) const;
    KeyedText text_;
    Plan original_;
    bool isNew_ = false;
};

bool samePlan(const Plan& a, const Plan& b);
bool sameFlow(const SteadyFlow& a, const SteadyFlow& b);

}  // namespace deras::io
