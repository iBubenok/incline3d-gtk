cmake_minimum_required(VERSION 3.20)

project(Incline3D
    VERSION 0.1.0
    DESCRIPTION "Приложение для обработки и визуализации данных инклинометрии скважин"
    LANGUAGES CXX
)

# Требуем C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Опции сборки
option(BUILD_TESTING "Собирать тесты" ON)
option(BUILD_GUI "Собирать GUI (требует GTK4)" ON)
option(ENABLE_ASAN "Включить Address Sanitizer" OFF)

# Экспортируем compile_commands.json для IDE
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# =============================================================================
# Платформо-зависимые настройки
# =============================================================================
if(WIN32)
    # Windows: UTF-8 кодировка для исходников и вывода
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
    # Минимальная версия Windows (Windows 7+)
    add_compile_definitions(_WIN32_WINNT=0x0601)
    # Исключаем редко используемые части Windows.h
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# Предупреждения компилятора
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wconversion
        -Wsign-conversion
        -Wshadow
        -Wnon-virtual-dtor
        -Wold-style-cast
        -Wcast-align
        -Wunused
        -Woverloaded-virtual
        -Wformat=2
    )
    if(ENABLE_ASAN)
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
elseif(MSVC)
    add_compile_options(
        /W4           # Высокий уровень предупреждений
        /permissive-  # Строгое соответствие стандарту
        /Zc:__cplusplus  # Корректное значение __cplusplus
        /EHsc         # Стандартная обработка исключений
    )
    # Отключаем предупреждения о "небезопасных" функциях CRT
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
endif()

# Подключение зависимостей через FetchContent
include(FetchContent)

# GLM для математики
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 0.9.9.8
)
FetchContent_MakeAvailable(glm)

# nlohmann/json для работы с JSON
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# ============================================================================
# Core библиотека (без зависимостей от GTK4/OpenGL)
# ============================================================================
add_library(incline3d_core STATIC
    # Model
    src/model/types.cpp

    # Core
    src/core/angle_utils.cpp
    src/core/trajectory.cpp
    src/core/dogleg.cpp
    src/core/errors.cpp
    src/core/processing.cpp
    src/core/analysis.cpp

    # IO
    src/io/csv_reader.cpp
    src/io/csv_writer.cpp
    src/io/las_reader.cpp
    src/io/las_writer.cpp
    src/io/project_io.cpp
    src/io/format_registry.cpp
)

target_include_directories(incline3d_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(incline3d_core PUBLIC
    glm::glm
    nlohmann_json::nlohmann_json
)

# ============================================================================
# GUI компоненты (опционально, требуют GTK4 и OpenGL)
# ============================================================================
if(BUILD_GUI)
    set(GTK4_FOUND FALSE)

    # Способ 1: vcpkg (Windows/Linux/macOS)
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(GTK4 QUIET gtk4)
    endif()

    # Способ 2: Прямой поиск через CMake CONFIG (vcpkg)
    if(NOT GTK4_FOUND)
        find_package(GTK4 CONFIG QUIET)
        if(GTK4_FOUND)
            message(STATUS "GTK4 найден через CONFIG mode (vcpkg)")
        endif()
    endif()

    # Способ 3: Windows MSYS2 - ручной путь
    if(NOT GTK4_FOUND AND WIN32)
        # Проверяем стандартные пути MSYS2
        set(MSYS2_PATHS
            "C:/msys64/mingw64"
            "C:/msys64/ucrt64"
            "C:/msys64/clang64"
            "$ENV{MSYS2_ROOT}/mingw64"
        )
        foreach(MSYS2_PATH ${MSYS2_PATHS})
            if(EXISTS "${MSYS2_PATH}/include/gtk-4.0")
                message(STATUS "GTK4 найден в MSYS2: ${MSYS2_PATH}")
                set(GTK4_FOUND TRUE)
                set(GTK4_INCLUDE_DIRS
                    "${MSYS2_PATH}/include/gtk-4.0"
                    "${MSYS2_PATH}/include/glib-2.0"
                    "${MSYS2_PATH}/lib/glib-2.0/include"
                    "${MSYS2_PATH}/include/pango-1.0"
                    "${MSYS2_PATH}/include/harfbuzz"
                    "${MSYS2_PATH}/include/cairo"
                    "${MSYS2_PATH}/include/gdk-pixbuf-2.0"
                    "${MSYS2_PATH}/include/graphene-1.0"
                    "${MSYS2_PATH}/lib/graphene-1.0/include"
                )
                set(GTK4_LIBRARY_DIRS "${MSYS2_PATH}/lib")
                set(GTK4_LIBRARIES gtk-4 glib-2.0 gobject-2.0 gio-2.0)
                break()
            endif()
        endforeach()
    endif()

    find_package(OpenGL QUIET)

    if(GTK4_FOUND AND OpenGL_FOUND)
        if(GTK4_VERSION)
            message(STATUS "GTK4 найден: ${GTK4_VERSION}")
        else()
            message(STATUS "GTK4 найден")
        endif()
        message(STATUS "OpenGL найден")

        # Полная библиотека с UI
        add_library(incline3d_lib STATIC
            # Rendering
            src/rendering/camera.cpp
            src/rendering/shader_program.cpp
            src/rendering/trajectory_renderer.cpp
            src/rendering/plan_renderer.cpp
            src/rendering/vertical_renderer.cpp

            # UI
            src/ui/application.cpp
            src/ui/main_window.cpp
        )

        target_include_directories(incline3d_lib PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/src
            ${GTK4_INCLUDE_DIRS}
        )

        if(GTK4_LIBRARY_DIRS)
            target_link_directories(incline3d_lib PUBLIC
                ${GTK4_LIBRARY_DIRS}
            )
        endif()

        target_link_libraries(incline3d_lib PUBLIC
            incline3d_core
            ${GTK4_LIBRARIES}
            OpenGL::GL
        )

        target_compile_definitions(incline3d_lib PUBLIC
            INCLINE3D_HAS_GUI=1
        )
        if(GTK4_CFLAGS_OTHER)
            target_compile_definitions(incline3d_lib PUBLIC ${GTK4_CFLAGS_OTHER})
        endif()

        # Исполняемый файл
        add_executable(incline3d
            src/app/main.cpp
        )

        # Windows: GUI-приложение (без консоли)
        if(WIN32)
            set_target_properties(incline3d PROPERTIES
                WIN32_EXECUTABLE TRUE
            )
        endif()

        target_link_libraries(incline3d PRIVATE
            incline3d_lib
        )

        # Установка
        install(TARGETS incline3d
            RUNTIME DESTINATION bin
        )

        install(DIRECTORY resources/
            DESTINATION share/incline3d
        )
    else()
        if(NOT GTK4_FOUND)
            message(WARNING "GTK4 не найден - GUI не будет собран. "
                "Для Windows установите через MSYS2: pacman -S mingw-w64-x86_64-gtk4")
        endif()
        if(NOT OpenGL_FOUND)
            message(WARNING "OpenGL не найден - GUI не будет собран")
        endif()
        set(BUILD_GUI OFF)
    endif()
endif()

# ============================================================================
# Тесты
# ============================================================================
if(BUILD_TESTING)
    enable_testing()

    # doctest
    FetchContent_Declare(
        doctest
        GIT_REPOSITORY https://github.com/doctest/doctest.git
        GIT_TAG v2.4.11
    )
    FetchContent_MakeAvailable(doctest)

    # Unit тесты
    add_executable(incline3d_tests
        tests/test_main.cpp
        tests/unit/test_angle_utils.cpp
        tests/unit/test_trajectory.cpp
    )

    target_link_libraries(incline3d_tests PRIVATE
        incline3d_core
        doctest::doctest
    )

    include(${doctest_SOURCE_DIR}/scripts/cmake/doctest.cmake)
    doctest_discover_tests(incline3d_tests)

    # Parity тесты (сравнение с эталоном Delphi)
    add_executable(incline3d_parity_tests
        tests/test_main.cpp
        tests/parity/test_parity_trajectory.cpp
    )

    target_compile_definitions(incline3d_parity_tests PRIVATE
        INCLINE3D_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
    )

    target_link_libraries(incline3d_parity_tests PRIVATE
        incline3d_core
        doctest::doctest
    )

    doctest_discover_tests(incline3d_parity_tests)
endif()
