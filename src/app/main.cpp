/**
 * @file main.cpp
 * @brief Точка входа приложения Incline3D
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "ui/application.hpp"
#include "render_selftest.hpp"
#include <iostream>
#include <filesystem>

int main(int argc, char* argv[]) {
    try {
        // Режим самопроверки рендеринга: --render-selftest [путь]
        if (argc >= 2 && std::string_view(argv[1]) == "--render-selftest") {
            std::filesystem::path out_dir = (argc >= 3) ? std::filesystem::path(argv[2]) : std::filesystem::path("render-selftest");
            int code = incline::app::runRenderSelfTest(out_dir);
            if (code != 0) {
                std::cerr << "Самопроверка рендеринга завершилась с ошибкой" << std::endl;
            }
            return code;
        }

        incline::ui::Application app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        return 1;
    }
}
