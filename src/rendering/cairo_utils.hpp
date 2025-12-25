/**
 * @file cairo_utils.hpp
 * @brief Вспомогательные функции для работы с Cairo и цветами
 */

#pragma once

#include <cairo.h>
#include "model/types.hpp"

namespace incline::rendering {

inline void setCairoColor(cairo_t* cr, const model::Color& color) {
    constexpr double inv = 1.0 / 255.0;
    cairo_set_source_rgb(
        cr,
        static_cast<double>(color.r) * inv,
        static_cast<double>(color.g) * inv,
        static_cast<double>(color.b) * inv
    );
}

inline void setCairoColorWithAlpha(cairo_t* cr, const model::Color& color, double alpha) {
    constexpr double inv = 1.0 / 255.0;
    cairo_set_source_rgba(
        cr,
        static_cast<double>(color.r) * inv,
        static_cast<double>(color.g) * inv,
        static_cast<double>(color.b) * inv,
        alpha
    );
}

} // namespace incline::rendering
