// De-RAS - app/Report.cpp
#include "app/Report.h"

#include <cstdint>
#include <sstream>

#include "Version.h"
#include "core/Units.h"

namespace deras::app {

namespace {
std::string esc(const std::string& s) {
    std::string o;
    o.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '&': o += "&amp;"; break;
            case '<': o += "&lt;"; break;
            case '>': o += "&gt;"; break;
            case '"': o += "&quot;"; break;
            default: o += c;
        }
    }
    return o;
}
ReportBlock para(ReportBlock::Kind k, std::string t) {
    ReportBlock b;
    b.kind = k;
    b.text = std::move(t);
    return b;
}
}  // namespace

std::vector<ReportBlock> runReportBlocks(const RunResult& r, Language lang, const std::string& projectTitle, const std::string& planTitle) {
    const bool id = lang == Language::ID;
    const auto u = r.meta.units;
    const bool unsteady = r.meta.method.find("unsteady") != std::string::npos;
    std::vector<ReportBlock> out;
    out.push_back(para(ReportBlock::Kind::Title, id ? "Laporan Run De-RAS" : "De-RAS Run Report"));
    out.push_back(para(ReportBlock::Kind::Subtitle, projectTitle + " · " + planTitle));
    out.push_back(para(ReportBlock::Kind::Heading, id ? "Ringkasan" : "Summary"));
    ReportBlock t;
    t.kind = ReportBlock::Kind::Table;
    t.leftAligned = {true, true};
    t.rows.push_back({id ? "Butir" : "Item", id ? "Nilai" : "Value"});
    auto row = [&](const std::string& k, const std::string& v) { t.rows.push_back({k, v}); };
    row("Status", runStatusLabel(r.status, lang));
    row(id ? "Asal data" : "Data origin", originLabel(r.origin, lang));
    row(id ? "Metode" : "Method", unsteady ? (id ? "Skema implisit Preissmann, aliran unsteady 1D (Pratinjau)" : "Preissmann implicit scheme, 1D unsteady flow (Preview)")
                                           : (id ? "Standard step, aliran steady 1D" : "Standard step, 1D steady flow"));
    row(id ? "Versi solver" : "Solver version", r.meta.solverName + " " + r.meta.solverVersion);
    row(id ? "Mulai / selesai (UTC)" : "Started / finished (UTC)", r.meta.startedUtc + " / " + r.meta.finishedUtc);
    row(id ? "Satuan" : "Units", u == UnitSystem::SI ? "SI (m, m3/s)" : "US Customary (ft, cfs)");
    row(id ? "File geometri (SHA-256)" : "Geometry file (SHA-256)", r.meta.geomFile + "  " + r.meta.geomSha256);
    row(id ? "File aliran (SHA-256)" : "Flow file (SHA-256)", r.meta.flowFile + "  " + r.meta.flowSha256);
    row(id ? "File plan (SHA-256)" : "Plan file (SHA-256)", r.meta.planFile + "  " + r.meta.planSha256);
    out.push_back(t);
    out.push_back(para(ReportBlock::Kind::Heading, id ? "Status validasi" : "Validation status"));
    out.push_back(para(ReportBlock::Kind::Paragraph,
                       id ? "Metode ini lolos uji unit dan benchmark analitis De-RAS. Kesetaraan numerik dengan HEC-RAS belum divalidasi; jangan anggap hasil ini sama dengan HEC-RAS."
                          : "This method passes the De-RAS unit tests and analytical benchmarks. Numerical equivalence with HEC-RAS has not been validated; do not treat these results as identical to HEC-RAS."));
    out.push_back(para(ReportBlock::Kind::Heading, id ? "Peringatan dan kesalahan" : "Warnings and errors"));
    int n = 0;
    for (const auto& m : r.messages) {
        if (m.severity == Severity::Info) continue;
        out.push_back(para(ReportBlock::Kind::Paragraph, std::string(severityLabel(m.severity, lang)) + ": " + m.text(lang)));
        ++n;
    }
    if (n == 0) out.push_back(para(ReportBlock::Kind::Paragraph, id ? "Tidak ada." : "None."));
    if (r.status == RunStatus::Succeeded) {
        const std::string L = lengthUnit(u), Q = flowUnit(u), V = velocityUnit(u), A = areaUnit(u);
        size_t shown = 0;
        for (const auto& p : r.profiles) {
            if (unsteady && shown >= 1) break;   // unsteady: the Max WS profile only
            ++shown;
            out.push_back(para(ReportBlock::Kind::Heading, (id ? "Hasil profil " : "Results, profile ") + p.name));
            ReportBlock tb;
            tb.kind = ReportBlock::Kind::Table;
            tb.leftAligned = {true, false, false, false, false, false, false, false, false, false};
            tb.rows.push_back({"River / Reach", "RS", "Q (" + Q + ")", "Min Ch El (" + L + ")", "W.S. (" + L + ")", "E.G. (" + L + ")", "E.G. Slope",
                               "Vel Chnl (" + V + ")", "Area (" + A + ")", "Froude"});
            for (const auto& x : p.xs)
                tb.rows.push_back({x.river + " / " + x.reach, x.rs + (x.critical ? " *" : ""), fmt(x.q, 2), fmt(x.minChEl, 2), fmt(x.ws, 3), fmt(x.eg, 3),
                                   fmt(x.egSlope, 6), fmt(x.velCh, 3), fmt(x.area, 2), fmt(x.froudeCh, 3)});
            out.push_back(tb);
        }
        if (unsteady)
            out.push_back(para(ReportBlock::Kind::Note, id ? "Profil per waktu keluaran dan time series ada di file hasil De-RAS (.deras-results.json) dan bisa diekspor ke CSV."
                                                           : "Profiles per output time and time series are in the De-RAS results file (.deras-results.json) and can be exported to CSV."));
        else
            out.push_back(para(ReportBlock::Kind::Note, id ? "* muka air ditetapkan pada kedalaman kritis" : "* water surface set to critical depth"));
    } else {
        out.push_back(para(ReportBlock::Kind::Paragraph, id ? "Run tidak berhasil, sehingga tidak ada tabel hasil." : "The run did not succeed, so there is no results table."));
    }
    out.push_back(para(ReportBlock::Kind::Note, std::string(id ? "Dibuat oleh De-RAS " : "Generated by De-RAS ") + DERAS_VERSION +
                                                   (id ? ". De-RAS tidak berafiliasi dengan HEC atau USACE." : ". De-RAS is not affiliated with HEC or USACE.")));
    return out;
}

// ---------------- HTML for QTextDocument ----------------
std::string reportBlocksHtml(const std::vector<ReportBlock>& blocks, const std::string& logo) {
    std::ostringstream o;
    o << "<html><body style=\"font-family:'Segoe UI',Arial,sans-serif;font-size:9pt;color:#1A2433\">";
    for (const auto& b : blocks) {
        switch (b.kind) {
            case ReportBlock::Kind::Title:
                o << "<table cellpadding=\"0\" cellspacing=\"0\"><tr>";
                if (!logo.empty()) o << "<td><img src=\"" << esc(logo) << "\" width=\"42\" height=\"42\"></td><td width=\"10\"></td>";
                o << "<td><h1 style=\"color:#01215F;font-size:16pt;margin:0\">" << esc(b.text) << "</h1></td></tr></table>";
                break;
            case ReportBlock::Kind::Subtitle: o << "<p style=\"color:#5B6B82\">" << esc(b.text) << "</p>"; break;
            case ReportBlock::Kind::Heading: o << "<h2 style=\"color:#01215F;font-size:12pt;margin-top:14px\">" << esc(b.text) << "</h2>"; break;
            case ReportBlock::Kind::Paragraph: o << "<p>" << esc(b.text) << "</p>"; break;
            case ReportBlock::Kind::Note: o << "<p style=\"color:#5B6B82;font-size:8pt\">" << esc(b.text) << "</p>"; break;
            case ReportBlock::Kind::Table: {
                o << "<table border=\"1\" cellspacing=\"0\" cellpadding=\"3\" style=\"border-color:#B8C4D6;font-size:7.5pt\">";
                for (size_t i = 0; i < b.rows.size(); ++i) {
                    o << "<tr>";
                    for (size_t c = 0; c < b.rows[i].size(); ++c) {
                        const bool left = c < b.leftAligned.size() && b.leftAligned[c];
                        o << (i == 0 ? "<th bgcolor=\"#EEF3FA\"" : "<td") << " align=\"" << (left ? "left" : "right") << "\">" << esc(b.rows[i][c])
                          << (i == 0 ? "</th>" : "</td>");
                    }
                    o << "</tr>";
                }
                o << "</table>";
                break;
            }
        }
    }
    o << "</body></html>";
    return o.str();
}

// ---------------- zip (stored) ----------------
unsigned long crc32(const std::string& bytes) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int k = 0; k < 8; ++k) c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[i] = c;
        }
        init = true;
    }
    uint32_t c = 0xFFFFFFFFu;
    for (unsigned char ch : bytes) c = table[(c ^ ch) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFFu;
}

namespace {
void le16(std::string& s, unsigned v) { s += char(v & 0xFF); s += char((v >> 8) & 0xFF); }
void le32(std::string& s, unsigned long v) { for (int i = 0; i < 4; ++i) s += char((v >> (8 * i)) & 0xFF); }

std::string zipStored(const std::vector<std::pair<std::string, std::string>>& files) {
    std::string out, central;
    for (const auto& [name, data] : files) {
        const unsigned long crc = crc32(data);
        const unsigned long off = out.size();
        // local header
        le32(out, 0x04034b50);
        le16(out, 20); le16(out, 0x0800); le16(out, 0);  // version, flags (UTF-8 names), stored
        le16(out, 0); le16(out, 0x21);                   // time, date (1980-01-01)
        le32(out, crc); le32(out, data.size()); le32(out, data.size());
        le16(out, unsigned(name.size())); le16(out, 0);
        out += name;
        out += data;
        // central directory entry
        le32(central, 0x02014b50);
        le16(central, 20); le16(central, 20); le16(central, 0x0800); le16(central, 0);
        le16(central, 0); le16(central, 0x21);
        le32(central, crc); le32(central, data.size()); le32(central, data.size());
        le16(central, unsigned(name.size())); le16(central, 0); le16(central, 0); le16(central, 0); le16(central, 0);
        le32(central, 0); le32(central, off);
        central += name;
    }
    const unsigned long cdOff = out.size();
    out += central;
    le32(out, 0x06054b50);
    le16(out, 0); le16(out, 0);
    le16(out, unsigned(files.size())); le16(out, unsigned(files.size()));
    le32(out, central.size()); le32(out, cdOff);
    le16(out, 0);
    return out;
}

std::string run(const std::string& text, const char* props = nullptr) {
    std::string o = "<w:r>";
    if (props) o += std::string("<w:rPr>") + props + "</w:rPr>";
    o += "<w:t xml:space=\"preserve\">" + esc(text) + "</w:t></w:r>";
    return o;
}
}  // namespace

std::string docxBytes(const std::vector<ReportBlock>& blocks) {
    std::ostringstream d;
    d << "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
      << "<w:document xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\"><w:body>";
    for (const auto& b : blocks) {
        switch (b.kind) {
            case ReportBlock::Kind::Title: d << "<w:p><w:pPr><w:pStyle w:val=\"Title\"/></w:pPr>" << run(b.text) << "</w:p>"; break;
            case ReportBlock::Kind::Subtitle: d << "<w:p><w:pPr><w:pStyle w:val=\"Subtitle\"/></w:pPr>" << run(b.text) << "</w:p>"; break;
            case ReportBlock::Kind::Heading: d << "<w:p><w:pPr><w:pStyle w:val=\"Heading1\"/></w:pPr>" << run(b.text) << "</w:p>"; break;
            case ReportBlock::Kind::Paragraph: d << "<w:p>" << run(b.text) << "</w:p>"; break;
            case ReportBlock::Kind::Note: d << "<w:p>" << run(b.text, "<w:color w:val=\"5B6B82\"/><w:sz w:val=\"16\"/>") << "</w:p>"; break;
            case ReportBlock::Kind::Table: {
                d << "<w:tbl><w:tblPr><w:tblStyle w:val=\"TableGrid\"/><w:tblW w:w=\"0\" w:type=\"auto\"/></w:tblPr>";
                for (size_t i = 0; i < b.rows.size(); ++i) {
                    d << (i == 0 ? "<w:tr><w:trPr><w:tblHeader/></w:trPr>" : "<w:tr>");
                    for (size_t c = 0; c < b.rows[i].size(); ++c) {
                        const bool left = c < b.leftAligned.size() && b.leftAligned[c];
                        d << "<w:tc><w:tcPr>" << (i == 0 ? "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"EEF3FA\"/>" : "") << "</w:tcPr><w:p><w:pPr><w:jc w:val=\""
                          << (left ? "left" : "right") << "\"/><w:spacing w:before=\"0\" w:after=\"0\"/></w:pPr>"
                          << run(b.rows[i][c], i == 0 ? "<w:b/><w:sz w:val=\"16\"/>" : "<w:sz w:val=\"16\"/>") << "</w:p></w:tc>";
                    }
                    d << "</w:tr>";
                }
                d << "</w:tbl><w:p/>";
                break;
            }
        }
    }
    d << "<w:sectPr><w:pgSz w:w=\"16838\" w:h=\"11906\" w:orient=\"landscape\"/><w:pgMar w:top=\"1000\" w:right=\"1000\" w:bottom=\"1000\" w:left=\"1000\" w:header=\"500\" w:footer=\"500\" w:gutter=\"0\"/></w:sectPr>"
      << "</w:body></w:document>";
    const std::string styles =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<w:styles xmlns:w=\"http://schemas.openxmlformats.org/wordprocessingml/2006/main\">"
        "<w:docDefaults><w:rPrDefault><w:rPr><w:rFonts w:ascii=\"Segoe UI\" w:hAnsi=\"Segoe UI\" w:cs=\"Segoe UI\"/><w:sz w:val=\"20\"/><w:color w:val=\"1A2433\"/></w:rPr></w:rPrDefault>"
        "<w:pPrDefault><w:pPr><w:spacing w:after=\"120\"/></w:pPr></w:pPrDefault></w:docDefaults>"
        "<w:style w:type=\"paragraph\" w:default=\"1\" w:styleId=\"Normal\"><w:name w:val=\"Normal\"/></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Title\"><w:name w:val=\"Title\"/><w:basedOn w:val=\"Normal\"/><w:rPr><w:b/><w:color w:val=\"01215F\"/><w:sz w:val=\"40\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Subtitle\"><w:name w:val=\"Subtitle\"/><w:basedOn w:val=\"Normal\"/><w:rPr><w:color w:val=\"5B6B82\"/></w:rPr></w:style>"
        "<w:style w:type=\"paragraph\" w:styleId=\"Heading1\"><w:name w:val=\"heading 1\"/><w:basedOn w:val=\"Normal\"/><w:pPr><w:keepNext/><w:spacing w:before=\"240\" w:after=\"80\"/><w:outlineLvl w:val=\"0\"/></w:pPr><w:rPr><w:b/><w:color w:val=\"01215F\"/><w:sz w:val=\"26\"/></w:rPr></w:style>"
        "<w:style w:type=\"table\" w:styleId=\"TableGrid\"><w:name w:val=\"Table Grid\"/><w:tblPr><w:tblBorders>"
        "<w:top w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C4D6\"/><w:left w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C4D6\"/>"
        "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C4D6\"/><w:right w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C4D6\"/>"
        "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C4D6\"/><w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"B8C4D6\"/>"
        "</w:tblBorders><w:tblCellMar><w:left w:w=\"80\" w:type=\"dxa\"/><w:right w:w=\"80\" w:type=\"dxa\"/></w:tblCellMar></w:tblPr></w:style>"
        "</w:styles>";
    const std::string types =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
        "<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>"
        "<Default Extension=\"xml\" ContentType=\"application/xml\"/>"
        "<Override PartName=\"/word/document.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml\"/>"
        "<Override PartName=\"/word/styles.xml\" ContentType=\"application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml\"/>"
        "</Types>";
    const std::string rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"word/document.xml\"/>"
        "</Relationships>";
    const std::string docRels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
        "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles\" Target=\"styles.xml\"/>"
        "</Relationships>";
    return zipStored({{"[Content_Types].xml", types}, {"_rels/.rels", rels}, {"word/document.xml", d.str()}, {"word/_rels/document.xml.rels", docRels},
                      {"word/styles.xml", styles}});
}

bool writeDocx(const io::fs::path& path, const std::vector<ReportBlock>& blocks, std::string& error) {
    return io::writeFileAtomic(path, docxBytes(blocks), error);
}

}  // namespace deras::app
