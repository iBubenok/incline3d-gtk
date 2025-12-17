/**
 * @file analyses_dialog.cpp
 * @brief Диалог анализа сближения/отхода
 */

#include "analyses_dialog.hpp"
#include "core/analysis.hpp"
#include "io/analysis_report_writer.hpp"
#include <filesystem>

namespace incline::ui {
namespace {

using namespace incline::model;

struct AnalysesDialogState {
    Project* project = nullptr;
    GtkWidget* base_combo = nullptr;
    GtkWidget* target_combo = nullptr;
    GtkWidget* summary_label = nullptr;
    GtkWidget* path_label = nullptr;
};

void fill_combo(GtkComboBoxText* combo, const std::vector<WellEntry*>& wells) {
    for (const auto* well : wells) {
        auto name = well->result.has_value() ? well->result->displayName() : well->source_data.displayName();
        gtk_combo_box_text_append(combo, well->id.c_str(), name.c_str());
    }
    if (!wells.empty()) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo), 0);
    }
}

WellEntry* findWell(Project* project, const char* id) {
    if (!project || !id) return nullptr;
    return project->findWell(id);
}

void update_summary(AnalysesDialogState* state, const core::AnalysesReportData& report) {
    if (!GTK_IS_LABEL(state->summary_label)) return;

    if (!report.valid) {
        gtk_label_set_text(GTK_LABEL(state->summary_label), "Недостаточно данных для анализа.");
        return;
    }

    std::string text = "Минимальное расстояние: " +
        std::to_string(report.proximity.min_distance.value) + " м (MD базовой " +
        std::to_string(report.proximity.depth1.value) + " м, MD целевой " +
        std::to_string(report.proximity.depth2.value) + " м)";
    gtk_label_set_text(GTK_LABEL(state->summary_label), text.c_str());
}

void on_run_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* state = static_cast<AnalysesDialogState*>(user_data);
    auto wells = state->project->processedWells();
    if (wells.size() < 2) {
        gtk_label_set_text(GTK_LABEL(state->summary_label), "Нужно минимум две обработанные скважины.");
        return;
    }

    const char* base_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->base_combo));
    const char* target_id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(state->target_combo));

    auto* base = findWell(state->project, base_id);
    auto* target = findWell(state->project, target_id);
    if (!base || !target || !base->isProcessed() || !target->isProcessed()) {
        gtk_label_set_text(GTK_LABEL(state->summary_label), "Выберите обработанные скважины.");
        return;
    }

    auto report = core::buildAnalysesReport(*base->result, *target->result, Meters{50.0});
    update_summary(state, report);

    std::error_code ec;
    auto out_dir = std::filesystem::temp_directory_path() / "incline3d_analyses_ui";
    std::filesystem::remove_all(out_dir, ec);
    auto export_result = io::writeAnalysisReport(report, out_dir);

    std::string path_text = "Отчёт: " + out_dir.string();
    gtk_label_set_text(GTK_LABEL(state->path_label), path_text.c_str());
}

} // namespace

void showAnalysesDialog(GtkWindow* parent, Project* project) {
    auto* dialog = gtk_dialog_new_with_buttons(
        "Анализы",
        parent,
        GTK_DIALOG_MODAL,
        "_Закрыть",
        GTK_RESPONSE_CLOSE,
        nullptr
    );

    auto* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    gtk_orientable_set_orientation(GTK_ORIENTABLE(content), GTK_ORIENTATION_VERTICAL);
    gtk_widget_set_margin_top(content, 12);
    gtk_widget_set_margin_bottom(content, 12);
    gtk_widget_set_margin_start(content, 12);
    gtk_widget_set_margin_end(content, 12);

    auto* wells_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(content), wells_box);

    auto* base_combo = gtk_combo_box_text_new();
    auto* target_combo = gtk_combo_box_text_new();

    auto wells = project->processedWells();
    fill_combo(GTK_COMBO_BOX_TEXT(base_combo), wells);
    fill_combo(GTK_COMBO_BOX_TEXT(target_combo), wells);

    gtk_box_append(GTK_BOX(wells_box), gtk_label_new("Базовая:"));
    gtk_box_append(GTK_BOX(wells_box), base_combo);
    gtk_box_append(GTK_BOX(wells_box), gtk_label_new("Целевая:"));
    gtk_box_append(GTK_BOX(wells_box), target_combo);

    auto* summary_label = gtk_label_new("Запустите анализ");
    gtk_widget_set_halign(summary_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), summary_label);

    auto* path_label = gtk_label_new("Отчёт: -");
    gtk_widget_set_halign(path_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), path_label);

    auto* run_button = gtk_button_new_with_label("Выполнить анализ");
    gtk_box_append(GTK_BOX(content), run_button);

    auto* state = new AnalysesDialogState();
    state->project = project;
    state->base_combo = base_combo;
    state->target_combo = target_combo;
    state->summary_label = summary_label;
    state->path_label = path_label;

    g_signal_connect(run_button, "clicked", G_CALLBACK(on_run_clicked), state);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), nullptr);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_present(GTK_WINDOW(dialog));
}

} // namespace incline::ui
