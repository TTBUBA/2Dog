#pragma once
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include <utility>

namespace twodog {
// Small strict JSON value; no third-party dependency. Numbers are finite doubles.
class Json {
public:
    using Array = std::vector<Json>;
    using Object = std::map<std::string, Json, std::less<>>;
    Json() = default;
    Json(std::nullptr_t) {}
    Json(bool value) : value_(value) {}
    Json(double value);
    Json(float value) : Json(static_cast<double>(value)) {}
    Json(int value) : Json(static_cast<double>(value)) {}
    Json(const char* value) : value_(std::string(value)) {}
    Json(std::string value) : value_(std::move(value)) {}
    Json(Array value) : value_(std::move(value)) {}
    Json(Object value) : value_(std::move(value)) {}
    bool null() const noexcept;
    bool isBoolean() const noexcept {return std::holds_alternative<bool>(value_);}
    bool isNumber() const noexcept {return std::holds_alternative<double>(value_);}
    bool isString() const noexcept {return std::holds_alternative<std::string>(value_);}
    bool isObject() const noexcept;
    bool isArray() const noexcept;
    bool boolean() const;
    double number() const;
    int integer() const;
    const std::string& string() const;
    const Array& array() const;
    Array& array();
    const Object& object() const;
    Object& object();
    bool contains(std::string_view key) const;
    const Json& at(std::string_view key) const;
    Json& at(std::string_view key);
    Json& operator[](std::string key);
    std::string text(std::string_view key, std::string fallback = {}) const;
    double num(std::string_view key, double fallback = 0) const;
    bool flag(std::string_view key, bool fallback = false) const;
    std::string dump() const;
    static Json parse(std::string_view source);
    static Json read(const std::filesystem::path& path);
    void save(const std::filesystem::path& path) const;
private:
    std::variant<std::nullptr_t, bool, double, std::string, Array, Object> value_{nullptr};
};
std::filesystem::path containedPath(const std::filesystem::path& root, const std::filesystem::path& relative);
}
