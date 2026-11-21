// De-RAS - io/RasText.h
// Low-level helpers for HEC-RAS text files: byte-exact line storage,
// fixed-width numeric fields and "Key=Value" lines.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace deras::io {

namespace fs = std::filesystem;

// Convert between UTF-8 strings (used across De-RAS) and filesystem paths.
fs::path toPath(const std::string& utf8);
std::string fromPath(const fs::path& p);

// A text file kept as raw lines (bytes, no decoding) so that unchanged
// content can be written back byte for byte.
struct TextDoc {
    std::vector<std::string> lines;
    std::string eol = "\r\n";   // detected from the file; CRLF for new files (HEC-RAS style)
    bool finalEol = true;

    static bool load(const fs::path& path, TextDoc& out, std::string& error);
    static TextDoc fromString(const std::string& bytes);
    std::string toString() const;
    bool save(const fs::path& path, std::string& error) const;  // atomic: write temp then rename
};

bool readFile(const fs::path& path, std::string& bytes);
bool writeFileAtomic(const fs::path& path, const std::string& bytes, std::string& error);

// ---- strings ----
bool startsWith(const std::string& s, const std::string& prefix);
std::string trim(const std::string& s);
std::string rtrim(const std::string& s);
std::vector<std::string> split(const std::string& s, char sep);
std::string padRight(const std::string& s, size_t width);

// If line starts with key (e.g. "Geom Title="), returns the remainder.
std::optional<std::string> valueOf(const std::string& line, const std::string& key);

// ---- numbers ----
// Parses a number written by HEC-RAS (".06", "-12.3", " 1E+05"). Blank -> nullopt.
std::optional<double> parseNumber(const std::string& field);
// Formats a number to fit `width` characters, HEC-RAS style: no leading zero
// before the decimal point, trailing zeros removed, right aligned.
std::string formatFixed(double v, int width = 8);
// Shortest plain representation (used for comma separated values).
std::string formatPlain(double v);
// Splits a fixed-width data line into fields of `width` characters.
std::vector<std::string> fixedFields(const std::string& line, int width);
// Writes values as fixed-width lines with `perLine` values per line.
std::vector<std::string> fixedLines(const std::vector<double>& values, int width, int perLine);
// Number of data lines used by `count` values at `perLine` per line.
int linesFor(int count, int perLine);

}  // namespace deras::io
