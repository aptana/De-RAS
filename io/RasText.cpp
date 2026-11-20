// De-RAS - io/RasText.cpp
#include "RasText.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <system_error>

namespace deras::io {

fs::path toPath(const std::string& utf8) {
    std::u8string u(utf8.begin(), utf8.end());
    return fs::path(u);
}

std::string fromPath(const fs::path& p) {
    std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

bool readFile(const fs::path& path, std::string& bytes) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    bytes = ss.str();
    return true;
}

bool writeFileAtomic(const fs::path& path, const std::string& bytes, std::string& error) {
    fs::path tmp = path;
    tmp += ".deras-tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) { error = "cannot create " + fromPath(tmp); return false; }
        f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        f.flush();
        if (!f) { error = "write failed for " + fromPath(tmp); return false; }
    }
    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        // Windows cannot rename over an existing file in some situations; fall back.
        fs::remove(path, ec);
        ec.clear();
        fs::rename(tmp, path, ec);
        if (ec) { error = "cannot replace " + fromPath(path) + ": " + ec.message(); return false; }
    }
    return true;
}

TextDoc TextDoc::fromString(const std::string& bytes) {
    TextDoc d;
    d.lines.clear();
    size_t crlf = 0, lf = 0;
    std::string cur;
    for (size_t i = 0; i < bytes.size(); ++i) {
        char c = bytes[i];
        if (c == '\n') {
            if (!cur.empty() && cur.back() == '\r') { cur.pop_back(); ++crlf; } else { ++lf; }
            d.lines.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    d.finalEol = cur.empty() && !bytes.empty();
    if (!cur.empty()) d.lines.push_back(cur);
    d.eol = (lf > crlf) ? "\n" : "\r\n";
    return d;
}

std::string TextDoc::toString() const {
    std::string out;
    for (size_t i = 0; i < lines.size(); ++i) {
        out += lines[i];
        if (i + 1 < lines.size() || finalEol) out += eol;
    }
    return out;
}

bool TextDoc::load(const fs::path& path, TextDoc& out, std::string& error) {
    std::string bytes;
    if (!readFile(path, bytes)) { error = fromPath(path); return false; }
    out = fromString(bytes);
    return true;
}

bool TextDoc::save(const fs::path& path, std::string& error) const {
    return writeFileAtomic(path, toString(), error);
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r");
    return s.substr(b, e - b + 1);
}

std::string rtrim(const std::string& s) {
    size_t e = s.find_last_not_of(" \t\r");
    return e == std::string::npos ? "" : s.substr(0, e + 1);
}

std::vector<std::string> split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == sep) { out.push_back(cur); cur.clear(); } else cur += c;
    }
    out.push_back(cur);
    return out;
}

std::string padRight(const std::string& s, size_t width) {
    if (s.size() >= width) return s;
    return s + std::string(width - s.size(), ' ');
}

std::optional<std::string> valueOf(const std::string& line, const std::string& key) {
    if (!startsWith(line, key)) return std::nullopt;
    return line.substr(key.size());
}

std::optional<double> parseNumber(const std::string& field) {
    std::string t = trim(field);
    if (t.empty()) return std::nullopt;
    const char* b = t.c_str();
    char* e = nullptr;
    double v = std::strtod(b, &e);
    if (e == b) return std::nullopt;
    while (*e == ' ') ++e;
    if (*e != '\0') return std::nullopt;
    if (!std::isfinite(v)) return std::nullopt;
    return v;
}

static std::string stripNumber(std::string s) {
    if (s.find('.') != std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
        while (!s.empty() && s.back() == '0') s.pop_back();
        if (!s.empty() && s.back() == '.') s.pop_back();
    }
    if (s == "-0") s = "0";
    if (startsWith(s, "0.")) s = s.substr(1);
    else if (startsWith(s, "-0.")) s = "-" + s.substr(2);
    if (s.empty()) s = "0";
    return s;
}

std::string formatFixed(double v, int width) {
    std::string best;
    for (int dec = 6; dec >= 0; --dec) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.*f", dec, v);
        std::string s = stripNumber(buf);
        if (static_cast<int>(s.size()) <= width) { best = s; break; }
    }
    if (best.empty()) {
        for (int prec = width - 5; prec >= 1; --prec) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.*E", prec, v);
            if (static_cast<int>(std::string(buf).size()) <= width) { best = buf; break; }
        }
        if (best.empty()) best = std::string(static_cast<size_t>(width), '*');
    }
    return std::string(static_cast<size_t>(width) - best.size(), ' ') + best;
}

std::string formatPlain(double v) {
    if (std::fabs(v) < 1e15 && (v == 0 || std::fabs(v) >= 1e-6)) {
        for (int dec = 0; dec <= 12; ++dec) {
            char buf[64];
            std::snprintf(buf, sizeof buf, "%.*f", dec, v);
            if (std::strtod(buf, nullptr) == v) return stripNumber(buf);
        }
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g", v);
    return buf;
}

std::vector<std::string> fixedFields(const std::string& line, int width) {
    std::vector<std::string> out;
    std::string l = rtrim(line);
    for (size_t i = 0; i < l.size(); i += static_cast<size_t>(width)) out.push_back(l.substr(i, static_cast<size_t>(width)));
    return out;
}

std::vector<std::string> fixedLines(const std::vector<double>& values, int width, int perLine) {
    std::vector<std::string> out;
    std::string cur;
    int n = 0;
    for (double v : values) {
        cur += formatFixed(v, width);
        if (++n == perLine) { out.push_back(cur); cur.clear(); n = 0; }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

int linesFor(int count, int perLine) { return count <= 0 ? 0 : (count + perLine - 1) / perLine; }

}  // namespace deras::io
