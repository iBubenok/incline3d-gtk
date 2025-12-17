/**
 * @file render_selftest.hpp
 * @brief Самопроверка рендеринга (экспорт изображений)
 */

#pragma once

#include <filesystem>

namespace incline::app {

/**
 * @brief Запуск самопроверки рендеринга.
 *
 * Экспортирует изображения (3D-псевдо, План, Вертикальная) в указанный каталог.
 * @return 0 при успехе, ненулевое при ошибке.
 */
int runRenderSelfTest(const std::filesystem::path& output_dir);

} // namespace incline::app
