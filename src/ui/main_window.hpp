/**
 * @file main_window.hpp
 * @brief Главное окно приложения Incline3D
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include "model/project.hpp"
#include "core/processing.hpp"
#include <gtk/gtk.h>
#include <memory>
#include <string>

namespace incline::ui {

// Forward declarations для внутренних типов
struct MainWindowPrivate;

} // namespace incline::ui

G_BEGIN_DECLS

#define INCLINE_TYPE_MAIN_WINDOW (incline_main_window_get_type())
G_DECLARE_FINAL_TYPE(InclineMainWindow, incline_main_window, INCLINE, MAIN_WINDOW, GtkApplicationWindow)

/**
 * @brief Создать главное окно
 */
GtkWidget* main_window_new(GtkApplication* app);

/**
 * @brief Открыть файл проекта
 */
void main_window_open_file(InclineMainWindow* window, const char* path);

/**
 * @brief Сохранить текущий проект
 */
gboolean main_window_save_project(InclineMainWindow* window);

/**
 * @brief Импортировать данные
 */
void main_window_import_data(InclineMainWindow* window, const char* path);

/**
 * @brief Обработать выбранные скважины
 */
void main_window_process_selected(InclineMainWindow* window);

/**
 * @brief Получить текущий проект (для внешнего доступа)
 */
incline::model::Project* main_window_get_project(InclineMainWindow* window);

/**
 * @brief Обновить визуализацию
 */
void main_window_update_views(InclineMainWindow* window);

G_END_DECLS
