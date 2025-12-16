/**
 * @file types.cpp
 * @brief Реализация базовых типов
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "types.hpp"
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace incline::model {

Color Color::fromHex(const std::string& hex) {
    std::string s = hex;

    // Убрать # если есть
    if (!s.empty() && s[0] == '#') {
        s = s.substr(1);
    }

    if (s.length() != 6 && s.length() != 8) {
        throw std::invalid_argument("Некорректный формат цвета: " + hex);
    }

    auto parseHex = [](const std::string& str, size_t pos) -> uint8_t {
        unsigned int val;
        std::stringstream ss;
        ss << std::hex << str.substr(pos, 2);
        ss >> val;
        return static_cast<uint8_t>(val);
    };

    Color color;
    color.r = parseHex(s, 0);
    color.g = parseHex(s, 2);
    color.b = parseHex(s, 4);
    color.a = (s.length() == 8) ? parseHex(s, 6) : 255;

    return color;
}

std::string Color::toHex() const {
    std::ostringstream ss;
    ss << '#' << std::hex << std::uppercase << std::setfill('0')
       << std::setw(2) << static_cast<int>(r)
       << std::setw(2) << static_cast<int>(g)
       << std::setw(2) << static_cast<int>(b);
    if (a != 255) {
        ss << std::setw(2) << static_cast<int>(a);
    }
    return ss.str();
}

} // namespace incline::model
