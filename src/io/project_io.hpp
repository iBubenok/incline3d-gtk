/**
 * @file project_io.hpp
 * @brief Чтение и запись файлов проекта (.inclproj)
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/project.hpp"
#include <filesystem>
#include <string>
#include <stdexcept>

namespace incline::io {

using namespace incline::model;

/// Текущая версия формата файла проекта
constexpr const char* PROJECT_FORMAT_VERSION = "1.1.0";

/// Идентификатор формата
constexpr const char* PROJECT_FORMAT_ID = "incline3d-project";

/**
 * @brief Ошибка работы с проектом
 */
class ProjectError : public std::runtime_error {
public:
    explicit ProjectError(const std::string& message)
        : std::runtime_error(message) {}
};

/**
 * @brief Загрузка проекта из файла
 *
 * Поддерживает автоматическую миграцию старых версий формата.
 *
 * @param path Путь к файлу проекта (.inclproj)
 * @return Загруженный проект
 * @throws ProjectError При ошибке чтения или парсинга
 */
[[nodiscard]] Project loadProject(const std::filesystem::path& path);

/**
 * @brief Сохранение проекта в файл
 *
 * Выполняет атомарную запись (через временный файл).
 *
 * @param project Проект для сохранения
 * @param path Путь к файлу (.inclproj)
 * @throws ProjectError При ошибке записи
 */
void saveProject(const Project& project, const std::filesystem::path& path);

/**
 * @brief Проверка, является ли файл проектом Incline3D
 */
[[nodiscard]] bool isProjectFile(const std::filesystem::path& path) noexcept;

/**
 * @brief Получить версию формата из файла проекта
 * @return Версия формата или пустая строка при ошибке
 */
[[nodiscard]] std::string getProjectVersion(const std::filesystem::path& path) noexcept;

/**
 * @brief Экспорт проекта в человекочитаемый JSON (с отступами)
 * @param project Проект
 * @param indent Размер отступа (по умолчанию 2)
 * @return JSON-строка
 */
[[nodiscard]] std::string projectToJson(const Project& project, int indent = 2);

/**
 * @brief Импорт проекта из JSON-строки
 * @param json JSON-строка
 * @return Проект
 * @throws ProjectError При ошибке парсинга
 */
[[nodiscard]] Project projectFromJson(const std::string& json);

} // namespace incline::io
