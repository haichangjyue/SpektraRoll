// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 SpektraRoll contributors
#pragma once

#include <string>
#include <utility>
#include <vector>

namespace sr {

// A very small JSON reader, sufficient for the spektrafilm profile files.
// Objects preserve key order, numbers are double, `null` is its own type
// (profiles use it for "no measurement at this wavelength").
class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    Type type() const { return type_; }
    bool isNull() const { return type_ == Type::Null; }
    bool isNumber() const { return type_ == Type::Number; }
    bool isString() const { return type_ == Type::String; }
    bool isArray() const { return type_ == Type::Array; }
    bool isObject() const { return type_ == Type::Object; }

    double number(double fallback = 0.0) const { return isNumber() ? number_ : fallback; }
    bool boolean(bool fallback = false) const { return type_ == Type::Bool ? bool_ : fallback; }
    const std::string& string() const { return string_; }

    const std::vector<JsonValue>& items() const { return array_; }
    size_t size() const { return array_.size(); }

    // Object lookup; returns nullptr when the key is missing (or on non-objects).
    const JsonValue* find(const std::string& key) const;

    // Parses `text`. On failure returns false and fills `error` with a message
    // that includes the byte offset.
    static bool parse(const std::string& text, JsonValue& out, std::string& error);

private:
    friend class JsonParser;

    Type type_ = Type::Null;
    bool bool_ = false;
    double number_ = 0.0;
    std::string string_;
    std::vector<JsonValue> array_;
    std::vector<std::pair<std::string, JsonValue>> object_;
};

}  // namespace sr
