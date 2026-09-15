#pragma once
#include <cmath>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace engine::assets::detail {
struct JsonValue {
    enum class Type { Null, Bool, String, Number, Array, Object } type = Type::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    std::vector<std::string> keys;
    std::vector<JsonValue> items;

    bool isNull() const { return type == Type::Null; }
    int size() const { return static_cast<int>(items.size()); }
    const JsonValue* at(int index) const { return (index >= 0 && index < size()) ? &items[index] : nullptr; }
    const JsonValue* obj(const std::string& key) const {
        if (type != Type::Object) {
            return nullptr;
        }
        for (int i = 0; i < static_cast<int>(keys.size()); ++i) {
            if (keys[static_cast<std::size_t>(i)] == key) {
                return &items[static_cast<std::size_t>(i)];
            }
        }
        return nullptr;
    }
    int asInt(int fallback) const {
        return type == Type::Number && number >= std::numeric_limits<int>::min() &&
                       number <= std::numeric_limits<int>::max() && std::trunc(number) == number
                   ? static_cast<int>(number)
                   : fallback;
    }
    float asFloat(float fallback) const { return type == Type::Number ? static_cast<float>(number) : fallback; }
};

JsonValue parseJson(std::string_view text);
} // namespace engine::assets::detail
