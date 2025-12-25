/**
 * @file text_utils.hpp
 * @brief Утилиты для нормализации UTF-8 строк (ASCII + кириллица)
 */

#pragma once

#include <string>
#include <string_view>

namespace incline::io {

/**
 * @brief Перевод строки в нижний регистр (ASCII + базовая кириллица)
 */
[[nodiscard]] std::string utf8ToLower(std::string_view input);

/**
 * @brief Перевод строки в верхний регистр (ASCII + базовая кириллица)
 */
[[nodiscard]] std::string utf8ToUpper(std::string_view input);

} // namespace incline::io
