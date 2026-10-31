// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#include "sr/json.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace sr {

class JsonParser {
public:
    JsonParser(const std::string& text, std::string& error) : text_(text), error_(error) {}

    bool run(JsonValue& out) {
        skipWhitespace();
        if (!parseValue(out, 0)) return false;
        skipWhitespace();
        if (pos_ != text_.size()) return fail("trailing characters after top-level value");
        return true;
    }

private:
    static constexpr int kMaxDepth = 64;

    bool fail(const std::string& message) {
        if (error_.empty()) {
            error_ = message + " at byte " + std::to_string(pos_);
        }
        return false;
    }

    void skipWhitespace() {
        while (pos_ < text_.size()) {
            const char c = text_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool consume(char expected) {
        if (pos_ >= text_.size() || text_[pos_] != expected) {
            return fail(std::string("expected '") + expected + "'");
        }
        ++pos_;
        return true;
    }

    bool literal(const char* word) {
        const size_t length = std::strlen(word);
        if (text_.compare(pos_, length, word) != 0) return fail("invalid literal");
        pos_ += length;
        return true;
    }

    bool parseValue(JsonValue& out, int depth) {
        if (depth > kMaxDepth) return fail("nesting too deep");
        if (pos_ >= text_.size()) return fail("unexpected end of input");

        switch (text_[pos_]) {
            case '{': return parseObject(out, depth);
            case '[': return parseArray(out, depth);
            case '"':
                out.type_ = JsonValue::Type::String;
                return parseString(out.string_);
            case 't':
                out.type_ = JsonValue::Type::Bool;
                out.bool_ = true;
                return literal("true");
            case 'f':
                out.type_ = JsonValue::Type::Bool;
                out.bool_ = false;
                return literal("false");
            case 'n':
                out.type_ = JsonValue::Type::Null;
                return literal("null");
            default:
                return parseNumber(out);
        }
    }

    bool parseObject(JsonValue& out, int depth) {
        out.type_ = JsonValue::Type::Object;
        out.object_.clear();
        if (!consume('{')) return false;
        skipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == '}') {
            ++pos_;
            return true;
        }
        while (true) {
            skipWhitespace();
            std::string key;
            if (pos_ >= text_.size() || text_[pos_] != '"') return fail("expected object key");
            if (!parseString(key)) return false;
            skipWhitespace();
            if (!consume(':')) return false;
            skipWhitespace();
            JsonValue value;
            if (!parseValue(value, depth + 1)) return false;
            out.object_.emplace_back(std::move(key), std::move(value));
            skipWhitespace();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            return consume('}');
        }
    }

    bool parseArray(JsonValue& out, int depth) {
        out.type_ = JsonValue::Type::Array;
        out.array_.clear();
        if (!consume('[')) return false;
        skipWhitespace();
        if (pos_ < text_.size() && text_[pos_] == ']') {
            ++pos_;
            return true;
        }
        while (true) {
            skipWhitespace();
            JsonValue value;
            if (!parseValue(value, depth + 1)) return false;
            out.array_.push_back(std::move(value));
            skipWhitespace();
            if (pos_ < text_.size() && text_[pos_] == ',') {
                ++pos_;
                continue;
            }
            return consume(']');
        }
    }

    bool parseString(std::string& out) {
        if (!consume('"')) return false;
        out.clear();
        while (pos_ < text_.size()) {
            const char c = text_[pos_++];
            if (c == '"') return true;
            if (c != '\\') {
                out.push_back(c);
                continue;
            }
            if (pos_ >= text_.size()) return fail("unterminated escape");
            const char escape = text_[pos_++];
            switch (escape) {
                case '"': out.push_back('"'); break;
                case '\\': out.push_back('\\'); break;
                case '/': out.push_back('/'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'n': out.push_back('\n'); break;
                case 'r': out.push_back('\r'); break;
                case 't': out.push_back('\t'); break;
                case 'u': {
                    if (pos_ + 4 > text_.size()) return fail("truncated \\u escape");
                    unsigned code = 0;
                    for (int i = 0; i < 4; ++i) {
                        const char digit = text_[pos_++];
                        code <<= 4;
                        if (digit >= '0' && digit <= '9') {
                            code |= static_cast<unsigned>(digit - '0');
                        } else if (digit >= 'a' && digit <= 'f') {
                            code |= static_cast<unsigned>(digit - 'a' + 10);
                        } else if (digit >= 'A' && digit <= 'F') {
                            code |= static_cast<unsigned>(digit - 'A' + 10);
                        } else {
                            return fail("invalid \\u escape");
                        }
                    }
                    appendUtf8(out, code);
                    break;
                }
                default:
                    return fail("unknown escape sequence");
            }
        }
        return fail("unterminated string");
    }

    static void appendUtf8(std::string& out, unsigned code) {
        if (code < 0x80) {
            out.push_back(static_cast<char>(code));
        } else if (code < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (code >> 6)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xE0 | (code >> 12)));
            out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (code & 0x3F)));
        }
    }

    bool parseNumber(JsonValue& out) {
        const size_t start = pos_;
        if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
        bool anyDigit = false;
        while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
            ++pos_;
            anyDigit = true;
        }
        if (pos_ < text_.size() && text_[pos_] == '.') {
            ++pos_;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
                anyDigit = true;
            }
        }
        if (!anyDigit) return fail("invalid number");
        if (pos_ < text_.size() && (text_[pos_] == 'e' || text_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < text_.size() && (text_[pos_] == '-' || text_[pos_] == '+')) ++pos_;
            bool exponentDigit = false;
            while (pos_ < text_.size() && std::isdigit(static_cast<unsigned char>(text_[pos_]))) {
                ++pos_;
                exponentDigit = true;
            }
            if (!exponentDigit) return fail("invalid exponent");
        }
        out.type_ = JsonValue::Type::Number;
        out.number_ = std::strtod(text_.substr(start, pos_ - start).c_str(), nullptr);
        return true;
    }

    const std::string& text_;
    std::string& error_;
    size_t pos_ = 0;
};

const JsonValue* JsonValue::find(const std::string& key) const {
    if (type_ != Type::Object) return nullptr;
    for (const auto& entry : object_) {
        if (entry.first == key) return &entry.second;
    }
    return nullptr;
}

bool JsonValue::parse(const std::string& text, JsonValue& out, std::string& error) {
    error.clear();
    out = JsonValue{};
    JsonParser parser(text, error);
    return parser.run(out);
}

}  // namespace sr
