// De-RAS - core/Json.cpp
#include "Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace deras::json {

Value& Value::operator[](const std::string& key) {
    if (type_ != Type::Object) { type_ = Type::Object; obj_.clear(); }
    for (auto& kv : obj_)
        if (kv.first == key) return kv.second;
    obj_.emplace_back(key, Value());
    return obj_.back().second;
}

const Value& Value::get(const std::string& key) const {
    static const Value n;
    for (const auto& kv : obj_)
        if (kv.first == key) return kv.second;
    return n;
}

bool Value::has(const std::string& key) const {
    for (const auto& kv : obj_)
        if (kv.first == key) return true;
    return false;
}

static void escapeTo(std::string& out, const std::string& s) {
    out += '"';
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char b[8];
                    std::snprintf(b, sizeof b, "\\u%04x", c);
                    out += b;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

static void numberTo(std::string& out, double d) {
    if (!std::isfinite(d)) { out += "null"; return; }
    char b[40];
    if (d == std::floor(d) && std::fabs(d) < 1e15) std::snprintf(b, sizeof b, "%.0f", d);
    else {
        // shortest form that reads back to the same value (2.65, not 2.6499999999999999)
        for (int prec = 15; prec <= 17; ++prec) {
            std::snprintf(b, sizeof b, "%.*g", prec, d);
            if (std::strtod(b, nullptr) == d) break;
        }
    }
    out += b;
}

void Value::dumpTo(std::string& out, int indent, int level) const {
    auto nl = [&](int lv) {
        if (indent <= 0) return;
        out += '\n';
        out.append(static_cast<size_t>(indent * lv), ' ');
    };
    switch (type_) {
        case Type::Null: out += "null"; break;
        case Type::Bool: out += b_ ? "true" : "false"; break;
        case Type::Number: numberTo(out, d_); break;
        case Type::String: escapeTo(out, s_); break;
        case Type::Array: {
            // arrays of numbers stay on one line (time series stay readable and small)
            bool numeric = !arr_.empty();
            for (const auto& x : arr_) numeric = numeric && x.type_ == Type::Number;
            if (numeric) {
                out += '[';
                for (size_t i = 0; i < arr_.size(); ++i) {
                    if (i) out += ',';
                    numberTo(out, arr_[i].d_);
                }
                out += ']';
                break;
            }
            out += '[';
            for (size_t i = 0; i < arr_.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                arr_[i].dumpTo(out, indent, level + 1);
            }
            if (!arr_.empty()) nl(level);
            out += ']';
            break;
        }
        case Type::Object:
            out += '{';
            for (size_t i = 0; i < obj_.size(); ++i) {
                if (i) out += ',';
                nl(level + 1);
                escapeTo(out, obj_[i].first);
                out += indent > 0 ? ": " : ":";
                obj_[i].second.dumpTo(out, indent, level + 1);
            }
            if (!obj_.empty()) nl(level);
            out += '}';
            break;
    }
}

std::string Value::dump(int indent) const {
    std::string out;
    dumpTo(out, indent, 0);
    return out;
}

namespace {
struct Parser {
    const std::string& t;
    size_t p = 0;
    std::string err;

    void ws() { while (p < t.size() && (t[p] == ' ' || t[p] == '\n' || t[p] == '\r' || t[p] == '\t')) ++p; }
    bool fail(const char* m) { if (err.empty()) err = std::string(m) + " at offset " + std::to_string(p); return false; }

    static void utf8(std::string& o, unsigned cp) {
        if (cp < 0x80) o += static_cast<char>(cp);
        else if (cp < 0x800) { o += static_cast<char>(0xC0 | (cp >> 6)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { o += static_cast<char>(0xE0 | (cp >> 12)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
        else { o += static_cast<char>(0xF0 | (cp >> 18)); o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F)); o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F)); o += static_cast<char>(0x80 | (cp & 0x3F)); }
    }

    bool str(std::string& o) {
        if (t[p] != '"') return fail("expected string");
        ++p;
        while (p < t.size() && t[p] != '"') {
            char c = t[p++];
            if (c == '\\') {
                if (p >= t.size()) return fail("bad escape");
                char e = t[p++];
                switch (e) {
                    case '"': o += '"'; break;
                    case '\\': o += '\\'; break;
                    case '/': o += '/'; break;
                    case 'b': o += '\b'; break;
                    case 'f': o += '\f'; break;
                    case 'n': o += '\n'; break;
                    case 'r': o += '\r'; break;
                    case 't': o += '\t'; break;
                    case 'u': {
                        if (p + 4 > t.size()) return fail("bad unicode escape");
                        unsigned cp = static_cast<unsigned>(std::strtoul(t.substr(p, 4).c_str(), nullptr, 16));
                        p += 4;
                        if (cp >= 0xD800 && cp < 0xDC00 && p + 6 <= t.size() && t[p] == '\\' && t[p + 1] == 'u') {
                            unsigned lo = static_cast<unsigned>(std::strtoul(t.substr(p + 2, 4).c_str(), nullptr, 16));
                            p += 6;
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                        }
                        utf8(o, cp);
                        break;
                    }
                    default: return fail("bad escape");
                }
            } else {
                o += c;
            }
        }
        if (p >= t.size()) return fail("unterminated string");
        ++p;
        return true;
    }

    bool val(Value& v, int depth) {
        if (depth > 200) return fail("nesting too deep");
        ws();
        if (p >= t.size()) return fail("unexpected end");
        char c = t[p];
        if (c == '{') {
            ++p;
            v = Value::object();
            ws();
            if (p < t.size() && t[p] == '}') { ++p; return true; }
            while (true) {
                ws();
                std::string k;
                if (p >= t.size() || !str(k)) return fail("expected key");
                ws();
                if (p >= t.size() || t[p] != ':') return fail("expected ':'");
                ++p;
                Value item;
                if (!val(item, depth + 1)) return false;
                v[k] = std::move(item);
                ws();
                if (p < t.size() && t[p] == ',') { ++p; continue; }
                if (p < t.size() && t[p] == '}') { ++p; return true; }
                return fail("expected ',' or '}'");
            }
        }
        if (c == '[') {
            ++p;
            v = Value::array();
            ws();
            if (p < t.size() && t[p] == ']') { ++p; return true; }
            while (true) {
                Value item;
                if (!val(item, depth + 1)) return false;
                v.push(std::move(item));
                ws();
                if (p < t.size() && t[p] == ',') { ++p; continue; }
                if (p < t.size() && t[p] == ']') { ++p; return true; }
                return fail("expected ',' or ']'");
            }
        }
        if (c == '"') { std::string s; if (!str(s)) return false; v = Value(std::move(s)); return true; }
        if (t.compare(p, 4, "true") == 0) { p += 4; v = Value(true); return true; }
        if (t.compare(p, 5, "false") == 0) { p += 5; v = Value(false); return true; }
        if (t.compare(p, 4, "null") == 0) { p += 4; v = Value(); return true; }
        const char* b = t.c_str() + p;
        char* e = nullptr;
        double d = std::strtod(b, &e);
        if (e == b) return fail("unexpected character");
        p += static_cast<size_t>(e - b);
        v = Value(d);
        return true;
    }
};
}  // namespace

bool parse(const std::string& text, Value& out, std::string& error) {
    Parser ps{text, 0, {}};
    if (!ps.val(out, 0)) { error = ps.err; return false; }
    ps.ws();
    if (ps.p != text.size()) { error = "trailing characters at offset " + std::to_string(ps.p); return false; }
    return true;
}

}  // namespace deras::json
