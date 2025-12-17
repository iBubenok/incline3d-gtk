/**
 * @file diagnostics_dialog.cpp
 * @brief Диалог запуска диагностики/selftest
 */

#include "diagnostics_dialog.hpp"
#include "app/diagnostics_runner.hpp"
#include <filesystem>
#include <thread>

namespace incline::ui {
namespace {

using namespace incline::model;

struct DiagnosticsDialogState {
    GtkWidget* dialog = nullptr;
    GtkWidget* list = nullptr;
    GtkWidget* status_label = nullptr;
    GtkWidget* path_label = nullptr;
    GtkWidget* run_button = nullptr;
};

void populate_checks(DiagnosticsDialogState* state, const model::DiagnosticsReport& report) {
    if (!GTK_IS_LIST_BOX(state->list)) return;

    auto* list_box = GTK_LIST_BOX(state->list);
    for (GtkWidget* row = gtk_widget_get_first_child(GTK_WIDGET(list_box));
         row != nullptr;
         row = gtk_widget_get_first_child(GTK_WIDGET(list_box))) {
        gtk_list_box_remove(list_box, row);
    }

    for (const auto& check : report.checks) {
        std::string row_text = check.title + " — " + std::string(diagnosticStatusToString(check.status));
        if (!check.details.empty()) {
            row_text += " (" + check.details + ")";
        }
        GtkWidget* row = gtk_label_new(row_text.c_str());
        gtk_widget_set_halign(row, GTK_ALIGN_START);
        gtk_list_box_append(list_box, row);
    }
}

struct AsyncPayload {
    DiagnosticsDialogState* state;
    incline::app::DiagnosticsCommandResult result;
};

gboolean on_finished(gpointer data) {
    auto* payload = static_cast<AsyncPayload*>(data);
    auto* state = payload->state;

    if (!GTK_IS_WIDGET(state->dialog) || !gtk_widget_get_visible(state->dialog)) {
        delete payload;
        return G_SOURCE_REMOVE;
    }

    auto summary = payload->result.summary;
    std::string status_text = "Статус: " + std::string(diagnosticStatusToString(summary.status)) +
        " (OK: " + std::to_string(summary.ok) +
        ", WARN: " + std::to_string(summary.warning) +
        ", FAIL: " + std::to_string(summary.fail) +
        ", SKIPPED: " + std::to_string(summary.skipped) + ")";
    gtk_label_set_text(GTK_LABEL(state->status_label), status_text.c_str());

    std::string path_text = "Каталог отчёта: " + payload->result.output_dir.string();
    gtk_label_set_text(GTK_LABEL(state->path_label), path_text.c_str());

    populate_checks(state, payload->result.report);
    gtk_widget_set_sensitive(state->run_button, TRUE);

    delete payload;
    return G_SOURCE_REMOVE;
}

void run_diagnostics_async(DiagnosticsDialogState* state) {
    gtk_widget_set_sensitive(state->run_button, FALSE);
    gtk_label_set_text(GTK_LABEL(state->status_label), "Диагностика выполняется...");

    std::thread([state]() {
        std::error_code ec;
        auto out_dir = std::filesystem::temp_directory_path() / "incline3d_diag_ui";
        std::filesystem::remove_all(out_dir, ec);
        auto result = incline::app::runDiagnosticsCommand(out_dir, true);

        auto* payload = new AsyncPayload{state, std::move(result)};
        g_idle_add_full(G_PRIORITY_DEFAULT, on_finished, payload, nullptr);
    }).detach();
}

void on_run_clicked(GtkButton* /*button*/, gpointer user_data) {
    auto* state = static_cast<DiagnosticsDialogState*>(user_data);
    run_diagnostics_async(state);
}

} // namespace

void showDiagnosticsDialog(GtkWindow* parent, Project* /*project*/) {
    auto* dialog = gtk_dialog_new_with_buttons(
        "Диагностика",
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

    auto* status_label = gtk_label_new("Диагностика не запускалась");
    gtk_widget_set_halign(status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), status_label);

    auto* list = gtk_list_box_new();
    gtk_widget_set_vexpand(list, TRUE);
    gtk_widget_set_hexpand(list, TRUE);
    gtk_widget_set_size_request(list, 420, 200);
    gtk_box_append(GTK_BOX(content), list);

    auto* path_label = gtk_label_new("Каталог отчёта: -");
    gtk_widget_set_halign(path_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(content), path_label);

    auto* button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    gtk_box_append(GTK_BOX(content), button_box);

    auto* run_button = gtk_button_new_with_label("Запустить диагностику");
    gtk_box_append(GTK_BOX(button_box), run_button);

    auto* state = new DiagnosticsDialogState();
    state->dialog = dialog;
    state->list = list;
    state->status_label = status_label;
    state->path_label = path_label;
    state->run_button = run_button;

    g_signal_connect(run_button, "clicked", G_CALLBACK(on_run_clicked), state);
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), nullptr);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_present(GTK_WINDOW(dialog));

    // Сразу показать результаты для удобства
    run_diagnostics_async(state);
}

} // namespace incline::ui
