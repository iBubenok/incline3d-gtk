/**
 * @file main.cpp
 * @brief Точка входа приложения Incline3D
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "ui/application.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    try {
        incline::ui::Application app;
        return app.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Критическая ошибка: " << e.what() << std::endl;
        return 1;
    }
}
