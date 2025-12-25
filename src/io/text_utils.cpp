/**
 * @file text_utils.cpp
 * @brief Утилиты для нормализации UTF-8 строк (ASCII + кириллица)
 */

#include "text_utils.hpp"
#include <cctype>

namespace incline::io {

namespace {

void appendUtf8(char32_t cp, std::string& out) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

struct DecodedRune {
    char32_t codepoint = 0;
    size_t length = 1;
};

DecodedRune decodeUtf8(std::string_view input, size_t offset) {
    DecodedRune rune{};
    if (offset >= input.size()) {
        return rune;
    }

    unsigned char c0 = static_cast<unsigned char>(input[offset]);
    if (c0 < 0x80) {
        rune.codepoint = c0;
        rune.length = 1;
        return rune;
    }

    // 2-байтовая последовательность
    if ((c0 & 0xE0) == 0xC0 && offset + 1 < input.size()) {
        unsigned char c1 = static_cast<unsigned char>(input[offset + 1]);
        if ((c1 & 0xC0) == 0x80) {
            rune.codepoint = static_cast<char32_t>(((c0 & 0x1F) << 6) | (c1 & 0x3F));
            rune.length = 2;
            return rune;
        }
    }

    // 3-байтовая последовательность
    if ((c0 & 0xF0) == 0xE0 && offset + 2 < input.size()) {
        unsigned char c1 = static_cast<unsigned char>(input[offset + 1]);
        unsigned char c2 = static_cast<unsigned char>(input[offset + 2]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80) {
            rune.codepoint = static_cast<char32_t>(((c0 & 0x0F) << 12) |
                                                   ((c1 & 0x3F) << 6) |
                                                   (c2 & 0x3F));
            rune.length = 3;
            return rune;
        }
    }

    // 4-байтовая последовательность
    if ((c0 & 0xF8) == 0xF0 && offset + 3 < input.size()) {
        unsigned char c1 = static_cast<unsigned char>(input[offset + 1]);
        unsigned char c2 = static_cast<unsigned char>(input[offset + 2]);
        unsigned char c3 = static_cast<unsigned char>(input[offset + 3]);
        if ((c1 & 0xC0) == 0x80 && (c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
            rune.codepoint = static_cast<char32_t>(((c0 & 0x07) << 18) |
                                                   ((c1 & 0x3F) << 12) |
                                                   ((c2 & 0x3F) << 6) |
                                                   (c3 & 0x3F));
            rune.length = 4;
            return rune;
        }
    }

    // Некорректная последовательность — возвращаем первый байт как есть
    rune.codepoint = c0;
    rune.length = 1;
    return rune;
}

char32_t toLowerCp(char32_t cp) {
    if (cp >= U'A' && cp <= U'Z') {
        return cp + 32;
    }
    if (cp >= 0x410 && cp <= 0x42F) { // А-Я
        return cp + 0x20;
    }
    if (cp == 0x401) { // Ё
        return 0x451;
    }
    return cp;
}

char32_t toUpperCp(char32_t cp) {
    if (cp >= U'a' && cp <= U'z') {
        return cp - 32;
    }
    if (cp >= 0x430 && cp <= 0x44F) { // а-я
        return cp - 0x20;
    }
    if (cp == 0x451) { // ё
        return 0x401;
    }
    return cp;
}

template <typename Converter>
std::string convertCase(std::string_view input, Converter fn) {
    std::string out;
    out.reserve(input.size());

    for (size_t i = 0; i < input.size();) {
        auto rune = decodeUtf8(input, i);
        char32_t converted = fn(rune.codepoint);
        appendUtf8(converted, out);
        i += rune.length;
    }

    return out;
}

} // namespace

std::string utf8ToLower(std::string_view input) {
    return convertCase(input, toLowerCp);
}

std::string utf8ToUpper(std::string_view input) {
    return convertCase(input, toUpperCp);
}

} // namespace incline::io
