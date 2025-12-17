/**
 * @file diagnostics_dialog.hpp
 * @brief Диалог диагностики/selftest
 */

#pragma once

#include <gtk/gtk.h>
#include "model/project.hpp"

namespace incline::ui {

void showDiagnosticsDialog(GtkWindow* parent, incline::model::Project* project);

} // namespace incline::ui
