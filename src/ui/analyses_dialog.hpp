/**
 * @file analyses_dialog.hpp
 * @brief Диалог анализа сближения/отхода
 */

#pragma once

#include <gtk/gtk.h>
#include "model/project.hpp"

namespace incline::ui {

void showAnalysesDialog(GtkWindow* parent, incline::model::Project* project);

} // namespace incline::ui
