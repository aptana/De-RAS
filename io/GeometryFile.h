// De-RAS - io/GeometryFile.h
// Reader/writer for HEC-RAS geometry files (.g##), Stage 1 scope:
// river reaches and cross sections. Every other block (structures,
// junctions, storage/2D areas, GIS data, unknown keys) is preserved
// verbatim. Unchanged cross sections are written back byte for byte;
// edited ones are updated key by key so unknown lines inside the block stay.
#pragma once

#include <string>
#include <vector>

#include "core/Messages.h"
#include "core/Model.h"
#include "RasText.h"

namespace deras::io {

class GeometryFile {
public:
    // Load from disk. Returns false on I/O failure (msgs gets the reason).
    bool load(const fs::path& path, MessageList& msgs);
    // Parse from bytes (used by tests).
    bool parse(const std::string& bytes, const std::string& sourceName, MessageList& msgs);
    // New, empty geometry file.
    static GeometryFile createNew(const std::string& title, const std::string& programVersion);

    Geometry model;  // edit this; save() merges it into the original text

    std::string serialize() const;
    bool save(const fs::path& path, MessageList& msgs);
    bool isModified() const;

    // Generators (exposed for tests and for the flow/plan writers).
    static std::vector<std::string> crossSectionLines(const CrossSection& xs, const std::string& editedTime);
    static std::vector<std::string> reachHeaderLines(const Reach& r);

private:
    enum class Kind { Header, Raw, Reach, Node };
    struct Segment {
        Kind kind = Kind::Raw;
        std::vector<std::string> lines;
        int uid = 0;  // reach uid or node uid
    };

    void parseSegments(MessageList& msgs, const std::string& sourceName);
    static void updateNodeLines(std::vector<std::string>& lines, const CrossSection& xs, const CrossSection* orig,
                                const std::string& editedTime);
    static void updateReachLines(std::vector<std::string>& lines, const Reach& r);

    TextDoc doc_;
    std::vector<Segment> segs_;
    Geometry original_;
    bool isNew_ = false;
};

std::string rasTimestampNow();  // "Nov/19/2026 08:39:02" (HEC-RAS style, local time)

}  // namespace deras::io
