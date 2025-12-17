/**
 * @file file_utils.hpp
 * @brief Вспомогательные функции для работы с файлами
 */

#pragma once

#include <filesystem>
#include <string>

namespace incline::io {

/**
 * @brief Атомарная запись строковых данных в файл (через временный файл + rename).
 */
void atomicWrite(const std::filesystem::path& path, const std::string& content);

} // namespace incline::io
