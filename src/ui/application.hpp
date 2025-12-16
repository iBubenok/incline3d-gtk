/**
 * @file application.hpp
 * @brief GTK4 приложение Incline3D
 * @author Yan Bubenok <yan@bubenok.com>
 */

#pragma once

#include <gtk/gtk.h>
#include <string>
#include <memory>

namespace incline::ui {

/**
 * @brief Главное приложение Incline3D
 *
 * Точка входа для GTK4 приложения.
 */
class Application {
public:
    Application();
    ~Application();

    // Запрет копирования
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    /**
     * @brief Запуск приложения
     * @param argc Количество аргументов командной строки
     * @param argv Массив аргументов
     * @return Код возврата
     */
    int run(int argc, char* argv[]);

    /**
     * @brief Получить GtkApplication
     */
    [[nodiscard]] GtkApplication* getGtkApp() const { return app_; }

    /**
     * @brief Идентификатор приложения
     */
    static constexpr const char* APP_ID = "com.incline3d.app";

    /**
     * @brief Название приложения
     */
    static constexpr const char* APP_NAME = "Incline3D";

    /**
     * @brief Версия приложения
     */
    static constexpr const char* APP_VERSION = "0.1.0";

private:
    static void onActivate(GtkApplication* app, gpointer user_data);
    static void onStartup(GtkApplication* app, gpointer user_data);
    static void onShutdown(GtkApplication* app, gpointer user_data);
    static void onOpen(GtkApplication* app, GFile** files, int n_files,
                       const char* hint, gpointer user_data);

    void setupActions();
    void createMainWindow();

    GtkApplication* app_{nullptr};
    GtkWidget* main_window_{nullptr};
};

} // namespace incline::ui
