// Fixed-width number formatting/parsing and byte-exact text storage.
#include "TestMain.h"
#include "io/RasText.h"
using namespace deras::io;

TEST(format_fixed) {
    CHECK(formatFixed(0) == "       0");
    CHECK(formatFixed(0.06) == "     .06");
    CHECK(formatFixed(100.5) == "   100.5");
    CHECK(formatFixed(-12.34) == "  -12.34");
    CHECK(formatFixed(-0.5) == "     -.5");
    CHECK(formatFixed(1234.5678) == "1234.568");
    CHECK(formatFixed(12345678) == "12345678");
    CHECK(formatFixed(0.035) == "    .035");
    CHECK(formatFixed(1, 16) == "               1");
    CHECK(formatFixed(123456789.0).size() == 8);
}
TEST(parse_number) {
    CHECK(parseNumber("     .06").value() == 0.06);
    CHECK(parseNumber("  -12.34").value() == -12.34);
    CHECK(!parseNumber("        ").has_value());
    CHECK(!parseNumber("abc").has_value());
    CHECK(parseNumber("1E+05").value() == 1e5);
}
TEST(fixed_fields_roundtrip) {
    std::vector<double> v{0, 106, 20, 104.5, 30, 102, 35, 100.5, 45, 100.5, 50, 102};
    auto lines = fixedLines(v, 8, 10);
    CHECK(lines.size() == 2);
    CHECK(lines[0] == "       0     106      20   104.5      30     102      35   100.5      45   100.5");
    std::vector<double> back;
    for (auto& l : lines) for (auto& f : fixedFields(l, 8)) back.push_back(*parseNumber(f));
    CHECK(back == v);
}
TEST(textdoc_bytes) {
    std::string a = "A=1\r\nB=2\r\n";
    CHECK(TextDoc::fromString(a).toString() == a);
    std::string b = "A=1\nB=2";
    CHECK(TextDoc::fromString(b).toString() == b);
    std::string c = "A=1\r\n\r\nB=\r\n";
    CHECK(TextDoc::fromString(c).toString() == c);
    CHECK(TextDoc::fromString(a).eol == "\r\n");
}
TEST(format_plain) {
    CHECK(formatPlain(0.3) == ".3");
    CHECK(formatPlain(250) == "250");
    CHECK(formatPlain(1000.25) == "1000.25");
    CHECK(formatPlain(0.0012) == ".0012");
}
