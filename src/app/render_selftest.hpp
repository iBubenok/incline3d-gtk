/**
 * @file render_selftest.hpp
 * @brief Самопроверка рендеринга (экспорт изображений)
 */

#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace incline::app {

struct RenderSelfTestResult {
    bool success = false;
    std::vector<std::filesystem::path> images;
    std::string error_message;
};

/**
 * @brief Запуск самопроверки рендеринга.
 *
 * Экспортирует изображения (3D-псевдо, План, Вертикальная) в указанный каталог.
 * @return 0 при успехе, ненулевое при ошибке.
 */
int runRenderSelfTest(const std::filesystem::path& output_dir);

/**
 * @brief Расширенный вариант самопроверки с деталями и списком артефактов.
 */
RenderSelfTestResult performRenderSelfTest(const std::filesystem::path& output_dir);

} // namespace incline::app
