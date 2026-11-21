// De-RAS - io/GeoTiff.cpp
#include "GeoTiff.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>

namespace deras::io {

// ------------------------------------------------------------------ inflate
namespace {

struct BitIn {
    const uint8_t* p;
    size_t n, pos = 0;
    uint32_t buf = 0;
    int cnt = 0;
    bool err = false;
    int bits(int need) {
        uint32_t v = buf;
        while (cnt < need) {
            if (pos >= n) { err = true; return 0; }
            v |= uint32_t(p[pos++]) << cnt;
            cnt += 8;
        }
        buf = v >> need;
        cnt -= need;
        return int(v & ((1u << need) - 1));
    }
};

struct Huff {
    short count[16]{};
    short symbol[320]{};
};

int decodeSym(BitIn& s, const Huff& h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; ++len) {
        code |= s.bits(1);
        if (s.err) return -1;
        const int count = h.count[len];
        if (code - count < first) return h.symbol[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    return -1;
}

int buildHuff(Huff& h, const short* length, int n) {
    std::memset(h.count, 0, sizeof h.count);
    for (int s = 0; s < n; ++s) h.count[length[s]]++;
    if (h.count[0] == n) return 0;
    int left = 1;
    for (int len = 1; len < 16; ++len) {
        left <<= 1;
        left -= h.count[len];
        if (left < 0) return left;
    }
    short offs[16];
    offs[1] = 0;
    for (int len = 1; len < 15; ++len) offs[len + 1] = short(offs[len] + h.count[len]);
    for (int s = 0; s < n; ++s)
        if (length[s] != 0) h.symbol[offs[length[s]]++] = short(s);
    return left;
}

const short kLBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const short kLExt[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const short kDBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const short kDExt[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

bool codes(BitIn& s, std::vector<uint8_t>& out, const Huff& lc, const Huff& dc) {
    for (;;) {
        int sym = decodeSym(s, lc);
        if (sym < 0) return false;
        if (sym < 256) { out.push_back(uint8_t(sym)); continue; }
        if (sym == 256) return true;
        sym -= 257;
        if (sym >= 29) return false;
        const int len = kLBase[sym] + s.bits(kLExt[sym]);
        const int ds = decodeSym(s, dc);
        if (ds < 0 || ds >= 30) return false;
        const size_t dist = size_t(kDBase[ds] + s.bits(kDExt[ds]));
        if (s.err || dist > out.size()) return false;
        const size_t from = out.size() - dist;
        for (int i = 0; i < len; ++i) out.push_back(out[from + i]);
    }
}

}  // namespace

bool inflateBytes(const uint8_t* in, size_t n, std::vector<uint8_t>& out, size_t expected) {
    out.clear();
    out.reserve(expected);
    size_t start = 0;
    if (n >= 2 && (in[0] & 0x0F) == 8 && ((in[0] << 8) | in[1]) % 31 == 0) start = 2;  // zlib header
    BitIn s{in + start, n - start};
    int last;
    do {
        last = s.bits(1);
        const int type = s.bits(2);
        if (s.err) return false;
        if (type == 0) {
            s.buf = 0;
            s.cnt = 0;
            if (s.pos + 4 > s.n) return false;
            const unsigned len = s.p[s.pos] | (s.p[s.pos + 1] << 8);
            s.pos += 4;
            if (s.pos + len > s.n) return false;
            out.insert(out.end(), s.p + s.pos, s.p + s.pos + len);
            s.pos += len;
        } else if (type == 1) {
            static Huff lc, dc;
            static bool init = false;
            if (!init) {
                short l[288];
                int i = 0;
                for (; i < 144; ++i) l[i] = 8;
                for (; i < 256; ++i) l[i] = 9;
                for (; i < 280; ++i) l[i] = 7;
                for (; i < 288; ++i) l[i] = 8;
                buildHuff(lc, l, 288);
                short d[30];
                for (i = 0; i < 30; ++i) d[i] = 5;
                buildHuff(dc, d, 30);
                init = true;
            }
            if (!codes(s, out, lc, dc)) return false;
        } else if (type == 2) {
            static const short order[19] = {16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
            const int nlen = s.bits(5) + 257, ndist = s.bits(5) + 1, ncode = s.bits(4) + 4;
            if (s.err || nlen > 286 || ndist > 30) return false;
            short lengths[320]{};
            for (int i = 0; i < ncode; ++i) lengths[order[i]] = short(s.bits(3));
            Huff lencode, distcode;
            if (buildHuff(lencode, lengths, 19) != 0) return false;
            int idx = 0;
            while (idx < nlen + ndist) {
                int sym = decodeSym(s, lencode);
                if (sym < 0) return false;
                if (sym < 16) { lengths[idx++] = short(sym); continue; }
                short len = 0;
                int rep;
                if (sym == 16) {
                    if (idx == 0) return false;
                    len = lengths[idx - 1];
                    rep = 3 + s.bits(2);
                } else if (sym == 17) rep = 3 + s.bits(3);
                else rep = 11 + s.bits(7);
                if (idx + rep > nlen + ndist) return false;
                while (rep--) lengths[idx++] = len;
            }
            if (lengths[256] == 0) return false;
            const int e1 = buildHuff(lencode, lengths, nlen);
            if (e1 < 0 || (e1 > 0 && nlen - lencode.count[0] != 1)) return false;
            const int e2 = buildHuff(distcode, lengths + nlen, ndist);
            if (e2 < 0 || (e2 > 0 && ndist - distcode.count[0] != 1)) return false;
            if (!codes(s, out, lencode, distcode)) return false;
        } else {
            return false;
        }
        if (s.err) return false;
    } while (!last);
    return true;
}

// ---------------------------------------------------------------------- LZW
namespace {

bool lzwDecode(const uint8_t* in, size_t n, std::vector<uint8_t>& out, size_t expected) {
    out.clear();
    out.reserve(expected);
    std::vector<std::vector<uint8_t>> dict;
    auto reset = [&]() {
        dict.assign(258, {});
        for (int i = 0; i < 256; ++i) dict[i] = {uint8_t(i)};
    };
    reset();
    size_t bitPos = 0;
    int width = 9;
    auto read = [&]() -> int {
        if ((bitPos + width + 7) / 8 > n) return 257;
        int v = 0;
        for (int i = 0; i < width; ++i, ++bitPos) v = (v << 1) | ((in[bitPos >> 3] >> (7 - (bitPos & 7))) & 1);
        return v;
    };
    int old = -1;
    for (;;) {
        int code = read();
        if (code == 257) break;
        if (code == 256) {
            reset();
            width = 9;
            code = read();
            if (code == 257) break;
            if (code > 255) return false;
            out.insert(out.end(), dict[code].begin(), dict[code].end());
            old = code;
            continue;
        }
        if (old < 0) return false;
        std::vector<uint8_t> entry;
        if (code < int(dict.size())) {
            out.insert(out.end(), dict[code].begin(), dict[code].end());
            entry = dict[old];
            entry.push_back(dict[code][0]);
        } else if (code == int(dict.size())) {
            entry = dict[old];
            entry.push_back(dict[old][0]);
            out.insert(out.end(), entry.begin(), entry.end());
        } else {
            return false;
        }
        dict.push_back(std::move(entry));
        if (int(dict.size()) + 1 >= (1 << width) && width < 12) ++width;
        old = code;
        if (out.size() >= expected) break;
    }
    return true;
}

// ------------------------------------------------------------- TIFF reading
struct Reader {
    const std::string& b;
    bool le = true;
    uint16_t u16(size_t o) const {
        const auto* p = reinterpret_cast<const uint8_t*>(b.data()) + o;
        return le ? uint16_t(p[0] | (p[1] << 8)) : uint16_t((p[0] << 8) | p[1]);
    }
    uint32_t u32(size_t o) const {
        const auto* p = reinterpret_cast<const uint8_t*>(b.data()) + o;
        return le ? uint32_t(p[0] | (p[1] << 8) | (p[2] << 16) | (uint32_t(p[3]) << 24))
                  : uint32_t((uint32_t(p[0]) << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
    }
    uint64_t u64(size_t o) const {
        const uint64_t a = u32(o), c = u32(o + 4);
        return le ? (a | (c << 32)) : ((a << 32) | c);
    }
    double f64(size_t o) const {
        const uint64_t v = u64(o);
        double d;
        std::memcpy(&d, &v, 8);
        return d;
    }
    float f32(size_t o) const {
        const uint32_t v = u32(o);
        float f;
        std::memcpy(&f, &v, 4);
        return f;
    }
};

struct Entry {
    uint16_t tag = 0, type = 0;
    uint32_t count = 0;
    size_t valueAt = 0;  // file offset of the first value
};

int typeSize(uint16_t t) {
    switch (t) {
        case 1: case 2: case 6: case 7: return 1;
        case 3: case 8: return 2;
        case 4: case 9: case 11: return 4;
        case 5: case 10: case 12: return 8;
        default: return 0;
    }
}

}  // namespace

double Raster::sample(double x, double y) const {
    if (empty() || !contains(x, y)) return std::nan("");
    const double fx = (x - x0) / dx - 0.5, fy = (y0 - y) / dy - 0.5;
    int c0 = int(std::floor(fx)), r0 = int(std::floor(fy));
    c0 = std::clamp(c0, 0, std::max(0, nx - 2));
    r0 = std::clamp(r0, 0, std::max(0, ny - 2));
    const int c1 = std::min(c0 + 1, nx - 1), r1 = std::min(r0 + 1, ny - 1);
    const double tx = std::clamp(fx - c0, 0.0, 1.0), ty = std::clamp(fy - r0, 0.0, 1.0);
    const double a = cell(c0, r0), bb = cell(c1, r0), c = cell(c0, r1), d = cell(c1, r1);
    if (std::isnan(a) || std::isnan(bb) || std::isnan(c) || std::isnan(d)) {
        const int cn = std::clamp(int(std::floor(fx + 0.5)), 0, nx - 1), rn = std::clamp(int(std::floor(fy + 0.5)), 0, ny - 1);
        return cell(cn, rn);
    }
    return (a * (1 - tx) + bb * tx) * (1 - ty) + (c * (1 - tx) + d * tx) * ty;
}

bool parseGeoTiff(const std::string& bytes, Raster& out, std::string& error) {
    out = Raster{};
    if (bytes.size() < 8) { error = "file too small"; return false; }
    Reader rd{bytes};
    if (bytes[0] == 'I' && bytes[1] == 'I') rd.le = true;
    else if (bytes[0] == 'M' && bytes[1] == 'M') rd.le = false;
    else { error = "not a TIFF file"; return false; }
    const uint16_t magic = rd.u16(2);
    if (magic == 43) { error = "BigTIFF is not supported"; return false; }
    if (magic != 42) { error = "not a TIFF file"; return false; }
    const size_t ifd = rd.u32(4);
    if (ifd + 2 > bytes.size()) { error = "corrupt TIFF header"; return false; }
    const int n = rd.u16(ifd);
    if (ifd + 2 + size_t(n) * 12 > bytes.size()) { error = "corrupt TIFF directory"; return false; }
    std::map<uint16_t, Entry> tags;
    for (int i = 0; i < n; ++i) {
        const size_t e = ifd + 2 + size_t(i) * 12;
        Entry en{rd.u16(e), rd.u16(e + 2), rd.u32(e + 4), e + 8};
        const size_t sz = size_t(typeSize(en.type)) * en.count;
        if (sz > 4) en.valueAt = rd.u32(e + 8);
        if (typeSize(en.type) == 0 || en.valueAt + sz > bytes.size()) continue;
        tags[en.tag] = en;
    }
    auto num = [&](uint16_t tag, uint32_t i = 0, double def = 0) -> double {
        auto it = tags.find(tag);
        if (it == tags.end() || i >= it->second.count) return def;
        const Entry& e = it->second;
        const size_t o = e.valueAt + size_t(i) * typeSize(e.type);
        switch (e.type) {
            case 1: case 7: return uint8_t(bytes[o]);
            case 6: return int8_t(bytes[o]);
            case 3: return rd.u16(o);
            case 8: return int16_t(rd.u16(o));
            case 4: return rd.u32(o);
            case 9: return int32_t(rd.u32(o));
            case 11: return rd.f32(o);
            case 12: return rd.f64(o);
            case 5: return double(rd.u32(o)) / std::max(1u, rd.u32(o + 4));
            default: return def;
        }
    };
    auto count = [&](uint16_t tag) -> uint32_t { auto it = tags.find(tag); return it == tags.end() ? 0 : it->second.count; };
    auto ascii = [&](uint16_t tag) -> std::string {
        auto it = tags.find(tag);
        if (it == tags.end() || it->second.type != 2) return {};
        std::string s = bytes.substr(it->second.valueAt, it->second.count);
        while (!s.empty() && s.back() == '\0') s.pop_back();
        return s;
    };

    const int width = int(num(256)), height = int(num(257));
    const int bps = int(num(258, 0, 1)), spp = int(num(277, 0, 1)), planar = int(num(284, 0, 1));
    const int comp = int(num(259, 0, 1)), pred = int(num(317, 0, 1)), fmt = int(num(339, 0, 1));
    if (width <= 0 || height <= 0) { error = "missing image size"; return false; }
    if (comp != 1 && comp != 5 && comp != 8 && comp != 32946) { error = "compression " + std::to_string(comp) + " is not supported (use none, LZW or Deflate)"; return false; }
    if (!(bps == 8 || bps == 16 || bps == 32 || bps == 64)) { error = "unsupported bits per sample"; return false; }
    if (fmt == 3 && bps != 32 && bps != 64) { error = "unsupported float size"; return false; }
    const bool tiled = tags.count(322) && tags.count(324);
    const int bw = tiled ? int(num(322)) : width;
    const int bh = tiled ? int(num(323)) : std::min(height, int(num(278, 0, double(height))));
    if (bw <= 0 || bh <= 0) { error = "bad strip or tile size"; return false; }
    const uint16_t offTag = tiled ? 324 : 273, cntTag = tiled ? 325 : 279;
    const int across = tiled ? (width + bw - 1) / bw : 1, down = (height + bh - 1) / bh;
    const int bytesPS = bps / 8;
    const int chunkSpp = planar == 2 ? 1 : spp;  // samples per pixel inside a chunk
    const size_t rowBytes = size_t(bw) * chunkSpp * bytesPS;
    const size_t chunkBytes = rowBytes * bh;
    if (count(offTag) < uint32_t(across * down)) { error = "missing strip or tile offsets"; return false; }

    std::string nod = ascii(42113);
    double nodata = std::nan("");
    if (!nod.empty()) nodata = std::atof(nod.c_str());

    out.nx = width;
    out.ny = height;
    out.z.assign(size_t(width) * height, std::nanf(""));
    std::vector<uint8_t> buf;
    for (int ty = 0; ty < down; ++ty)
        for (int tx = 0; tx < across; ++tx) {
            const int idx = ty * across + tx;  // band 1 only (planar 2: the first across*down chunks)
            const size_t off = size_t(num(offTag, idx)), cnt = size_t(num(cntTag, idx, double(chunkBytes)));
            if (off + cnt > bytes.size()) { error = "strip or tile outside the file"; return false; }
            const auto* src = reinterpret_cast<const uint8_t*>(bytes.data()) + off;
            if (comp == 1) buf.assign(src, src + std::min(cnt, chunkBytes));
            else if (comp == 5) { if (!lzwDecode(src, cnt, buf, chunkBytes)) { error = "corrupt LZW data"; return false; } }
            else if (!inflateBytes(src, cnt, buf, chunkBytes)) { error = "corrupt Deflate data"; return false; }
            const int rows = tiled ? bh : std::min(bh, height - ty * bh);
            if (buf.size() < rowBytes * rows) buf.resize(rowBytes * bh, 0);
            for (int r = 0; r < rows; ++r) {
                uint8_t* row = buf.data() + r * rowBytes;
                if (pred == 3) {  // floating point predictor: undo byte differencing, then unshuffle
                    for (size_t i = chunkSpp; i < rowBytes; ++i) row[i] = uint8_t(row[i] + row[i - chunkSpp]);
                    std::vector<uint8_t> tmp(row, row + rowBytes);
                    const size_t nv = size_t(bw) * chunkSpp;
                    for (size_t k = 0; k < nv; ++k)
                        for (int j = 0; j < bytesPS; ++j) {
                            const uint8_t v = tmp[size_t(j) * nv + k];  // j = 0 is the most significant byte
                            row[k * bytesPS + (rd.le ? bytesPS - 1 - j : j)] = v;
                        }
                }
                auto value = [&](int c) -> double {
                    const size_t o = (size_t(c) * chunkSpp) * bytesPS;
                    std::string tmp(reinterpret_cast<const char*>(row + o), bytesPS);
                    Reader rr{tmp, rd.le};
                    if (fmt == 3) return bps == 32 ? double(rr.f32(0)) : rr.f64(0);
                    const bool sgn = fmt == 2;
                    switch (bps) {
                        case 8: return sgn ? double(int8_t(row[o])) : double(row[o]);
                        case 16: return sgn ? double(int16_t(rr.u16(0))) : double(rr.u16(0));
                        case 32: return sgn ? double(int32_t(rr.u32(0))) : double(rr.u32(0));
                        default: return sgn ? double(int64_t(rr.u64(0))) : double(rr.u64(0));
                    }
                };
                std::vector<double> vals(bw);
                for (int c = 0; c < bw; ++c) vals[c] = value(c);
                if (pred == 2 && fmt != 3) {
                    // Horizontal differencing works on the sample type modulo its size.
                    const double mod = std::ldexp(1.0, bps);
                    for (int c = 1; c < bw; ++c) {
                        double v = vals[c] + vals[c - 1];
                        if (fmt == 2) { const double h = mod / 2; while (v >= h) v -= mod; while (v < -h) v += mod; }
                        else { while (v >= mod) v -= mod; }
                        vals[c] = v;
                    }
                }
                const int gy = ty * bh + r;
                if (gy >= height) break;
                for (int c = 0; c < bw; ++c) {
                    const int gx = tx * bw + c;
                    if (gx >= width) break;
                    double v = vals[c];
                    if (!std::isnan(nodata) && (v == nodata || std::fabs(v - nodata) <= 1e-6 * std::max(1.0, std::fabs(nodata)))) v = std::nan("");
                    if (fmt == 3 && std::fabs(v) > 1e30) v = std::nan("");
                    out.z[size_t(gy) * width + gx] = float(v);
                }
            }
        }

    // Georeferencing
    bool pixelIsPoint = false;
    if (auto it = tags.find(34735); it != tags.end() && it->second.count >= 4) {
        for (uint32_t i = 0; i < it->second.count; ++i) out.geoKeys.push_back(uint16_t(num(34735, i)));
        for (uint32_t i = 0; i < count(34736); ++i) out.geoDoubles.push_back(num(34736, i));
        out.geoAscii = ascii(34737);
        const int nk = out.geoKeys[3];
        int projected = 0, geographic = 0;
        for (int k = 0; k < nk && 4 + 4 * k + 3 < int(out.geoKeys.size()); ++k) {
            const uint16_t id = out.geoKeys[4 + 4 * k], loc = out.geoKeys[5 + 4 * k], cnt = out.geoKeys[6 + 4 * k], val = out.geoKeys[7 + 4 * k];
            if (loc == 0) {
                if (id == 3072) projected = val;
                if (id == 2048) geographic = val;
                if (id == 1025) pixelIsPoint = val == 2;
            } else if (loc == 34737 && (id == 1026 || id == 3073 || id == 2049) && out.crsName.empty()) {
                std::string s = out.geoAscii.substr(std::min<size_t>(val, out.geoAscii.size()), cnt);
                while (!s.empty() && (s.back() == '|' || s.back() == '\0')) s.pop_back();
                out.crsName = s;
            }
        }
        out.epsg = projected > 0 && projected != 32767 ? projected : (geographic > 0 && geographic != 32767 ? geographic : 0);
    }
    if (count(33550) >= 2 && count(33922) >= 6) {
        out.dx = num(33550, 0);
        out.dy = num(33550, 1);
        const double i = num(33922, 0), j = num(33922, 1), x = num(33922, 3), y = num(33922, 4);
        out.x0 = x - i * out.dx;
        out.y0 = y + j * out.dy;
    } else if (count(34264) >= 16) {
        if (std::fabs(num(34264, 1)) > 1e-12 || std::fabs(num(34264, 4)) > 1e-12) { error = "rotated rasters are not supported"; return false; }
        out.dx = num(34264, 0);
        out.dy = -num(34264, 5);
        out.x0 = num(34264, 3);
        out.y0 = num(34264, 7);
    } else {
        error = "the file has no georeferencing (ModelPixelScale/ModelTiepoint)";
        return false;
    }
    if (out.dx <= 0 || out.dy <= 0) { error = "bad cell size"; return false; }
    if (pixelIsPoint) {
        out.x0 -= out.dx / 2;
        out.y0 += out.dy / 2;
    }
    return true;
}

bool readGeoTiff(const fs::path& path, Raster& out, std::string& error) {
    std::string bytes;
    if (!readFile(path, bytes)) { error = "cannot read file"; return false; }
    return parseGeoTiff(bytes, out, error);
}

// ------------------------------------------------------------- TIFF writing
std::string buildGeoTiff(const Raster& r, float nodata) {
    std::string s;
    auto put16 = [&](size_t at, uint16_t v) { s[at] = char(v & 0xFF); s[at + 1] = char(v >> 8); };
    auto put32 = [&](size_t at, uint32_t v) { for (int i = 0; i < 4; ++i) s[at + i] = char((v >> (8 * i)) & 0xFF); };
    auto app = [&](const void* p, size_t n) { s.append(static_cast<const char*>(p), n); };
    auto align = [&]() { if (s.size() & 1) s.push_back('\0'); };

    const int rowsPerStrip = std::max(1, std::min(r.ny, int(65536 / std::max<size_t>(1, size_t(r.nx) * 4))));
    const int strips = (r.ny + rowsPerStrip - 1) / rowsPerStrip;

    std::vector<uint16_t> keys = r.geoKeys;
    std::vector<double> kd = r.geoDoubles;
    std::string ka = r.geoAscii;
    if (keys.size() >= 4) {
        for (int k = 0; k < keys[3] && 4 + 4 * k + 3 < int(keys.size()); ++k)
            if (keys[4 + 4 * k] == 1025 && keys[5 + 4 * k] == 0) keys[7 + 4 * k] = 1;  // corners are written: PixelIsArea
    } else if (r.epsg > 0) {
        const bool geo = r.epsg >= 4000 && r.epsg < 5000;
        keys = {1, 1, 0, 3, 1024, 0, 1, uint16_t(geo ? 2 : 1), 1025, 0, 1, 1, uint16_t(geo ? 2048 : 3072), 0, 1, uint16_t(r.epsg)};
    }
    const std::string nodStr = [&]() { char b[32]; std::snprintf(b, sizeof b, "%g", double(nodata)); return std::string(b); }();

    struct Tag { uint16_t tag, type; uint32_t count; std::string data; };
    std::vector<Tag> tags;
    auto shortTag = [&](uint16_t t, uint16_t v) { std::string d(2, '\0'); d[0] = char(v & 0xFF); d[1] = char(v >> 8); tags.push_back({t, 3, 1, d}); };
    auto longTag = [&](uint16_t t, std::vector<uint32_t> v) {
        std::string d(v.size() * 4, '\0');
        for (size_t i = 0; i < v.size(); ++i) for (int k = 0; k < 4; ++k) d[i * 4 + k] = char((v[i] >> (8 * k)) & 0xFF);
        tags.push_back({t, 4, uint32_t(v.size()), d});
    };
    auto dblTag = [&](uint16_t t, std::vector<double> v) { std::string d(v.size() * 8, '\0'); std::memcpy(d.data(), v.data(), d.size()); tags.push_back({t, 12, uint32_t(v.size()), d}); };
    auto asciiTag = [&](uint16_t t, const std::string& v) { tags.push_back({t, 2, uint32_t(v.size() + 1), v + std::string(1, '\0')}); };

    longTag(256, {uint32_t(r.nx)});
    longTag(257, {uint32_t(r.ny)});
    shortTag(258, 32);
    shortTag(259, 1);
    shortTag(262, 1);
    longTag(273, std::vector<uint32_t>(strips, 0));  // patched below
    shortTag(277, 1);
    longTag(278, {uint32_t(rowsPerStrip)});
    std::vector<uint32_t> counts(strips);
    for (int i = 0; i < strips; ++i) counts[i] = uint32_t(std::min(rowsPerStrip, r.ny - i * rowsPerStrip)) * r.nx * 4;
    longTag(279, counts);
    shortTag(284, 1);
    shortTag(339, 3);
    dblTag(33550, {r.dx, r.dy, 0});
    dblTag(33922, {0, 0, 0, r.x0, r.y0, 0});
    if (!keys.empty()) {
        std::string d(keys.size() * 2, '\0');
        for (size_t i = 0; i < keys.size(); ++i) { d[i * 2] = char(keys[i] & 0xFF); d[i * 2 + 1] = char(keys[i] >> 8); }
        tags.push_back({34735, 3, uint32_t(keys.size()), d});
        if (!kd.empty()) dblTag(34736, kd);
        if (!ka.empty()) asciiTag(34737, ka);
    }
    asciiTag(42113, nodStr);
    std::sort(tags.begin(), tags.end(), [](const Tag& a, const Tag& b) { return a.tag < b.tag; });

    s = std::string("II*\0\0\0\0\0", 8);
    put32(4, 8);
    const size_t ifdSize = 2 + tags.size() * 12 + 4;
    s.resize(8 + ifdSize, '\0');
    put16(8, uint16_t(tags.size()));
    size_t stripOffsetsAt = 0;
    std::vector<size_t> entryAt(tags.size());
    for (size_t i = 0; i < tags.size(); ++i) {
        const size_t e = 10 + i * 12;
        entryAt[i] = e;
        put16(e, tags[i].tag);
        put16(e + 2, tags[i].type);
        put32(e + 4, tags[i].count);
        if (tags[i].data.size() <= 4) {
            for (size_t k = 0; k < tags[i].data.size(); ++k) s[e + 8 + k] = tags[i].data[k];
            if (tags[i].tag == 273) stripOffsetsAt = e + 8;
        } else {
            align();
            put32(e + 8, uint32_t(s.size()));
            if (tags[i].tag == 273) stripOffsetsAt = s.size();
            app(tags[i].data.data(), tags[i].data.size());
        }
    }
    align();
    for (int i = 0; i < strips; ++i) {
        put32(stripOffsetsAt + (strips > 1 ? i * 4 : 0), uint32_t(s.size()));
        const int r0 = i * rowsPerStrip, r1 = std::min(r.ny, r0 + rowsPerStrip);
        for (int row = r0; row < r1; ++row)
            for (int c = 0; c < r.nx; ++c) {
                float v = r.z[size_t(row) * r.nx + c];
                if (std::isnan(v)) v = nodata;
                app(&v, 4);  // little-endian host (x86/x64)
            }
    }
    return s;
}

bool writeGeoTiff(const fs::path& path, const Raster& r, std::string& error, float nodata) {
    if (r.empty()) { error = "empty raster"; return false; }
    return writeFileAtomic(path, buildGeoTiff(r, nodata), error);
}

}  // namespace deras::io
