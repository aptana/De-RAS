// De-RAS - app/Report.h
// Run report as a list of blocks (M8), written as Word (.docx, Office Open
// XML in a stored zip, no external library) or turned into simple HTML for
// the PDF writer of the UI (QTextDocument + QPdfWriter).
#pragma once

#include <string>
#include <vector>

#include "core/Results.h"
#include "io/RasText.h"

namespace deras::app {

struct ReportBlock {
    enum class Kind { Title, Subtitle, Heading, Paragraph, Note, Table };
    Kind kind = Kind::Paragraph;
    std::string text;
    std::vector<std::vector<std::string>> rows;   // Table: first row is the header
    std::vector<bool> leftAligned;                // Table: per column
};

std::vector<ReportBlock> runReportBlocks(const RunResult& r, Language lang, const std::string& projectTitle, const std::string& planTitle);

// Word document. Returns false with `error` on failure.
bool writeDocx(const io::fs::path& path, const std::vector<ReportBlock>& blocks, std::string& error);
// The bytes of the .docx (tests).
std::string docxBytes(const std::vector<ReportBlock>& blocks);

// Simple HTML (table borders as attributes) for QTextDocument.
std::string reportBlocksHtml(const std::vector<ReportBlock>& blocks, const std::string& logoResource);

// CRC-32 (zip, IEEE 802.3).
unsigned long crc32(const std::string& bytes);

}  // namespace deras::app
