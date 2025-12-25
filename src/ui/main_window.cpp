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
#include "diagnostics_dialog.hpp"
#include "analyses_dialog.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <cstdlib>
#include <algorithm>
#include <glm/glm.hpp>

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
    std::string selected_well_id;
    bool axonometry_needs_fit;

    // Рендереры
    TrajectoryRenderer* trajectory_renderer;
    PlanRenderer* plan_renderer;
    VerticalRenderer* vertical_renderer;
    Camera* camera;

    // Опции обработки
    ProcessingOptions processing_options;
    AxonometrySettings axonometry_settings;
    PlanRenderSettings plan_render_settings;
    VerticalRenderSettings vertical_render_settings;
    bool plan_needs_fit = true;
    bool vertical_needs_fit = true;
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
static void on_open_diagnostics(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void on_open_analyses(GSimpleAction* action, GVariant* parameter, gpointer user_data);
static void update_title(InclineMainWindow* self);
static void update_status(InclineMainWindow* self, const char* message);
static void sync_processing_from_project(InclineMainWindow* self);
static void sync_processing_to_project(InclineMainWindow* self);
static void refresh_wells_list(InclineMainWindow* self);
static void refresh_project_points_list(InclineMainWindow* self);
static WellEntry* get_selected_well(InclineMainWindow* self);
static void sync_render_settings_from_project(InclineMainWindow* self);
static void sync_render_settings_to_project(InclineMainWindow* self);
static void on_plan_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
static void on_vertical_draw(GtkDrawingArea* area, cairo_t* cr, int width, int height, gpointer user_data);
static void on_gl_realize(GtkGLArea* area, gpointer user_data);
static void on_gl_unrealize(GtkGLArea* area, gpointer user_data);
static gboolean on_gl_render(GtkGLArea* area, GdkGLContext* context, gpointer user_data);
static void on_gl_resize(GtkGLArea* area, int width, int height, gpointer user_data);
static void on_well_row_selected(GtkListBox* box, GtkListBoxRow* row, gpointer user_data);
static void on_well_visibility_toggled(GtkToggleButton* toggle, gpointer user_data);
static void on_add_project_point(GtkButton* button, gpointer user_data);
static void on_remove_project_point(GtkButton* button, gpointer user_data);
static gboolean save_project_to_path(InclineMainWindow* self, const std::filesystem::path& path);
static std::optional<std::filesystem::path> prompt_save_path(GtkWindow* parent);
static void apply_trajectory_renderer_settings(InclineMainWindow* self);

static void incline_main_window_init(InclineMainWindow* self) {
    self->project = new Project();
    self->project_modified = false;
    self->axonometry_needs_fit = true;
    self->trajectory_renderer = new TrajectoryRenderer();
    self->plan_renderer = new PlanRenderer();
    self->vertical_renderer = new VerticalRenderer();
    self->camera = new Camera();
    self->processing_options = processingOptionsFromSettings(self->project->processing);

    create_ui(self);
    setup_actions(self);
    setup_menu(self);
    sync_render_settings_from_project(self);
    refresh_wells_list(self);
    refresh_project_points_list(self);

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
    g_signal_connect(self->wells_list, "row-selected", G_CALLBACK(on_well_row_selected), self);
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
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(self->project_points_list), GTK_SELECTION_SINGLE);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(pp_scroll), self->project_points_list);

    GtkWidget* pp_actions = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_append(GTK_BOX(self->left_box), pp_actions);

    GtkWidget* add_pp_btn = gtk_button_new_with_label("Добавить точку");
    gtk_widget_set_tooltip_text(add_pp_btn, "Добавить проектную точку для выбранной скважины");
    g_signal_connect(add_pp_btn, "clicked", G_CALLBACK(on_add_project_point), self);
    gtk_box_append(GTK_BOX(pp_actions), add_pp_btn);

    GtkWidget* remove_pp_btn = gtk_button_new_with_label("Удалить точку");
    gtk_widget_set_tooltip_text(remove_pp_btn, "Удалить выбранную проектную точку");
    g_signal_connect(remove_pp_btn, "clicked", G_CALLBACK(on_remove_project_point), self);
    gtk_box_append(GTK_BOX(pp_actions), remove_pp_btn);

    // === Правая панель (визуализация) ===
    self->right_notebook = gtk_notebook_new();
    gtk_paned_set_end_child(GTK_PANED(self->main_paned), self->right_notebook);
    gtk_paned_set_shrink_end_child(GTK_PANED(self->main_paned), FALSE);

    // Вкладка "Аксонометрия"
    self->axonometry_view = gtk_gl_area_new();
    gtk_gl_area_set_has_depth_buffer(GTK_GL_AREA(self->axonometry_view), TRUE);
    gtk_gl_area_set_auto_render(GTK_GL_AREA(self->axonometry_view), FALSE);
    g_signal_connect(self->axonometry_view, "realize", G_CALLBACK(on_gl_realize), self);
    g_signal_connect(self->axonometry_view, "unrealize", G_CALLBACK(on_gl_unrealize), self);
    g_signal_connect(self->axonometry_view, "resize", G_CALLBACK(on_gl_resize), self);
    g_signal_connect(self->axonometry_view, "render", G_CALLBACK(on_gl_render), self);
    gtk_widget_set_hexpand(self->axonometry_view, TRUE);
    gtk_widget_set_vexpand(self->axonometry_view, TRUE);
    gtk_notebook_append_page(GTK_NOTEBOOK(self->right_notebook),
        self->axonometry_view, gtk_label_new("Аксонометрия"));

    // Вкладка "План"
    self->plan_view = gtk_drawing_area_new();
    gtk_widget_set_hexpand(self->plan_view, TRUE);
    gtk_widget_set_vexpand(self->plan_view, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(self->plan_view),
        on_plan_draw, self, nullptr);
    gtk_notebook_append_page(GTK_NOTEBOOK(self->right_notebook),
        self->plan_view, gtk_label_new("План"));

    // Вкладка "Вертикальная проекция"
    self->vertical_view = gtk_drawing_area_new();
    gtk_widget_set_hexpand(self->vertical_view, TRUE);
    gtk_widget_set_vexpand(self->vertical_view, TRUE);
    gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(self->vertical_view),
        on_vertical_draw, self, nullptr);
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

    // Действие "Диагностика"
    GSimpleAction* diag_action = g_simple_action_new("diagnostics", nullptr);
    g_signal_connect(diag_action, "activate", G_CALLBACK(on_open_diagnostics), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(diag_action));

    // Действие "Анализы"
    GSimpleAction* analyses_action = g_simple_action_new("analyses", nullptr);
    g_signal_connect(analyses_action, "activate", G_CALLBACK(on_open_analyses), self);
    g_action_map_add_action(G_ACTION_MAP(self), G_ACTION(analyses_action));

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

    // Секция "Диагностика и анализы"
    GMenu* diag_section = g_menu_new();
    g_menu_append(diag_section, "Диагностика...", "win.diagnostics");
    g_menu_append(diag_section, "Анализы...", "win.analyses");
    g_menu_append_section(menu, nullptr, G_MENU_MODEL(diag_section));

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

static void sync_render_settings_from_project(InclineMainWindow* self) {
    self->axonometry_settings = self->project->axonometry;
    self->camera->setRotation(
        static_cast<float>(self->axonometry_settings.rotation_x),
        0.0f,
        static_cast<float>(self->axonometry_settings.rotation_z)
    );
    self->camera->setPan(
        static_cast<float>(self->axonometry_settings.pan_x),
        static_cast<float>(self->axonometry_settings.pan_y)
    );
    self->camera->setZoom(self->axonometry_settings.zoom);

    const auto& plan = self->project->plan;
    self->plan_render_settings.scale = plan.scale;
    self->plan_render_settings.offset_x = plan.pan_x;
    self->plan_render_settings.offset_y = plan.pan_y;
    self->plan_render_settings.show_grid = plan.show_grid;
    self->plan_render_settings.grid_interval = plan.grid_interval;
    self->plan_render_settings.show_project_points = plan.show_project_points;
    self->plan_render_settings.show_tolerance_circles = plan.show_tolerance_circles;
    self->plan_render_settings.show_well_labels = plan.show_well_labels;
    self->plan_render_settings.show_north_arrow = plan.show_north_arrow;
    self->plan_render_settings.show_scale_bar = plan.show_scale_bar;
    self->plan_render_settings.background_color = plan.background_color;
    self->plan_render_settings.grid_color = plan.grid_color;
    self->plan_render_settings.trajectory_width = plan.trajectory_line_width;

    const auto& vert = self->project->vertical;
    self->vertical_render_settings.scale_h = vert.scale_horizontal;
    self->vertical_render_settings.scale_v = vert.scale_vertical;
    self->vertical_render_settings.offset_x = vert.pan_x;
    self->vertical_render_settings.offset_y = vert.pan_y;
    self->vertical_render_settings.projection_azimuth = vert.plane_azimuth;
    self->vertical_render_settings.auto_azimuth = vert.auto_plane;
    self->vertical_render_settings.show_grid = vert.show_grid;
    self->vertical_render_settings.grid_interval_h = vert.grid_interval_horizontal;
    self->vertical_render_settings.grid_interval_v = vert.grid_interval_vertical;
    self->vertical_render_settings.show_sea_level = vert.show_sea_level;
    self->vertical_render_settings.show_depth_labels = vert.show_depth_labels;
    self->vertical_render_settings.show_well_labels = vert.show_well_labels;
    self->vertical_render_settings.show_header = vert.show_header;
    self->vertical_render_settings.show_project_point_labels = vert.show_well_labels;
    self->vertical_render_settings.background_color = vert.background_color;
    self->vertical_render_settings.grid_color = vert.grid_color;
    self->vertical_render_settings.sea_level_color = vert.sea_level_color;
    self->vertical_render_settings.trajectory_width = vert.trajectory_line_width;
    apply_trajectory_renderer_settings(self);
    self->plan_needs_fit = true;
    self->vertical_needs_fit = true;
    self->axonometry_needs_fit = true;
}

static void sync_render_settings_to_project(InclineMainWindow* self) {
    self->project->plan.scale = self->plan_render_settings.scale;
    self->project->plan.pan_x = self->plan_render_settings.offset_x;
    self->project->plan.pan_y = self->plan_render_settings.offset_y;
    self->project->plan.show_grid = self->plan_render_settings.show_grid;
    self->project->plan.grid_interval = self->plan_render_settings.grid_interval;
    self->project->plan.show_project_points = self->plan_render_settings.show_project_points;
    self->project->plan.show_tolerance_circles = self->plan_render_settings.show_tolerance_circles;
    self->project->plan.show_well_labels = self->plan_render_settings.show_well_labels;
    self->project->plan.show_north_arrow = self->plan_render_settings.show_north_arrow;
    self->project->plan.show_scale_bar = self->plan_render_settings.show_scale_bar;
    self->project->plan.background_color = self->plan_render_settings.background_color;
    self->project->plan.grid_color = self->plan_render_settings.grid_color;
    self->project->plan.trajectory_line_width = self->plan_render_settings.trajectory_width;

    self->project->vertical.scale_horizontal = self->vertical_render_settings.scale_h;
    self->project->vertical.scale_vertical = self->vertical_render_settings.scale_v;
    self->project->vertical.pan_x = self->vertical_render_settings.offset_x;
    self->project->vertical.pan_y = self->vertical_render_settings.offset_y;
    self->project->vertical.plane_azimuth = self->vertical_render_settings.projection_azimuth;
    self->project->vertical.auto_plane = self->vertical_render_settings.auto_azimuth;
    self->project->vertical.show_grid = self->vertical_render_settings.show_grid;
    self->project->vertical.grid_interval_horizontal = self->vertical_render_settings.grid_interval_h;
    self->project->vertical.grid_interval_vertical = self->vertical_render_settings.grid_interval_v;
    self->project->vertical.show_sea_level = self->vertical_render_settings.show_sea_level;
    self->project->vertical.show_depth_labels = self->vertical_render_settings.show_depth_labels;
    self->project->vertical.show_well_labels = self->vertical_render_settings.show_well_labels;
    self->project->vertical.show_header = self->vertical_render_settings.show_header;
    self->project->vertical.background_color = self->vertical_render_settings.background_color;
    self->project->vertical.grid_color = self->vertical_render_settings.grid_color;
    self->project->vertical.sea_level_color = self->vertical_render_settings.sea_level_color;
    self->project->vertical.trajectory_line_width = self->vertical_render_settings.trajectory_width;
    auto rot = self->camera->getRotation();
    auto pan = self->camera->getPan();
    self->project->axonometry.rotation_x = rot.x;
    self->project->axonometry.rotation_z = rot.z;
    self->project->axonometry.zoom = self->camera->getZoom();
    self->project->axonometry.pan_x = pan.x;
    self->project->axonometry.pan_y = pan.y;
}

static WellEntry* get_selected_well(InclineMainWindow* self) {
    GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(self->wells_list));
    if (!row) return nullptr;
    const char* id = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "well-id"));
    if (!id) return nullptr;
    for (auto& well : self->project->wells) {
        if (well.id == id) {
            return &well;
        }
    }
    return nullptr;
}

static void refresh_project_points_list(InclineMainWindow* self) {
    GtkWidget* child = gtk_widget_get_first_child(self->project_points_list);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(self->project_points_list), child);
        child = next;
    }

    WellEntry* well = get_selected_well(self);
    if (!well) {
        return;
    }

    const ProjectPointList* points = nullptr;
    if (well->result.has_value() && !well->result->project_points.empty()) {
        points = &well->result->project_points;
    } else {
        points = &well->project_points;
    }

    int idx = 0;
    for (const auto& pp : *points) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

        std::string title = pp.name.empty() ? "Проектная точка" : pp.name;
        GtkWidget* name_label = gtk_label_new(title.c_str());
        gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
        gtk_box_append(GTK_BOX(box), name_label);

        std::string details = "Азимут: ";
        details += pp.azimuth_geographic.has_value()
            ? std::to_string(pp.azimuth_geographic->value)
            : "—";
        details += "°, смещение: " + std::to_string(pp.shift.value) + " м";
        if (pp.depth.has_value()) {
            details += ", глубина: " + std::to_string(pp.depth->value) + " м";
        } else if (pp.abs_depth.has_value()) {
            details += ", абс. отметка: " + std::to_string(pp.abs_depth->value) + " м";
        }
        GtkWidget* details_label = gtk_label_new(details.c_str());
        gtk_label_set_xalign(GTK_LABEL(details_label), 0.0f);
        gtk_widget_add_css_class(details_label, "dim-label");
        gtk_box_append(GTK_BOX(box), details_label);

        g_object_set_data(G_OBJECT(row), "pp-index", GINT_TO_POINTER(idx));
        gtk_list_box_append(GTK_LIST_BOX(self->project_points_list), row);
        ++idx;
    }
}

static void refresh_wells_list(InclineMainWindow* self) {
    GtkWidget* child = gtk_widget_get_first_child(self->wells_list);
    while (child) {
        GtkWidget* next = gtk_widget_get_next_sibling(child);
        gtk_list_box_remove(GTK_LIST_BOX(self->wells_list), child);
        child = next;
    }

    for (const auto& well : self->project->wells) {
        GtkWidget* row = gtk_list_box_row_new();
        GtkWidget* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_list_box_row_set_child(GTK_LIST_BOX_ROW(row), box);

        GtkWidget* toggle = gtk_check_button_new();
        gtk_check_button_set_active(GTK_CHECK_BUTTON(toggle), well.visible);
        g_object_set_data_full(G_OBJECT(toggle), "well-id", g_strdup(well.id.c_str()), g_free);
        g_signal_connect(toggle, "toggled", G_CALLBACK(on_well_visibility_toggled), self);
        gtk_box_append(GTK_BOX(box), toggle);

        std::string name = well.displayName();
        GtkWidget* name_label = gtk_label_new(name.c_str());
        gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
        gtk_box_append(GTK_BOX(box), name_label);

        std::string status = well.isProcessed() ? "Обработано" : "Не обработано";
        if (well.is_base) {
            status += " · базовая";
        }
        GtkWidget* status_label = gtk_label_new(status.c_str());
        gtk_widget_add_css_class(status_label, "dim-label");
        gtk_box_append(GTK_BOX(box), status_label);

        g_object_set_data_full(G_OBJECT(row), "well-id", g_strdup(well.id.c_str()), g_free);
        gtk_list_box_append(GTK_LIST_BOX(self->wells_list), row);
    }

    if (self->project->wells.empty()) {
        self->selected_well_id.clear();
        refresh_project_points_list(self);
        return;
    }

    if (self->selected_well_id.empty()) {
        self->selected_well_id = self->project->wells.front().id;
    }

    GtkWidget* row = gtk_widget_get_first_child(self->wells_list);
    bool selected = false;
    while (row) {
        const char* id = static_cast<const char*>(g_object_get_data(G_OBJECT(row), "well-id"));
        if (id && self->selected_well_id == id) {
            gtk_list_box_select_row(GTK_LIST_BOX(self->wells_list), GTK_LIST_BOX_ROW(row));
            selected = true;
            break;
        }
        row = gtk_widget_get_next_sibling(row);
    }

    if (!selected) {
        GtkListBoxRow* first_row = gtk_list_box_get_row_at_index(GTK_LIST_BOX(self->wells_list), 0);
        if (first_row) {
            gtk_list_box_select_row(GTK_LIST_BOX(self->wells_list), first_row);
            const char* id = static_cast<const char*>(g_object_get_data(G_OBJECT(first_row), "well-id"));
            self->selected_well_id = id ? id : "";
        }
    }

    refresh_project_points_list(self);
}

static void on_well_row_selected(GtkListBox* /*box*/, GtkListBoxRow* row, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    const char* id = row ? static_cast<const char*>(g_object_get_data(G_OBJECT(row), "well-id")) : nullptr;
    if (id) {
        self->selected_well_id = id;
    } else {
        self->selected_well_id.clear();
    }
    refresh_project_points_list(self);
    self->plan_needs_fit = true;
    self->vertical_needs_fit = true;
    main_window_update_views(self);
}

static void on_well_visibility_toggled(GtkToggleButton* toggle, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    const char* id = static_cast<const char*>(g_object_get_data(G_OBJECT(toggle), "well-id"));
    if (!id) return;

    for (auto& well : self->project->wells) {
        if (well.id == id) {
            well.visible = gtk_toggle_button_get_active(toggle);
            self->project_modified = true;
            main_window_update_views(self);
            update_status(self, well.visible ? "Скважина отображается" : "Скважина скрыта");
            return;
        }
    }
}

static void on_plan_draw(GtkDrawingArea* /*area*/, cairo_t* cr, int width, int height, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    self->plan_renderer->setSettings(self->plan_render_settings);
    if (self->plan_needs_fit) {
        self->plan_renderer->fitToContent(width, height);
        self->plan_needs_fit = false;
    }
    self->plan_renderer->render(cr, width, height);
    self->plan_render_settings = self->plan_renderer->getSettings();
}

static void on_vertical_draw(GtkDrawingArea* /*area*/, cairo_t* cr, int width, int height, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    self->vertical_renderer->setSettings(self->vertical_render_settings);
    if (self->vertical_needs_fit) {
        self->vertical_renderer->fitToContent(width, height);
        self->vertical_needs_fit = false;
    }
    self->vertical_renderer->render(cr, width, height);
    self->vertical_render_settings = self->vertical_renderer->getSettings();
}

static void on_add_project_point(GtkButton* /*button*/, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    WellEntry* well = get_selected_well(self);
    if (!well) {
        update_status(self, "Сначала выберите скважину");
        return;
    }

    ProjectPoint pp;
    pp.name = "Точка " + std::to_string(well->project_points.size() + 1);
    pp.azimuth_geographic = Degrees{0.0};
    pp.shift = Meters{0.0};
    pp.radius = Meters{50.0};

    if (well->result.has_value() && !well->result->points.empty()) {
        pp.depth = well->result->points.back().depth;
    } else if (!well->source_data.measurements.empty()) {
        pp.depth = well->source_data.measurements.back().depth;
    }

    well->project_points.push_back(pp);
    if (well->result.has_value()) {
        well->result->project_points = well->project_points;
        interpolateProjectPoints(*well->result);
    }

    self->project_modified = true;
    refresh_project_points_list(self);
    main_window_update_views(self);
    update_status(self, "Проектная точка добавлена");
}

static void on_remove_project_point(GtkButton* /*button*/, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    WellEntry* well = get_selected_well(self);
    if (!well) {
        update_status(self, "Сначала выберите скважину");
        return;
    }

    GtkListBoxRow* row = gtk_list_box_get_selected_row(GTK_LIST_BOX(self->project_points_list));
    if (!row) {
        update_status(self, "Выберите проектную точку для удаления");
        return;
    }

    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(row), "pp-index"));
    if (idx < 0 || static_cast<size_t>(idx) >= well->project_points.size()) {
        update_status(self, "Проектная точка не найдена");
        return;
    }

    well->project_points.erase(well->project_points.begin() + idx);
    if (well->result.has_value()) {
        well->result->project_points = well->project_points;
        if (!well->result->project_points.empty()) {
            interpolateProjectPoints(*well->result);
        }
    }

    self->project_modified = true;
    refresh_project_points_list(self);
    main_window_update_views(self);
    update_status(self, "Проектная точка удалена");
}

struct SaveDialogContext {
    std::optional<std::filesystem::path> selected;
    bool completed = false;
};

static std::optional<std::filesystem::path> prompt_save_path(GtkWindow* parent) {
    GtkFileDialog* dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Сохранить проект");

    GtkFileFilter* filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Проект Incline3D (*.inclproj)");
    gtk_file_filter_add_pattern(filter, "*.inclproj");

    GListStore* filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    g_object_unref(filter);
    g_object_unref(filters);

    SaveDialogContext ctx;
    gtk_file_dialog_save(dialog, parent, nullptr,
        [](GObject* source, GAsyncResult* result, gpointer data) {
            auto* dlg = GTK_FILE_DIALOG(source);
            auto* context = static_cast<SaveDialogContext*>(data);
            GError* error = nullptr;
            GFile* file = gtk_file_dialog_save_finish(dlg, result, &error);
            if (file) {
                char* path = g_file_get_path(file);
                if (path) {
                    context->selected = std::filesystem::path(path);
                    g_free(path);
                }
                g_object_unref(file);
            }
            if (error) {
                g_error_free(error);
            }
            context->completed = true;
        }, &ctx);

    while (!ctx.completed) {
        while (g_main_context_pending(nullptr)) {
            g_main_context_iteration(nullptr, TRUE);
        }
    }

    g_object_unref(dialog);
    return ctx.selected;
}

static void apply_trajectory_renderer_settings(InclineMainWindow* self) {
    TrajectoryRenderSettings traj;
    traj.line_width = self->axonometry_settings.trajectory_line_width;
    traj.show_depth_labels = self->axonometry_settings.show_depth_labels;
    traj.depth_label_interval = self->axonometry_settings.depth_label_interval;
    self->trajectory_renderer->setTrajectorySettings(traj);

    GridSettings grid;
    grid.show_horizontal = self->axonometry_settings.show_grid_horizontal;
    grid.show_vertical = self->axonometry_settings.show_grid_vertical;
    grid.show_plan = self->axonometry_settings.show_grid_plan;
    grid.grid_interval = self->axonometry_settings.grid_interval;
    grid.horizontal_depth = self->axonometry_settings.grid_horizontal_depth;
    grid.grid_color = self->axonometry_settings.grid_color;
    self->trajectory_renderer->setGridSettings(grid);

    SceneSettings scene;
    scene.show_axes = self->axonometry_settings.show_axes;
    scene.show_sea_level = self->axonometry_settings.show_sea_level;
    scene.sea_level_color = self->axonometry_settings.sea_level_color;
    scene.background_color = self->axonometry_settings.background_color;
    scene.axis_x_color = Color::red();
    scene.axis_y_color = Color::green();
    scene.axis_z_color = Color::blue();
    self->trajectory_renderer->setSceneSettings(scene);
}

static void on_gl_realize(GtkGLArea* area, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) {
        return;
    }
    self->trajectory_renderer->initialize();
    apply_trajectory_renderer_settings(self);
}

static void on_gl_unrealize(GtkGLArea* area, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) {
        return;
    }
    self->trajectory_renderer->cleanup();
}

static void on_gl_resize(GtkGLArea* /*area*/, int width, int height, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    self->trajectory_renderer->setViewportSize(width, height);
    self->camera->setViewportSize(width, height);
    self->axonometry_needs_fit = true;
}

static float compute_fit_zoom(InclineMainWindow* self, int width, int height) {
    auto [min_bound, max_bound] = self->trajectory_renderer->getSceneBounds();
    float span_x = (max_bound.x - min_bound.x) * 0.5f;
    float span_y = (max_bound.y - min_bound.y) * 0.5f;
    float span_z = (max_bound.z - min_bound.z) * 0.5f;
    float aspect = static_cast<float>(std::max(1, width)) / static_cast<float>(std::max(1, height));

    float size_needed = 100.0f; // по умолчанию
    size_needed = std::max(size_needed, 2.0f * span_y);
    size_needed = std::max(size_needed, 2.0f * span_x / std::max(0.1f, aspect));
    size_needed = std::max(size_needed, 2.0f * std::abs(span_z));

    float zoom = 1000.0f / std::max(1.0f, size_needed);
    zoom = std::clamp(zoom, Camera::MIN_ZOOM, Camera::MAX_ZOOM);
    return zoom;
}

static gboolean on_gl_render(GtkGLArea* area, GdkGLContext* /*context*/, gpointer user_data) {
    auto* self = INCLINE_MAIN_WINDOW(user_data);
    gtk_gl_area_make_current(area);
    if (gtk_gl_area_get_error(area)) {
        return FALSE;
    }

    GtkAllocation alloc;
    gtk_widget_get_allocation(GTK_WIDGET(area), &alloc);
    self->trajectory_renderer->setViewportSize(alloc.width, alloc.height);
    self->camera->setViewportSize(alloc.width, alloc.height);

    auto bounds = self->trajectory_renderer->getSceneBounds();
    glm::vec3 center = (bounds.first + bounds.second) * 0.5f;
    self->camera->setSceneCenter(center);

    if (self->axonometry_needs_fit) {
        float zoom = compute_fit_zoom(self, alloc.width, alloc.height);
        self->camera->setZoom(zoom);
        self->axonometry_needs_fit = false;
    }

    self->trajectory_renderer->render(*self->camera);
    return TRUE;
}

static gboolean save_project_to_path(InclineMainWindow* self, const std::filesystem::path& path) {
    try {
        if (self->project->name.empty()) {
            self->project->name = path.stem().string();
        }
        sync_processing_to_project(self);
        sync_render_settings_to_project(self);
        saveProject(*self->project, path);
        self->project->file_path = path.string();
        self->project_path = path.string();
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

static void on_new_project(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);

    // TODO: Проверить несохранённые изменения

    delete self->project;
    self->project = new Project();
    self->project_path.clear();
    self->project_modified = false;
    self->selected_well_id.clear();
    sync_processing_from_project(self);
    sync_render_settings_from_project(self);

    refresh_wells_list(self);
    main_window_update_views(self);

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

static void on_open_diagnostics(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);
    incline::ui::showDiagnosticsDialog(GTK_WINDOW(self), self->project);
}

static void on_open_analyses(GSimpleAction* /*action*/, GVariant* /*parameter*/, gpointer user_data) {
    InclineMainWindow* self = INCLINE_MAIN_WINDOW(user_data);
    incline::ui::showAnalysesDialog(GTK_WINDOW(self), self->project);
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
        sync_render_settings_from_project(self);
        self->selected_well_id.clear();

        refresh_wells_list(self);
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
        auto selected = prompt_save_path(GTK_WINDOW(self));
        if (!selected.has_value()) {
            update_status(self, "Сохранение отменено");
            return FALSE;
        }
        return save_project_to_path(self, *selected);
    }

    return save_project_to_path(self, std::filesystem::path(self->project_path));
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
    entry.id = self->project->generateWellId();
    entry.source_data = std::move(result.data);
    entry.visible = true;
    entry.color = Color::fromHex("#0000FF");  // Синий по умолчанию
    if (self->project->wells.empty()) {
        entry.is_base = true;
    }

    self->project->wells.push_back(std::move(entry));
    self->selected_well_id = self->project->wells.back().id;
    self->project_modified = true;

    refresh_wells_list(self);
    main_window_update_views(self);
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
        entry.result->project_points = entry.project_points;
        if (!entry.result->project_points.empty()) {
            interpolateProjectPoints(*entry.result);
        }
    }

    self->project_modified = true;
    self->plan_needs_fit = true;
    self->vertical_needs_fit = true;
    main_window_update_views(self);
    refresh_wells_list(self);
    refresh_project_points_list(self);
    update_title(self);
    update_status(self, "Обработка завершена");
}

Project* main_window_get_project(InclineMainWindow* self) {
    return self->project;
}

void main_window_update_views(InclineMainWindow* self) {
    self->plan_renderer->setSettings(self->plan_render_settings);
    self->vertical_renderer->setSettings(self->vertical_render_settings);
    // Обновляем рендереры
    self->plan_renderer->updateFromProject(*self->project);
    self->vertical_renderer->updateFromProject(*self->project);
    self->trajectory_renderer->updateFromProject(*self->project);
    self->plan_needs_fit = true;
    self->vertical_needs_fit = true;
    self->axonometry_needs_fit = true;

    // Перерисовываем виджеты
    gtk_widget_queue_draw(self->plan_view);
    gtk_widget_queue_draw(self->vertical_view);
    gtk_gl_area_queue_render(GTK_GL_AREA(self->axonometry_view));
}
