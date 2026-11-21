// De-RAS - io/HdfWriter.cpp
#include "HdfWriter.h"

#include <algorithm>
#include <cstring>

namespace deras::io::hdf {

namespace {

const uint64_t kUndef = ~uint64_t(0);

void put8(std::string& b, uint8_t v) { b.push_back(static_cast<char>(v)); }
void put16(std::string& b, uint16_t v) { for (int i = 0; i < 2; ++i) put8(b, static_cast<uint8_t>(v >> (8 * i))); }
void put32(std::string& b, uint32_t v) { for (int i = 0; i < 4; ++i) put8(b, static_cast<uint8_t>(v >> (8 * i))); }
void put64(std::string& b, uint64_t v) { for (int i = 0; i < 8; ++i) put8(b, static_cast<uint8_t>(v >> (8 * i))); }
void set64(std::string& b, size_t at, uint64_t v) { for (int i = 0; i < 8; ++i) b[at + i] = static_cast<char>(v >> (8 * i)); }
void set32(std::string& b, size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) b[at + i] = static_cast<char>(v >> (8 * i)); }

size_t elemSize(Type t, size_t strLen) {
    switch (t) {
        case Type::Float32: return 4;
        case Type::Float64: return 8;
        case Type::Int32: return 4;
        case Type::String: return std::max<size_t>(strLen, 1);
    }
    return 1;
}

// Datatype message body.
std::string datatype(Type t, size_t strLen) {
    std::string b;
    switch (t) {
        case Type::Int32:
            put8(b, 0x10 | 0);            // version 1, class 0 fixed-point
            put8(b, 0x08); put8(b, 0); put8(b, 0);  // little endian, signed
            put32(b, 4);
            put16(b, 0); put16(b, 32);    // bit offset, precision
            break;
        case Type::Float32:
            put8(b, 0x10 | 1);
            put8(b, 0x20); put8(b, 31); put8(b, 0);  // LE, implied mantissa msb, sign bit 31
            put32(b, 4);
            put16(b, 0); put16(b, 32);
            put8(b, 23); put8(b, 8); put8(b, 0); put8(b, 23);
            put32(b, 127);
            break;
        case Type::Float64:
            put8(b, 0x10 | 1);
            put8(b, 0x20); put8(b, 63); put8(b, 0);
            put32(b, 8);
            put16(b, 0); put16(b, 64);
            put8(b, 52); put8(b, 11); put8(b, 0); put8(b, 52);
            put32(b, 1023);
            break;
        case Type::String:
            put8(b, 0x10 | 3);
            put8(b, 0x00); put8(b, 0); put8(b, 0);  // null-terminated, ASCII
            put32(b, static_cast<uint32_t>(std::max<size_t>(strLen, 1)));
            break;
    }
    return b;
}

std::string dataspace(const std::vector<uint64_t>& dims) {
    std::string b;
    put8(b, 2);                                   // version 2
    put8(b, static_cast<uint8_t>(dims.size()));
    put8(b, 0);                                   // no max dims
    put8(b, dims.empty() ? 0 : 1);                // scalar / simple
    for (uint64_t d : dims) put64(b, d);
    return b;
}

uint64_t count(const std::vector<uint64_t>& dims) {
    uint64_t n = 1;
    for (uint64_t d : dims) n *= d;
    return n;
}

void appendValue(std::string& b, Type t, double v) {
    switch (t) {
        case Type::Float32: { float f = static_cast<float>(v); uint32_t u; std::memcpy(&u, &f, 4); put32(b, u); break; }
        case Type::Float64: { uint64_t u; std::memcpy(&u, &v, 8); put64(b, u); break; }
        case Type::Int32: put32(b, static_cast<uint32_t>(static_cast<int32_t>(v))); break;
        case Type::String: break;
    }
}

std::string rawData(Type t, const std::vector<double>& num, const std::vector<std::string>& str, size_t strLen, uint64_t n) {
    std::string b;
    if (t == Type::String) {
        const size_t L = std::max<size_t>(strLen, 1);
        for (uint64_t i = 0; i < n; ++i) {
            std::string s = i < str.size() ? str[i] : std::string();
            s.resize(L, '\0');
            b += s;
        }
    } else {
        for (uint64_t i = 0; i < n; ++i) appendValue(b, t, i < num.size() ? num[i] : 0.0);
    }
    return b;
}

size_t longest(const std::vector<std::string>& v) {
    size_t m = 1;
    for (const auto& s : v) m = std::max(m, s.size() + 1);
    return m;
}

std::string attributeMsg(const Attribute& a) {
    const size_t L = a.type == Type::String ? (a.strLen ? a.strLen : longest(a.str)) : 0;
    const std::string dt = datatype(a.type, L);
    const std::string ds = dataspace(a.dims);
    const uint64_t n = a.dims.empty() ? 1 : count(a.dims);
    std::string b;
    put8(b, 3);  // version 3
    put8(b, 0);
    put16(b, static_cast<uint16_t>(a.name.size() + 1));
    put16(b, static_cast<uint16_t>(dt.size()));
    put16(b, static_cast<uint16_t>(ds.size()));
    put8(b, 0);  // ASCII name
    b += a.name;
    put8(b, 0);
    b += dt;
    b += ds;
    b += rawData(a.type, a.num, a.str, L, n);
    return b;
}

struct Msg { uint8_t type; std::string body; };

// Version-2 object header with the given messages. Returns its bytes.
std::string objectHeader(const std::vector<Msg>& msgs) {
    std::string body;
    for (const auto& m : msgs) {
        put8(body, m.type);
        put16(body, static_cast<uint16_t>(m.body.size()));
        put8(body, 0);  // message flags
        body += m.body;
    }
    std::string h = "OHDR";
    put8(h, 2);           // version
    put8(h, 0x02);        // chunk#0 size stored in 4 bytes, no times, no creation order
    put32(h, static_cast<uint32_t>(body.size()));
    h += body;
    put32(h, lookup3(h.data(), h.size()));
    return h;
}

class Builder {
public:
    std::string file;

    // Writes the object header of `g` (children first) and returns its address.
    uint64_t writeGroup(const Group& g) {
        std::vector<std::pair<std::string, uint64_t>> links;
        for (const auto& c : g.groups) links.push_back({c->name, writeGroup(*c)});
        for (const auto& d : g.datasets) links.push_back({d.name, writeDataset(d)});
        std::vector<Msg> msgs;
        {
            std::string li;
            put8(li, 0); put8(li, 0);  // version, flags
            put64(li, kUndef);         // fractal heap
            put64(li, kUndef);         // name index B-tree
            msgs.push_back({0x02, li});
        }
        {
            std::string gi;
            put8(gi, 0); put8(gi, 0);
            msgs.push_back({0x0A, gi});
        }
        for (const auto& l : links) {
            std::string lm;
            put8(lm, 1);  // version
            const bool longName = l.first.size() > 255;
            put8(lm, longName ? 0x01 : 0x00);
            if (longName) put16(lm, static_cast<uint16_t>(l.first.size()));
            else put8(lm, static_cast<uint8_t>(l.first.size()));
            lm += l.first;
            put64(lm, l.second);
            msgs.push_back({0x06, lm});
        }
        for (const auto& a : g.attrs) msgs.push_back({0x0C, attributeMsg(a)});
        return append(objectHeader(msgs));
    }

private:
    uint64_t append(const std::string& bytes) {
        uint64_t at = file.size();
        file += bytes;
        return at;
    }

    uint64_t writeDataset(const Dataset& d) {
        const size_t L = d.type == Type::String ? (d.strLen ? d.strLen : longest(d.str)) : 0;
        const uint64_t n = count(d.dims);
        const std::string raw = rawData(d.type, d.num, d.str, L, n);
        const uint64_t dataAt = raw.empty() ? kUndef : append(raw);
        std::vector<Msg> msgs;
        msgs.push_back({0x01, dataspace(d.dims)});
        msgs.push_back({0x03, datatype(d.type, L)});
        {
            std::string fv;
            put8(fv, 3);
            put8(fv, 0x01 | (2 << 2));  // early allocation, write fill value if set, no fill value defined
            msgs.push_back({0x05, fv});
        }
        {
            std::string lay;
            put8(lay, 3);
            put8(lay, 1);  // contiguous
            put64(lay, dataAt);
            put64(lay, raw.size());
            msgs.push_back({0x08, lay});
        }
        for (const auto& a : d.attrs) msgs.push_back({0x0C, attributeMsg(a)});
        return append(objectHeader(msgs));
    }
};

}  // namespace

uint32_t lookup3(const void* data, size_t length, uint32_t initval) {
    const uint8_t* k = static_cast<const uint8_t*>(data);
    auto rot = [](uint32_t x, int r) { return (x << r) | (x >> (32 - r)); };
    uint32_t a, b, c;
    a = b = c = 0xdeadbeef + static_cast<uint32_t>(length) + initval;
    while (length > 12) {
        a += k[0] + (uint32_t(k[1]) << 8) + (uint32_t(k[2]) << 16) + (uint32_t(k[3]) << 24);
        b += k[4] + (uint32_t(k[5]) << 8) + (uint32_t(k[6]) << 16) + (uint32_t(k[7]) << 24);
        c += k[8] + (uint32_t(k[9]) << 8) + (uint32_t(k[10]) << 16) + (uint32_t(k[11]) << 24);
        a -= c; a ^= rot(c, 4);  c += b;
        b -= a; b ^= rot(a, 6);  a += c;
        c -= b; c ^= rot(b, 8);  b += a;
        a -= c; a ^= rot(c, 16); c += b;
        b -= a; b ^= rot(a, 19); a += c;
        c -= b; c ^= rot(b, 4);  b += a;
        length -= 12;
        k += 12;
    }
    switch (length) {
        case 12: c += uint32_t(k[11]) << 24; [[fallthrough]];
        case 11: c += uint32_t(k[10]) << 16; [[fallthrough]];
        case 10: c += uint32_t(k[9]) << 8; [[fallthrough]];
        case 9: c += k[8]; [[fallthrough]];
        case 8: b += uint32_t(k[7]) << 24; [[fallthrough]];
        case 7: b += uint32_t(k[6]) << 16; [[fallthrough]];
        case 6: b += uint32_t(k[5]) << 8; [[fallthrough]];
        case 5: b += k[4]; [[fallthrough]];
        case 4: a += uint32_t(k[3]) << 24; [[fallthrough]];
        case 3: a += uint32_t(k[2]) << 16; [[fallthrough]];
        case 2: a += uint32_t(k[1]) << 8; [[fallthrough]];
        case 1: a += k[0]; break;
        case 0: return c;
    }
    c ^= b; c -= rot(b, 14);
    a ^= c; a -= rot(c, 11);
    b ^= a; b -= rot(a, 25);
    c ^= b; c -= rot(b, 16);
    a ^= c; a -= rot(c, 4);
    b ^= a; b -= rot(a, 14);
    c ^= b; c -= rot(b, 24);
    return c;
}

Group& Group::group(const std::string& path) {
    Group* g = this;
    size_t start = 0;
    while (start <= path.size()) {
        size_t slash = path.find('/', start);
        std::string part = path.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!part.empty()) {
            Group* next = nullptr;
            for (auto& c : g->groups) if (c->name == part) next = c.get();
            if (!next) {
                g->groups.push_back(std::make_unique<Group>());
                g->groups.back()->name = part;
                next = g->groups.back().get();
            }
            g = next;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return *g;
}

Dataset& Group::addFloat32(const std::string& name, std::vector<uint64_t> dims, std::vector<double> values) {
    datasets.push_back({name, Type::Float32, std::move(dims), std::move(values), {}, 0, {}});
    return datasets.back();
}
Dataset& Group::addFloat64(const std::string& name, std::vector<uint64_t> dims, std::vector<double> values) {
    datasets.push_back({name, Type::Float64, std::move(dims), std::move(values), {}, 0, {}});
    return datasets.back();
}
Dataset& Group::addInt32(const std::string& name, std::vector<uint64_t> dims, std::vector<double> values) {
    datasets.push_back({name, Type::Int32, std::move(dims), std::move(values), {}, 0, {}});
    return datasets.back();
}
Dataset& Group::addStrings(const std::string& name, std::vector<std::string> values, size_t fixedLen) {
    const uint64_t n = values.size();
    datasets.push_back({name, Type::String, {n}, {}, std::move(values), fixedLen, {}});
    return datasets.back();
}
void Group::attr(const std::string& name, const std::string& value) {
    Attribute a;
    a.name = name;
    a.type = Type::String;
    a.str = {value};
    attrs.push_back(a);
}
void Group::attr(const std::string& name, double value, Type t) {
    Attribute a;
    a.name = name;
    a.type = t;
    a.num = {value};
    attrs.push_back(a);
}

std::string build(const Group& root) {
    Builder b;
    // superblock v2 (48 bytes), addresses filled in at the end
    std::string& f = b.file;
    f = std::string("\x89HDF\r\n\x1a\n", 8);
    put8(f, 2);   // version
    put8(f, 8);   // size of offsets
    put8(f, 8);   // size of lengths
    put8(f, 0);   // consistency flags
    put64(f, 0);       // base address
    put64(f, kUndef);  // superblock extension
    put64(f, 0);       // end of file (later)
    put64(f, 0);       // root object header (later)
    put32(f, 0);       // checksum (later)
    const uint64_t rootAddr = b.writeGroup(root);
    set64(f, 28, f.size());
    set64(f, 36, rootAddr);
    set32(f, 44, lookup3(f.data(), 44));
    return f;
}

bool write(const fs::path& path, const Group& root, std::string& error) {
    return writeFileAtomic(path, build(root), error);
}

}  // namespace deras::io::hdf
