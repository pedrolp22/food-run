#include "Json.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace engine::assets::detail {
namespace {
class JsonParser {
  public:
    explicit JsonParser(const std::string& input) : input_(input) {}

    bool parse(JsonValue& out) {
        skipWhitespace();
        if (!parseValue(out)) {
            return false;
        }
        skipWhitespace();
        return pos_ == input_.size();
    }

  private:
    const std::string& input_;
    std::size_t pos_ = 0;
    int depth_ = 0;

    char peek() const { return pos_ < input_.size() ? input_[pos_] : '\0'; }
    char next() { return pos_ < input_.size() ? input_[pos_++] : '\0'; }
    void skipWhitespace() {
        while (pos_ < input_.size() && std::isspace(static_cast<unsigned char>(input_[pos_]))) {
            ++pos_;
        }
    }

    bool parseValue(JsonValue& out) {
        if (++depth_ > 128) {
            return false;
        }
        struct DepthGuard {
            int& depth;
            ~DepthGuard() { --depth; }
        } guard{depth_};
        skipWhitespace();
        const char c = peek();
        if (c == '{') {
            return parseObject(out);
        }
        if (c == '[') {
            return parseArray(out);
        }
        if (c == '"') {
            out.type = JsonValue::Type::Null;
            if (!parseString(out.string)) {
                return false;
            }
            out.type = JsonValue::Type::String;
            return true;
        }
        if (c == 't' || c == 'f') {
            out.type = JsonValue::Type::Bool;
            std::string literal;
            while (pos_ < input_.size() && std::isalpha(static_cast<unsigned char>(input_[pos_]))) {
                literal += next();
            }
            if (literal == "true") {
                out.boolean = true;
                return true;
            }
            if (literal == "false") {
                out.boolean = false;
                return true;
            }
            return false;
        }
        if (c == 'n') {
            std::string literal;
            while (pos_ < input_.size() && std::isalpha(static_cast<unsigned char>(input_[pos_]))) {
                literal += next();
            }
            if (literal == "null") {
                out.type = JsonValue::Type::Null;
                return true;
            }
            return false;
        }
        return parseNumber(out);
    }

    bool parseObject(JsonValue& out) {
        out.type = JsonValue::Type::Object;
        next();
        skipWhitespace();
        if (peek() == '}') {
            next();
            return true;
        }
        for (;;) {
            skipWhitespace();
            if (peek() != '"') {
                return false;
            }
            std::string key;
            if (!parseString(key)) {
                return false;
            }
            skipWhitespace();
            if (next() != ':') {
                return false;
            }
            JsonValue value;
            if (!parseValue(value)) {
                return false;
            }
            out.keys.push_back(key);
            out.items.push_back(value);
            skipWhitespace();
            const char c = next();
            if (c == ',') {
                continue;
            }
            if (c == '}') {
                return true;
            }
            return false;
        }
    }

    bool parseArray(JsonValue& out) {
        out.type = JsonValue::Type::Array;
        next();
        skipWhitespace();
        if (peek() == ']') {
            next();
            return true;
        }
        for (;;) {
            JsonValue value;
            if (!parseValue(value)) {
                return false;
            }
            out.items.push_back(value);
            skipWhitespace();
            const char c = next();
            if (c == ',') {
                continue;
            }
            if (c == ']') {
                return true;
            }
            return false;
        }
    }

    bool parseString(std::string& out) {
        if (next() != '"') {
            return false;
        }
        out.clear();
        for (;;) {
            const char c = next();
            if (c == '\0') {
                return false;
            }
            if (c == '"') {
                return true;
            }
            if (static_cast<unsigned char>(c) < 0x20) {
                return false;
            }
            if (c == '\\') {
                const char e = next();
                switch (e) {
                case '"':
                    out += '"';
                    break;
                case '\\':
                    out += '\\';
                    break;
                case '/':
                    out += '/';
                    break;
                case 'b':
                    out += '\b';
                    break;
                case 'f':
                    out += '\f';
                    break;
                case 'n':
                    out += '\n';
                    break;
                case 'r':
                    out += '\r';
                    break;
                case 't':
                    out += '\t';
                    break;
                case 'u': {
                    unsigned int code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char h = next();
                        code <<= 4;
                        if (h >= '0' && h <= '9') {
                            code += static_cast<unsigned int>(h - '0');
                        } else if (h >= 'a' && h <= 'f') {
                            code += static_cast<unsigned int>(h - 'a' + 10);
                        } else if (h >= 'A' && h <= 'F') {
                            code += static_cast<unsigned int>(h - 'A' + 10);
                        } else {
                            return false;
                        }
                    }
                    if (code < 0x80) {
                        out += static_cast<char>(code);
                    } else if (code < 0x800) {
                        out += static_cast<char>(0xC0 | (code >> 6));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    } else {
                        out += static_cast<char>(0xE0 | (code >> 12));
                        out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                        out += static_cast<char>(0x80 | (code & 0x3F));
                    }
                    break;
                }
                default:
                    return false;
                }
                continue;
            }
            out += c;
        }
    }

    bool parseNumber(JsonValue& out) {
        const auto start = pos_;
        if (peek() == '-') {
            next();
        }
        if (peek() == '0') {
            next();
        } else {
            if (peek() < '1' || peek() > '9') {
                return false;
            }
            while (peek() >= '0' && peek() <= '9') {
                next();
            }
        }
        if (peek() == '.') {
            next();
            if (peek() < '0' || peek() > '9') {
                return false;
            }
            while (peek() >= '0' && peek() <= '9') {
                next();
            }
        }
        if (peek() == 'e' || peek() == 'E') {
            next();
            if (peek() == '+' || peek() == '-') {
                next();
            }
            if (peek() < '0' || peek() > '9') {
                return false;
            }
            while (peek() >= '0' && peek() <= '9') {
                next();
            }
        }
        const auto token = input_.substr(start, pos_ - start);
        out.number = std::strtod(token.c_str(), nullptr);
        if (!std::isfinite(out.number)) {
            return false;
        }
        out.type = JsonValue::Type::Number;
        return true;
    }
};

} // namespace
JsonValue parseJson(std::string_view text) {
    const std::string input(text);
    JsonValue root;
    if (!JsonParser(input).parse(root)) {
        throw std::runtime_error("Invalid glTF JSON");
    }
    return root;
}
} // namespace engine::assets::detail
