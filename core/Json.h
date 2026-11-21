// De-RAS - core/Json.h
// Minimal JSON value with ordered objects, a strict parser and a writer.
// Used for De-RAS sidecar files (*.deras.json, run results, settings) so the
// core and engines do not depend on Qt.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace deras::json {

class Value {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Value() = default;
    Value(std::nullptr_t) {}
    Value(bool b) : type_(Type::Bool), b_(b) {}
    Value(double d) : type_(Type::Number), d_(d) {}
    Value(int i) : type_(Type::Number), d_(i) {}
    Value(long long i) : type_(Type::Number), d_(static_cast<double>(i)) {}
    Value(const char* s) : type_(Type::String), s_(s) {}
    Value(std::string s) : type_(Type::String), s_(std::move(s)) {}

    static Value array() { Value v; v.type_ = Type::Array; return v; }
    static Value object() { Value v; v.type_ = Type::Object; return v; }

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isObject() const { return type_ == Type::Object; }
    bool isArray() const { return type_ == Type::Array; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }

    double num(double def = 0) const { return type_ == Type::Number ? d_ : def; }
    bool boolean(bool def = false) const { return type_ == Type::Bool ? b_ : def; }
    const std::string& str() const { static const std::string e; return type_ == Type::String ? s_ : e; }

    // Array
    void push(Value v) { type_ = Type::Array; arr_.push_back(std::move(v)); }
    size_t size() const { return type_ == Type::Array ? arr_.size() : (type_ == Type::Object ? obj_.size() : 0); }
    const Value& at(size_t i) const { static const Value n; return i < arr_.size() ? arr_[i] : n; }
    const std::vector<Value>& items() const { return arr_; }

    // Object (insertion ordered)
    Value& operator[](const std::string& key);
    const Value& get(const std::string& key) const;
    bool has(const std::string& key) const;
    const std::vector<std::pair<std::string, Value>>& members() const { return obj_; }

    std::string dump(int indent = 2) const;

private:
    void dumpTo(std::string& out, int indent, int level) const;
    Type type_ = Type::Null;
    bool b_ = false;
    double d_ = 0;
    std::string s_;
    std::vector<Value> arr_;
    std::vector<std::pair<std::string, Value>> obj_;
};

// Parses text; on failure returns false and sets error (with offset).
bool parse(const std::string& text, Value& out, std::string& error);

}  // namespace deras::json
