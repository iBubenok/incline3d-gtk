/**
 * @file main_window.cpp
 * @brief Реализация главного окна
 * @author Yan Bubenok <yan@bubenok.com>
 */

#include "main_window.hpp"
#include "model/project.hpp"
#include "core/processing.hpp"
#include "io/format_registry.hpp"
#include "io/project_io.hpp"
#include "rendering/trajectory_renderer.hpp"
#include "rendering/plan_renderer.hpp"
#include "rendering/vertical_renderer.hpp"
#include <filesystem>
#include <memory>

using namespace incline::model;
using namespace incline::core;
using namespace incline::io;
using namespace incline::rendering;

struct _InclineMainWindow {
    GtkApplicationWindow parent_instance;

    // Виджеты
    GtkWidget* header_bar;
    GtkWidget* main_paned;      // Разделитель левая/правая часть
    GtkWidget* left_box;        // Левая панель
    GtkWidget* right_notebook;  // Вкладки визуализации

    // Левая панель
    GtkWidget* wells_list;
    GtkWidget* project_points_list;

    // Вкладки визуализации
    GtkWidget* axonometry_view;
    GtkWidget* plan_view;
    GtkWidget* vertical_view;

    // Статусная строка
    GtkWidget* status_bar;
    GtkWidget* status_label;
    GtkWidget* coords_label;
    GtkWidget* mode_label;

    // Данные
    Project* project;
    std::string project_path;
    bool project_modified;

    // Рендереры
    TrajectoryRenderer* trajectory_renderer;
    PlanRenderer* plan_renderer;
    VerticalRenderer* vertical_renderer;
    Camera* camera;

    // Опции обработки
    ProcessingOptions processing_options;
};

G_DEFINE_TYPE(InclineMainWindow, incline_main_window, GTK_TYPE_APPLICATION_WINDOW)

// Forward declarations
static void setup_actions(InclineMainWindow* self);
static void setup_menu(InclineMainWindow* self);
static void create_ui(InclineMainWindow* self);
static void on_new_project(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void on_open_project(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void on_save_project(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void on_import_data(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void on_process_selected(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void update_title(InclineMainWindow* self);
static void update_status(InclineMainWindow* self, const char* message);
static void sync_processing_from_project(InclineMainWindow* self);
static void sync_processing_to_project(InclineMainWindow* self);

static void incline_main_window_init(InclineMainWindow* self) {
    self->project = new Project();
    self->project_modified = false;
    self->trajectory_renderer = new TrajectoryRenderer();
    self->plan_renderer = new PlanRenderer();
    self->vertical_renderer = new VerticalRenderer();
    self->camera = new Camera();
    self->processing_options = processingOptionsFromSettings(self->project->processing);

    create_ui(self);
    setup_actions(self);
    setup_menu(self);

    update_title(self);
    update_status(self, "Готово");
}

static void incline_main_window_finalize(GObject* object) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(object);

    delete self->project;
    delete self->trajectory_renderer;
    delete self->plan_renderer;
    delete self->vertical_renderer;
    delete self->camera;

    G_OBJECT_CLASS(incline_main_window_parent_class)->finalize(object);
}

static void incline_main_window_class_init(InclineMainWindowClass* klass) {
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    object_class->finalize = incline_main_window_finalize;
}

static void create_ui(InclineMainWindow* self) {
    // Размеры окна
    gtk_window_set_default_size(GTK_WINDOW(self), 1280, 800);

    // Header bar
    self->header_bar = gtk_header_bar_new();
    gtk_window_set_titlebar(GTK_WINDOW(self), self->header_bar);

    // Кнопки в header bar
    GtkWidget* open_btn = gtk_button_new_from_icon_name("document-open-symbolic");
    gtk_widget_set_tooltip_text(open_btn, "Открыть проект (Ctrl+O)");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(open_btn), "win.open");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(self->header_bar), open_btn);

    GtkWidget* save_btn = gtk_button_new_from_icon_name("document-save-symbolic");
    gtk_widget_set_tooltip_text(save_btn, "Сохранить проект (Ctrl+S)");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(save_btn), "win.save");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(self->header_bar), save_btn);

    GtkWidget* import_btn = gtk_button_new_from_icon_name("list-add-symbolic");
    gtk_widget_set_tooltip_text(import_btn, "Импорт данных (Ctrl+I)");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(import_btn), "win.import");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(self->header_bar), import_btn);

    GtkWidget* process_btn = gtk_button_new_from_icon_name("media-playback-start-symbolic");
    gtk_widget_set_tooltip_text(process_btn, "Обработать (F5)");
    gtk_actionable_set_action_name(GTK_ACTIONABLE(process_btn), "win.process");
    gtk_header_bar_pack_start(GTK_HEADER_BAR(self->header_bar), process_btn);

    // Меню
    GtkWidget* menu_btn = gtk_menu_button_new();
    gtk_menu_button_set_icon_name(GTK_MENU_BUTTON(menu_btn), "open-menu-symbolic");
    gtk_header_bar_pack_end(GTK_HEADER_BAR(self->header_bar), menu_btn);

    // Главный контейнер
    GtkWidget* main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_window_set_child(GTK_WINDOW(self), main_box);

    // Разделитель
    self->main_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_vexpand(self->main_paned, TRUE);
    gtk_box_append(GTK_BOX(main_box), self->main_paned);

    // === Левая панель ===
    self->left_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_widget_set_size_request(self->left_box, 300, -1);
    gtk_widget_set_margin_start(self->left_box, 6);
    gtk_widget_set_margin_end(self->left_box, 6);
    gtk_widget_set_margin_top(self->left_box, 6);
    gtk_widget_set_margin_bottom(self->left_box, 6);
    gtk_paned_set_start_child(GTK_PANED(self->main_paned), self->left_box);
    gtk_paned_set_shrink_start_child(GTK_PANED(self->main_paned), FALSE);

    // Заголовок "Скважины"
    GtkWidget* wells_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(wells_label), "<b>Скважины</b>");
    gtk_widget_set_halign(wells_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(self->left_box), wells_label);

    // Список скважин
    GtkWidget* wells_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(wells_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(wells_scroll, TRUE);
    gtk_box_append(GTK_BOX(self->left_box), wells_scroll);

    self->wells_list = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(self->wells_list), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(wells_scroll), self->wells_list);

    // Плейсхолдер для пустого списка
    GtkWidget* placeholder = gtk_label_new("Нет скважин\n\nНажмите + для импорта");
    gtk_widget_add_css_class(placeholder, "dim-label");
    gtk_list_box_set_placeholder(GTK_LIST_BOX(self->wells_list), placeholder);

    // Заголовок "Проектные точки"
    GtkWidget* pp_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(pp_label), "<b>Проектные точки</b>");
    gtk_widget_set_halign(pp_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(self->left_box), pp_label);

    // Список проектных точек
    GtkWidget* pp_scroll = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pp_scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(pp_scroll, -1, 150);
    gtk_box_append(GTK_BOX(self->left_box), pp_scroll);

    self->project_points_list = gtk_list_box_new();
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pp_scroll), self->project_points_list);

    // === Правая панель (визуализация) ===
    self->right_notebook = gtk_notebook_new();
    gtk_paned_set_end_child(GTK_PANED(self->main_paned), self->right_notebook);
    gtk_paned_set_shrink_end_child(GTK_PANED(self->main_paned), FALSE);

    // Вкладка "Аксонометрия"
    self->axonometry_view = gtk_gl_area_new();
    gtk_widget_set_hexpand(self->axonometry_view, TRUE);
    gtk_widget_set_vexpand(self->axonometry_view, TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(self->right_notebook),
        self->axonometry_view, gtk_label_new("Аксонометрия"));

    // Вкладка "План"
    self->plan_view = gtk_drawing_area_new();
    gtk_widget_set_hexpand(self->plan_view, TRUE);
    gtk_widget_set_vexpand(self->plan_view, TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(self->right_notebook),
        self->plan_view, gtk_label_new("План"));

    // Вкладка "Вертикальная проекция"
    self->vertical_view = gtk_drawing_area_new();
    gtk_widget_set_hexpand(self->vertical_view, TRUE);
    gtk_widget_set_vexpand(self->vertical_view, TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(self->right_notebook),
        self->vertical_view, gtk_label_new("Верт. проекция"));

    // === Статусная строка ===
    self->status_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_widget_add_css_class(self->status_bar, "statusbar");
    gtk_widget_set_margin_start(self->status_bar, 6);
    gtk_widget_set_margin_end(self->status_bar, 6);
    gtk_widget_set_margin_top(self->status_bar, 3);
    gtk_widget_set_margin_bottom(self->status_bar, 3);
    gtk_box_append(GTK_BOX(main_box), self->status_bar);

    self->status_label = gtk_label_new("Готово");
    gtk_widget_set_hexpand(self->status_label, TRUE);
    gtk_widget_set_halign(self->status_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(self->status_bar), self->status_label);

    self->coords_label = gtk_label_new("");
    gtk_box_append(GTK_BOX(self->status_bar), self->coords_label);

    self->mode_label = gtk_label_new("Азимут: Авто");
    gtk_box_append(GTK_BOX(self->status_bar), self->mode_label);

    // Установка позиции разделителя
    gtk_paned_set_position(GTK_PANED(self->main_paned), 320);
}

static void setup_actions(InclineMainWindow* self) {
    // Действие "Создать"
    GSimpleAction* new_action = g_simple_action_new("new", nullptr);
    g_signal_connect(new_action, "activate", G_CALLBACK(on_new_project), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(new_action));

    // Действие "Открыть"
    GSimpleAction* open_action = g_simple_action_new("open", nullptr);
    g_signal_connect(open_action, "activate", G_CALLBACK(on_open_project), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(open_action));

    // Действие "Сохранить"
    GSimpleAction* save_action = g_simple_action_new("save", nullptr);
    g_signal_connect(save_action, "activate", G_CALLBACK(on_save_project), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(save_action));

    // Действие "Импорт"
    GSimpleAction* import_action = g_simple_action_new("import", nullptr);
    g_signal_connect(import_action, "activate", G_CALLBACK(on_import_data), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(import_action));

    // Действие "Обработать"
    GSimpleAction* process_action = g_simple_action_new("process", nullptr);
    g_signal_connect(process_action, "activate", G_CALLBACK(on_process_selected), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(process_action));

    // Горячие клавиши
    GtkApplication* app = gtk_window_get_application(GTK_WINDOW(self));
    if (app) {
        const char* new_accels[] = {"<Control>n", nullptr};
        const char* open_accels[] = {"<Control>o", nullptr};
        const char* save_accels[] = {"<Control>s", nullptr};
        const char* import_accels[] = {"<Control>i", nullptr};
        const char* process_accels[] = {"F5", nullptr};

        gtk_application_set_accels_for_action(app, "win.new", new_accels);
        gtk_application_set_accels_for_action(app, "win.open", open_accels);
        gtk_application_set_accels_for_action(app, "win.save", save_accels);
        gtk_application_set_accels_for_action(app, "win.import", import_accels);
        gtk_application_set_accels_for_action(app, "win.process", process_accels);
    }
}

static void setup_menu(InclineMainWindow* self) {
    GMenu* menu = g_menu_new();

    // Секция "Проект"
    GMenu* project_section = g_menu_new();
    g_menu_append(project_section, "Создать", "win.new");
    g_menu_append(project_section, "Открыть...", "win.open");
    g_menu_append(project_section, "Сохранить", "win.save");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(project_section));

    // Секция "Данные"
    GMenu* data_section = g_menu_new();
    g_menu_append(data_section, "Импорт...", "win.import");
    g_menu_append(data_section, "Обработать", "win.process");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(data_section));

    // Секция "Справка"
    GMenu* help_section = g_menu_new();
    g_menu_append(help_section, "О программе", "app.about");
    g_menu_append(help_section, "Выход", "app.quit");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(help_section));

    // Привязываем меню к кнопке
    GtkWidget* menu_btn = gtk_widget_get_last_child(self->header_bar);
    if (GTK_IS_MENU_BUTTON(menu_btn)) {
        gtk_menu_button_set_menu_model(GTK_MENU_BUTTON(menu_btn), G_MENU_MODEL(menu));
    }

    g_object_unref(menu);
}

static void update_title(InclineMainWindow* self) {
    std::string title = "Incline3D";

    if (!self->project->name.empty()) {
        title = self->project->name + " - Incline3D";
    } else if (!self->project_path.empty()) {
        std::filesystem::path p(self->project_path);
        title = p.stem().string() + " - Incline3D";
    }

    if (self->project_modified) {
        title = "• " + title;
    }

    gtk_window_set_title(GTK_WINDOW(self), title.c_str());
}

static void update_status(InclineMainWindow* self, const char* message) {
    gtk_label_set_text(GTK_LABEL(self->status_label), message);
}

static void sync_processing_from_project(InclineMainWindow* self) {
    self->processing_options = processingOptionsFromSettings(self->project->processing);
}

static void sync_processing_to_project(InclineMainWindow* self) {
    self->project->processing = processingSettingsFromOptions(self->processing_options);
}

static void on_new_project(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);

    // TODO: Проверить несохранённые изменения

    delete self->project;
    self->project = new Project();
    self->project_path.clear();
    self->project_modified = false;
    sync_processing_from_project(self);

    // Очистить UI
    // TODO: Обновить списки

    update_title(self);
    update_status(self, "Создан новый проект");
}

static void on_open_project(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);

    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Открыть проект");

    // Фильтр файлов
    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Проект Incline3D (*.inclproj)");
    gtk_file_filter_add_pattern(filter, "*.inclproj");

    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filter);
    g_object_unref(filters);

    gtk_file_dialog_open(dialog, GTK_WINDOW(self), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            GtkFileDialog* dlg = GTK_FILE_DIALOG(source);
            InclineMainWindow* win = INCLINE_MAIN_WINDOW(data);

            GFile* file = gtk_file_dialog_open_finish(dlg, result, nullptr);
            if (file) {
                char* path = g_file_get_path(file);
                if (path) {
                    main_window_open_file(win, path);
                    g_free(path);
                }
                g_object_unref(file);
            }
        }, self);

    g_object_unref(dialog);
}

static void on_save_project(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);
    main_window_save_project(self);
}

static void on_import_data(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);

    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Импорт данных");

    // Фильтры файлов
    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);

    GtkFileFilter* all_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(all_filter, "Все поддерживаемые");
    gtk_file_filter_add_pattern(all_filter, "*.csv");
    gtk_file_filter_add_pattern(all_filter, "*.las");
    gtk_file_filter_add_pattern(all_filter, "*.txt");
    g_list_store_append(filters, all_filter);
    g_object_unref(all_filter);

    GtkFileFilter* csv_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(csv_filter, "CSV файлы (*.csv, *.txt)");
    gtk_file_filter_add_pattern(csv_filter, "*.csv");
    gtk_file_filter_add_pattern(csv_filter, "*.txt");
    g_list_store_append(filters, csv_filter);
    g_object_unref(csv_filter);

    GtkFileFilter* las_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(las_filter, "LAS файлы (*.las)");
    gtk_file_filter_add_pattern(las_filter, "*.las");
    g_list_store_append(filters, las_filter);
    g_object_unref(las_filter);

    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filters);

    gtk_file_dialog_open(dialog, GTK_WINDOW(self), nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            GtkFileDialog* dlg = GTK_FILE_DIALOG(source);
            InclineMainWindow* win = INCLINE_MAIN_WINDOW(data);

            GFile* file = gtk_file_dialog_open_finish(dlg, result, nullptr);
            if (file) {
                char* path = g_file_get_path(file);
                if (path) {
                    main_window_import_data(win, path);
                    g_free(path);
                }
                g_object_unref(file);
            }
        }, self);

    g_object_unref(dialog);
}

static void on_process_selected(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);
    main_window_process_selected(self);
}

// === Public API ===

GtkWidget* main_window_new(GtkApplication* app) {
    return GTK_WIDGET(g_object_new(INCLINE_TYPE_MAIN_WINDOW,
        "application", app,
        nullptr));
}

void main_window_open_file(InclineMainWindow* self, const char* path) {
    try {
        Project loaded = loadProject(path);
        delete self->project;
        self->project = new Project(std::move(loaded));
        self->project_path = path;
        self->project_modified = false;
        sync_processing_from_project(self);

        main_window_update_views(self);
        update_title(self);
        update_status(self, "Проект загружен");
    } catch (const std::exception& e) {
        GtkAlertDialog* alert = gtk_alert_dialog_new("Ошибка загрузки проекта:\n%s", e.what());
        gtk_alert_dialog_show(alert, GTK_WINDOW(self));
        g_object_unref(alert);
    }
}

gboolean main_window_save_project(InclineMainWindow* self) {
    if (self->project_path.empty()) {
        // TODO: Диалог "Сохранить как"
        update_status(self, "Укажите имя файла для сохранения");
        return FALSE;
    }

    try {
        saveProject(*self->project, self->project_path);
        self->project_modified = false;
        update_title(self);
        update_status(self, "Проект сохранён");
        return TRUE;
    } catch (const std::exception& e) {
        GtkAlertDialog* alert = gtk_alert_dialog_new("Ошибка сохранения:\n%s", e.what());
        gtk_alert_dialog_show(alert, GTK_WINDOW(self));
        g_object_unref(alert);
        return FALSE;
    }
}

void main_window_import_data(InclineMainWindow* self, const char* path) {
    update_status(self, "Импорт данных...");

    auto result = importMeasurements(path);

    if (!result.success) {
        GtkAlertDialog* alert = gtk_alert_dialog_new("Ошибка импорта:\n%s", result.error_message.c_str());
        gtk_alert_dialog_show(alert, GTK_WINDOW(self));
        g_object_unref(alert);
        update_status(self, "Ошибка импорта");
        return;
    }

    // Добавляем скважину в проект
    WellEntry entry;
    entry.source_data = std::move(result.data);
    entry.visible = true;
    entry.color = Color::fromHex("#0000FF");  // Синий по умолчанию

    self->project->wells.push_back(std::move(entry));
    self->project_modified = true;

    // TODO: Обновить список скважин в UI

    update_title(self);

    std::string msg = "Импортировано " + std::to_string(
        self->project->wells.back().source_data.measurements.size()) + " точек";
    update_status(self, msg.c_str());
}

void main_window_process_selected(InclineMainWindow* self) {
    if (self->project->wells.empty()) {
        update_status(self, "Нет скважин для обработки");
        return;
    }

    update_status(self, "Обработка...");
    sync_processing_to_project(self);

    // Обрабатываем все скважины (TODO: только выбранные)
    for (auto& entry : self->project->wells) {
        if (entry.source_data.measurements.empty()) continue;

        entry.result = processWell(entry.source_data, self->processing_options);
    }

    self->project_modified = true;
    main_window_update_views(self);
    update_title(self);
    update_status(self, "Обработка завершена");
}

Project* main_window_get_project(InclineMainWindow* self) {
    return self->project;
}

void main_window_update_views(InclineMainWindow* self) {
    // Обновляем рендереры
    self->plan_renderer->updateFromProject(*self->project);
    self->vertical_renderer->updateFromProject(*self->project);

    // Перерисовываем виджеты
    gtk_widget_queue_draw(self->plan_view);
    gtk_widget_queue_draw(self->vertical_view);
    gtk_gl_area_queue_render(GTK_GL_AREA(self->axonometry_view));
}
