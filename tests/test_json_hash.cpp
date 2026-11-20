// JSON parser/writer and SHA-256 known-answer tests.
#include "TestMain.h"
#include "core/Hash.h"
#include "core/Json.h"
using namespace deras;

TEST(sha256_known_answers) {
    CHECK(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(sha256Hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
          "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}
TEST(json_roundtrip) {
    json::Value v = json::Value::object();
    v["a"] = 1.5;
    v["b"] = "teks \"kutip\" \n baris";
    json::Value arr = json::Value::array();
    arr.push(1); arr.push(true); arr.push(nullptr);
    v["c"] = arr;
    std::string s = v.dump(2);
    json::Value w; std::string err;
    CHECK(json::parse(s, w, err));
    CHECK(w.get("a").num() == 1.5);
    CHECK(w.get("b").str() == "teks \"kutip\" \n baris");
    CHECK(w.get("c").size() == 3);
    CHECK(w.get("c").at(1).boolean());
}
TEST(json_errors) {
    json::Value w; std::string err;
    CHECK(!json::parse("{\"a\":}", w, err));
    CHECK(!err.empty());
    CHECK(!json::parse("[1,2", w, err));
    CHECK(json::parse("\"\\u00e9\"", w, err));
    CHECK(w.str() == "\xc3\xa9");
}
