/**
 * @file application.cpp
 * @brief Реализация GTK4 приложения
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "application.hpp"
#include "main_window.hpp"

namespace incline::ui {

Application::Application() {
    app_ = gtk_application_new(APP_ID, G_APPLICATION_HANDLES_OPEN);

    g_signal_connect(app_, "activate", G_CALLBACK(onActivate), this);
    g_signal_connect(app_, "startup", G_CALLBACK(onStartup), this);
    g_signal_connect(app_, "shutdown", G_CALLBACK(onShutdown), this);
    g_signal_connect(app_, "open", G_CALLBACK(onOpen), this);
}

Application::~Application() {
    if (app_) {
        g_object_unref(app_);
        app_ = nullptr;
    }
}

int Application::run(int argc, char* argv[]) {
    return g_application_run(G_APPLICATION(app_), argc, argv);
}

void Application::onActivate(GtkApplication* /*app*/, gpointer user_data) {
    auto* self = static_cast<Application*>(user_data);
    self->createMainWindow();
}

void Application::onStartup(GtkApplication* /*app*/, gpointer user_data) {
    auto* self = static_cast<Application*>(user_data);
    self->setupActions();
}

void Application::onShutdown(GtkApplication* /*app*/, gpointer /*user_data*/) {
    // Очистка при завершении
}

void Application::onOpen(GtkApplication* /*app*/, GFile** files, int n_files,
                         const char* /*hint*/, gpointer user_data) {
    auto* self = static_cast<Application*>(user_data);

    if (!self->main_window_) {
        self->createMainWindow();
    }

    // Открываем первый файл
    if (n_files > 0) {
        char* path = g_file_get_path(files[0]);
        if (path) {
            // TODO: загрузить файл
            // MainWindow* mw = INCLINE_MAIN_WINDOW(self->main_window_);
            // main_window_open_file(mw, path);
            g_free(path);
        }
    }
}

void Application::setupActions() {
    // Действие "Выход"
    GSimpleAction* quit_action = g_simple_action_new("quit", nullptr);
    g_signal_connect_swapped(quit_action, "activate",
        G_CALLBACK(g_application_quit), app_);
    g_action_map_add_action(G_ACTION_MAP(app_), G_ACTION(quit_action));

    // Горячие клавиши
    const char* quit_accels[] = {"<Control>q", nullptr};
    gtk_application_set_accels_for_action(app_, "app.quit", quit_accels);

    // Действие "О программе"
    GSimpleAction* about_action = g_simple_action_new("about", nullptr);
    g_signal_connect(about_action, "activate",
        [](GSimpleAction*, GVariant*, gpointer user_data) {
            auto* self = static_cast<Application*>(user_data);

            GtkWidget* dialog = gtk_about_dialog_new();
            gtk_about_dialog_set_program_name(GTK_ABOUT_DIALOG(dialog), APP_NAME);
            gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), APP_VERSION);
            gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog),
                "Обработка и визуализация данных инклинометрии скважин");
            gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog),
                "© 2024 Yan Bubenok");
            gtk_about_dialog_set_license_type(GTK_ABOUT_DIALOG(dialog),
                GTK_LICENSE_MIT_X11);
            gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog),
                "https://github.com/iBubenok/incline3d");

            const char* authors[] = {"Yan Bubenok <yan@bubenok.com>", nullptr};
            gtk_about_dialog_set_authors(GTK_ABOUT_DIALOG(dialog), authors);

            gtk_window_set_transient_for(GTK_WINDOW(dialog),
                GTK_WINDOW(self->main_window_));
            gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
            gtk_window_present(GTK_WINDOW(dialog));
        }, this);
    g_action_map_add_action(G_ACTION_MAP(app_), G_ACTION(about_action));
}

void Application::createMainWindow() {
    if (main_window_) {
        gtk_window_present(GTK_WINDOW(main_window_));
        return;
    }

    main_window_ = main_window_new(app_);
    gtk_window_present(GTK_WINDOW(main_window_));
}

} // namespace incline::ui
