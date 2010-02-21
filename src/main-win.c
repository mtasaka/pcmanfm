/*
 *      main-win.c
 *      
 *      Copyright 2009 PCMan <pcman.tw@gmail.com>
 *      
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *      
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *      
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *      MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>

#include "pcmanfm.h"

#include "app-config.h"
#include "main-win.h"
#include "pref.h"

static void fm_main_win_finalize              (GObject *object);
G_DEFINE_TYPE(FmMainWin, fm_main_win, GTK_TYPE_WINDOW);

static GtkWidget* create_tab_label(FmMainWin* win, FmPath* path, FmFolderView* view);
static void update_tab_label(FmMainWin* win, FmFolderView* fv, const char* title);
static void update_volume_info(FmMainWin* win);

static void on_focus_in(GtkWidget* w, GdkEventFocus* evt);
static gboolean on_delete_event(GtkWidget* w, GdkEvent* evt);

static void on_new_win(GtkAction* act, FmMainWin* win);
static void on_new_tab(GtkAction* act, FmMainWin* win);
static void on_close_tab(GtkAction* act, FmMainWin* win);
static void on_close_win(GtkAction* act, FmMainWin* win);

static void on_open_in_new_tab(GtkAction* act, FmMainWin* win);
static void on_open_in_new_win(GtkAction* act, FmMainWin* win);

static void on_cut(GtkAction* act, FmMainWin* win);
static void on_copy(GtkAction* act, FmMainWin* win);
static void on_copy_to(GtkAction* act, FmMainWin* win);
static void on_move_to(GtkAction* act, FmMainWin* win);
static void on_paste(GtkAction* act, FmMainWin* win);
static void on_del(GtkAction* act, FmMainWin* win);
static void on_rename(GtkAction* act, FmMainWin* win);

static void on_select_all(GtkAction* act, FmMainWin* win);
static void on_invert_select(GtkAction* act, FmMainWin* win);
static void on_preference(GtkAction* act, FmMainWin* win);

static void on_add_bookmark(GtkAction* act, FmMainWin* win);

static void on_go(GtkAction* act, FmMainWin* win);
static void on_go_back(GtkAction* act, FmMainWin* win);
static void on_go_forward(GtkAction* act, FmMainWin* win);
static void on_go_up(GtkAction* act, FmMainWin* win);
static void on_go_home(GtkAction* act, FmMainWin* win);
static void on_go_desktop(GtkAction* act, FmMainWin* win);
static void on_go_trash(GtkAction* act, FmMainWin* win);
static void on_go_computer(GtkAction* act, FmMainWin* win);
static void on_go_network(GtkAction* act, FmMainWin* win);
static void on_go_apps(GtkAction* act, FmMainWin* win);
static void on_show_hidden(GtkToggleAction* act, FmMainWin* win);
static void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win);
static void on_about(GtkAction* act, FmMainWin* win);
static void on_open_in_terminal(GtkAction* act, FmMainWin* win);
static void on_open_as_root(GtkAction* act, FmMainWin* win);

static void on_location(GtkAction* act, FmMainWin* win);

static void on_create_new(GtkAction* action, FmMainWin* win);
static void on_prop(GtkAction* action, FmMainWin* win);

static void on_switch_page(GtkNotebook* nb, GtkNotebookPage* page, guint num, FmMainWin* win);
static void on_page_removed(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win);

#include "main-win-ui.c" /* ui xml definitions and actions */

static GQuark nav_history_id = 0;
static GSList* all_wins = NULL;

static void fm_main_win_class_init(FmMainWinClass *klass)
{
    GObjectClass *g_object_class;
    GtkWidgetClass* widget_class;
    g_object_class = G_OBJECT_CLASS(klass);
    g_object_class->finalize = fm_main_win_finalize;

    widget_class = (GtkWidgetClass*)klass;
    widget_class->focus_in_event = on_focus_in;
    widget_class->delete_event = on_delete_event;

    fm_main_win_parent_class = (GtkWindowClass*)g_type_class_peek(GTK_TYPE_WINDOW);

    /* special style used by close button */
	gtk_rc_parse_string(
		"style \"close-btn-style\" {\n"
            "GtkWidget::focus-padding = 0\n"
            "GtkWidget::focus-line-width = 0\n"
            "xthickness = 0\n"
            "ythickness = 0\n"
		"}\n"
		"widget \"*.close-btn\" style \"close-btn-style\""
	);

    nav_history_id = g_quark_from_static_string("nav-history");
}

static void on_entry_activate(GtkEntry* entry, FmMainWin* self)
{
    fm_folder_view_chdir_by_name(self->folder_view, gtk_entry_get_text(entry));
}

static void on_view_loaded( FmFolderView* view, FmPath* path, gpointer user_data) 
{
    FmMainWin* win = FM_MAIN_WIN(user_data);
    FmIcon* icon;
    const FmNavHistoryItem* item;
    FmNavHistory* nh;

    /* FIXME: we shouldn't access private data member directly. */
    fm_path_entry_set_model( win->location, path, view->model );
    if(FM_FOLDER_MODEL(view->model)->dir->dir_fi)
    {
        icon = FM_FOLDER_MODEL(view->model)->dir->dir_fi->icon;
        if(icon)
        {
            icon->gicon;
            /* FIXME: load icon. we need to change window icon when switching pages. */
            gtk_window_set_icon_name(win, "folder");
        }
    }
    update_volume_info(win);

    /* scroll to recorded position */
    nh = (FmNavHistory*)g_object_get_qdata(view, nav_history_id);
    item = fm_nav_history_get_cur(nh);
    gtk_adjustment_set_value( gtk_scrolled_window_get_vadjustment(view), item->scroll_pos);
}

static gboolean open_folder_func(GAppLaunchContext* ctx, GList* folder_infos, gpointer user_data, GError** err)
{
    FmMainWin* win = FM_MAIN_WIN(user_data);
    GList* l = folder_infos;
    FmFileInfo* fi = (FmFileInfo*)l->data;
    fm_main_win_chdir(win, fi->path);
    l=l->next;
    for(; l; l=l->next)
    {
        FmFileInfo* fi = (FmFileInfo*)l->data;
        fm_main_win_add_tab(win, fi->path);
    }
    return TRUE;
}

static void on_file_clicked(FmFolderView* fv, FmFolderViewClickType type, FmFileInfo* fi, FmMainWin* win)
{
    char* fpath, *uri;
    GAppLaunchContext* ctx;
    switch(type)
    {
    case FM_FV_ACTIVATED: /* file activated */
        if(fm_file_info_is_dir(fi))
        {
            fm_main_win_chdir( win, fi->path);
        }
        else if(!fm_file_info_is_symlink(fi) && fi->target) /* FIXME: use accessor functions. */
        {
            /* symlinks also has fi->target, but we only handle shortcuts here. */
            if(fm_file_info_is_dir(fi))
                fm_main_win_chdir_by_name( win, fi->target);
            else
            {
                /* FIXME: items under applications:/// are not working... */
                /* fm_launch_file_simple(win, NULL, fi, open_folder_func, win); */
            }
        }
        else
        {
            fm_launch_file_simple(win, NULL, fi, open_folder_func, win);
        }
        break;
    case FM_FV_CONTEXT_MENU:
        if(fi)
        {
            FmFileMenu* menu;
            GtkMenu* popup;
            FmFileInfoList* files = fm_folder_view_get_selected_files(fv);
            menu = fm_file_menu_new_for_files(files, TRUE);
            fm_file_menu_set_folder_func(menu, open_folder_func, win);
            fm_list_unref(files);

            /* merge some specific menu items for folders */
            if(fm_file_menu_is_single_file_type(menu) && fm_file_info_is_dir(fi))
            {
                GtkUIManager* ui = fm_file_menu_get_ui(menu);
                GtkActionGroup* act_grp = fm_file_menu_get_action_group(menu);
                gtk_action_group_set_translation_domain(act_grp, NULL);
                gtk_action_group_add_actions(act_grp, folder_menu_actions, G_N_ELEMENTS(folder_menu_actions), win);
                gtk_ui_manager_add_ui_from_string(ui, folder_menu_xml, -1, NULL);
            }

            popup = fm_file_menu_get_menu(menu);
            gtk_menu_popup(popup, NULL, NULL, NULL, fi, 3, gtk_get_current_event_time());
        }
        else /* no files are selected. Show context menu of current folder. */
        {
            gtk_menu_popup(win->popup, NULL, NULL, NULL, NULL, 3, gtk_get_current_event_time());
        }
        break;
    case FM_FV_MIDDLE_CLICK:
        g_debug("middle click!");
        break;
    }
}

static void on_sel_changed(FmFolderView* fv, FmFileInfoList* files, FmMainWin* win)
{
	/* popup previous message if there is any */
	gtk_statusbar_pop(win->statusbar, win->statusbar_ctx2);
    if(files)
    {
        char* msg;
        /* FIXME: display total size of all selected files. */
        if(fm_list_get_length(files) == 1) /* only one file is selected */
        {
            FmFileInfo* fi = fm_list_peek_head(files);
            const char* size_str = fm_file_info_get_disp_size(fi);
            if(size_str)
            {
                msg = g_strdup_printf("\"%s\" (%s) %s", 
                            fm_file_info_get_disp_name(fi),
                            size_str ? size_str : "",
                            fm_file_info_get_desc(fi));
            }
            else
            {
                msg = g_strdup_printf("\"%s\" %s", 
                            fm_file_info_get_disp_name(fi),
                            fm_file_info_get_desc(fi));
            }
        }
        else
            msg = g_strdup_printf("%d items selected", fm_list_get_length(files));
        gtk_statusbar_push(win->statusbar, win->statusbar_ctx2, msg);
        g_free(msg);
    }
}

static gboolean on_view_key_press_event(FmFolderView* fv, GdkEventKey* evt, FmMainWin* win)
{
    switch(evt->keyval)
    {
    case GDK_BackSpace:
        on_go_up(NULL, win);
        break;
    case GDK_Delete:
        on_del(NULL, win);
        break;
    }
    return FALSE;
}

static void on_status(FmFolderView* fv, const char* msg, FmMainWin* win)
{
    gtk_statusbar_pop(win->statusbar, win->statusbar_ctx);
    gtk_statusbar_push(win->statusbar, win->statusbar_ctx, msg);
}

static void on_bookmark(GtkMenuItem* mi, FmMainWin* win)
{
    FmPath* path = (FmPath*)g_object_get_data(mi, "path");
    switch(FM_APP_CONFIG(fm_config)->bm_open_method)
    {
    case 0: /* current tab */
        fm_main_win_chdir(win, path);
        break;
    case 1: /* new tab */
        fm_main_win_add_tab(win, path);
        break;
    case 2: /* new window */
        fm_main_win_add_win(win, path);
        break;
    }
}

static void create_bookmarks_menu(FmMainWin* win)
{
    GList* l;
    int i = 0;
    /* FIXME: direct access to data member is not allowed */
    for(l=win->bookmarks->items;l;l=l->next)
    {
        FmBookmarkItem* item = (FmBookmarkItem*)l->data;
        GtkWidget* mi = gtk_image_menu_item_new_with_label(item->name);
        gtk_widget_show(mi);
        // gtk_image_menu_item_set_image(); // FIXME: set icons for menu items
        g_object_set_data_full(mi, "path", fm_path_ref(item->path), (GDestroyNotify)fm_path_unref);
        g_signal_connect(mi, "activate", G_CALLBACK(on_bookmark), win);
        gtk_menu_shell_insert(win->bookmarks_menu, mi, i);
        ++i;
    }
    if(i > 0)
        gtk_menu_shell_insert(win->bookmarks_menu, gtk_separator_menu_item_new(), i);
}

static void on_bookmarks_changed(FmBookmarks* bm, FmMainWin* win)
{
    /* delete old items first. */
    GList* mis = gtk_container_get_children(win->bookmarks_menu);
    GList* l;
    for(l = mis;l;l=l->next)
    {
        GtkWidget* item = (GtkWidget*)l->data;
        if( GTK_IS_SEPARATOR_MENU_ITEM(item) )
            break;
        gtk_widget_destroy(item);
    }
    g_list_free(mis);

    create_bookmarks_menu(win);
}

static void load_bookmarks(FmMainWin* win, GtkUIManager* ui)
{
    GtkWidget* mi = gtk_ui_manager_get_widget(ui, "/menubar/BookmarksMenu");
    win->bookmarks_menu = gtk_menu_item_get_submenu(mi);
    win->bookmarks = fm_bookmarks_get();
    g_signal_connect(win->bookmarks, "changed", G_CALLBACK(on_bookmarks_changed), win);

    create_bookmarks_menu(win);
}

static void on_history_item(GtkMenuItem* mi, FmMainWin* win)
{
    GList* l = g_object_get_data(mi, "path");
    const FmNavHistoryItem* item = (FmNavHistoryItem*)l->data;
    int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(win->folder_view));
    fm_nav_history_jump(win->nav_history, l, scroll_pos);
    item = fm_nav_history_get_cur(win->nav_history);
    /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
    fm_main_win_chdir_without_history(win, item->path);

    /* scroll to recorded position */
    gtk_adjustment_set_value( gtk_scrolled_window_get_vadjustment(win->folder_view), item->scroll_pos);
}

static void on_show_history_menu(GtkMenuToolButton* btn, FmMainWin* win)
{
    GtkMenuShell* menu = (GtkMenuShell*)gtk_menu_tool_button_get_menu(btn);
    GList* l;
    GList* cur = fm_nav_history_get_cur_link(win->nav_history);

    /* delete old items */
    gtk_container_foreach(menu, (GtkCallback)gtk_widget_destroy, NULL);

    for(l = fm_nav_history_list(win->nav_history); l; l=l->next)
    {
        const FmNavHistoryItem* item = (FmNavHistoryItem*)l->data;
        FmPath* path = item->path;
        char* str = fm_path_display_name(path, TRUE);
        GtkMenuItem* mi;
        if( l == cur )
        {
            mi = gtk_check_menu_item_new_with_label(str);
            gtk_check_menu_item_set_draw_as_radio(mi, TRUE);
            gtk_check_menu_item_set_active(mi, TRUE);
        }
        else
            mi = gtk_menu_item_new_with_label(str);
        g_free(str);

        g_object_set_data_full(mi, "path", l, NULL);
        g_signal_connect(mi, "activate", G_CALLBACK(on_history_item), win);
        gtk_menu_shell_append(menu, mi);
    }
    gtk_widget_show_all( GTK_WIDGET(menu) );
}

static void fm_main_win_init(FmMainWin *self)
{
    GtkWidget *vbox, *menubar, *toolitem, *next_btn, *scroll;
    GtkUIManager* ui;
    GtkActionGroup* act_grp;
    GtkAction* act;
    GtkAccelGroup* accel_grp;
    GtkShadowType shadow_type;

    pcmanfm_ref();
    all_wins = g_slist_prepend(all_wins, self);

    vbox = gtk_vbox_new(FALSE, 0);

    self->hpaned = gtk_hpaned_new();
    gtk_paned_set_position(self->hpaned, 150);

    /* places left pane */
    self->places_view = fm_places_view_new();
    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(scroll, GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_container_add(scroll, self->places_view);
    gtk_paned_add1(self->hpaned, scroll);
    g_signal_connect_swapped(self->places_view, "chdir", G_CALLBACK(fm_main_win_chdir), self);

    /* notebook right pane */
    self->notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(self->notebook, TRUE);
    g_signal_connect(self->notebook, "switch-page", G_CALLBACK(on_switch_page), self);
    g_signal_connect(self->notebook, "page-removed", G_CALLBACK(on_page_removed), self);
    gtk_paned_add2(self->hpaned, self->notebook);

    /* create menu bar and toolbar */
    ui = gtk_ui_manager_new();
    act_grp = gtk_action_group_new("Main");
    gtk_action_group_set_translation_domain(act_grp, NULL);
    gtk_action_group_add_actions(act_grp, main_win_actions, G_N_ELEMENTS(main_win_actions), self);
    /* FIXME: this is so ugly */
    main_win_toggle_actions[0].is_active = app_config->show_hidden;
    gtk_action_group_add_toggle_actions(act_grp, main_win_toggle_actions, G_N_ELEMENTS(main_win_toggle_actions), self);
    gtk_action_group_add_radio_actions(act_grp, main_win_mode_actions, G_N_ELEMENTS(main_win_mode_actions), app_config->view_mode, on_change_mode, self);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_type_actions, G_N_ELEMENTS(main_win_sort_type_actions), app_config->sort_type, on_sort_type, self);
    gtk_action_group_add_radio_actions(act_grp, main_win_sort_by_actions, G_N_ELEMENTS(main_win_sort_by_actions), app_config->sort_by, on_sort_by, self);

    accel_grp = gtk_ui_manager_get_accel_group(ui);
    gtk_window_add_accel_group(self, accel_grp);

    gtk_ui_manager_insert_action_group(ui, act_grp, 0);
    gtk_ui_manager_add_ui_from_string(ui, main_menu_xml, -1, NULL);

    menubar = gtk_ui_manager_get_widget(ui, "/menubar");
    self->toolbar = gtk_ui_manager_get_widget(ui, "/toolbar");
    /* FIXME: should make these optional */
    gtk_toolbar_set_icon_size(self->toolbar, GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_toolbar_set_style(self->toolbar, GTK_TOOLBAR_ICONS);

    /* create 'Next' button manually and add a popup menu to it */
    toolitem = g_object_new(GTK_TYPE_MENU_TOOL_BUTTON, NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(self->toolbar), toolitem, 2);
    gtk_widget_show(GTK_WIDGET(toolitem));
    act = gtk_ui_manager_get_action(ui, "/menubar/GoMenu/Next");
    gtk_activatable_set_related_action(toolitem, act);

    /* set up history menu */
    self->history_menu = gtk_menu_new();
    gtk_menu_tool_button_set_menu(toolitem, self->history_menu);
    g_signal_connect(toolitem, "show-menu", G_CALLBACK(on_show_history_menu), self);

    self->popup = gtk_ui_manager_get_widget(ui, "/popup");

    gtk_box_pack_start( (GtkBox*)vbox, menubar, FALSE, TRUE, 0 );
    gtk_box_pack_start( (GtkBox*)vbox, self->toolbar, FALSE, TRUE, 0 );

    /* load bookmarks menu */
    load_bookmarks(self, ui);

    /* the location bar */
    self->location = fm_path_entry_new();
    g_signal_connect(self->location, "activate", on_entry_activate, self);

    toolitem = gtk_tool_item_new();
    gtk_container_add( toolitem, self->location );
    gtk_tool_item_set_expand(toolitem, TRUE);
    gtk_toolbar_insert((GtkToolbar*)self->toolbar, toolitem, gtk_toolbar_get_n_items(self->toolbar) - 1 );

    gtk_box_pack_start( (GtkBox*)vbox, self->hpaned, TRUE, TRUE, 0 );

    /* status bar */
    self->statusbar = gtk_statusbar_new();
    /* status bar column showing volume free space */
    gtk_widget_style_get(self->statusbar, "shadow-type", &shadow_type, NULL);
    self->vol_status = gtk_frame_new(NULL);
    gtk_frame_set_shadow_type(self->vol_status, shadow_type);
    gtk_box_pack_start(self->statusbar, self->vol_status, FALSE, TRUE, 0);
    gtk_container_add(self->vol_status, gtk_label_new(NULL));

    gtk_box_pack_start( (GtkBox*)vbox, self->statusbar, FALSE, TRUE, 0 );
    self->statusbar_ctx = gtk_statusbar_get_context_id(self->statusbar, "status");
    self->statusbar_ctx2 = gtk_statusbar_get_context_id(self->statusbar, "status2");

    g_object_unref(act_grp);
    self->ui = ui;

    gtk_container_add( (GtkContainer*)self, vbox );
    gtk_widget_show_all(vbox);

    /* create new tab */
    fm_main_win_add_tab(self, fm_path_get_home());
    gtk_widget_grab_focus(self->folder_view);
}


GtkWidget* fm_main_win_new(void)
{
    return (GtkWidget*)g_object_new(FM_MAIN_WIN_TYPE, NULL);
}


static void fm_main_win_finalize(GObject *object)
{
    FmMainWin *self;

    g_return_if_fail(object != NULL);
    g_return_if_fail(IS_FM_MAIN_WIN(object));

    self = FM_MAIN_WIN(object);

    if(self->vol_status_cancellable)
        g_object_unref(self->vol_status_cancellable);

    g_object_unref(self->ui);
    g_object_unref(self->bookmarks);

    if (G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)
        (* G_OBJECT_CLASS(fm_main_win_parent_class)->finalize)(object);

    pcmanfm_unref();
}

void on_about(GtkAction* act, FmMainWin* win)
{
    GtkWidget* dlg;
    GtkBuilder* builder = gtk_builder_new();
    gtk_builder_add_from_file(builder, PACKAGE_UI_DIR "/about.ui", NULL);
    dlg = (GtkWidget*)gtk_builder_get_object(builder, "dlg");
    g_object_unref(builder);
    gtk_dialog_run((GtkDialog*)dlg);
    gtk_widget_destroy(dlg);
}

void on_open_in_terminal(GtkAction* act, FmMainWin* win)
{
    GAppInfo* app;
    char* cmd;
    char** argv;
    int argc;
    if(!fm_config->terminal)
    {
        fm_show_error(win, _("Terminal emulator is not set."));
        fm_edit_preference(win, PREF_ADVANCED);
        return;
    }
    if(!g_shell_parse_argv(fm_config->terminal, &argc, &argv, NULL))
        return;
    app = g_app_info_create_from_commandline(argv[0], NULL, 0, NULL);
    g_strfreev(argv);
    if(app)
    {
        const FmNavHistoryItem* item = fm_nav_history_get_cur(win->nav_history);
        FmPath* cwd = item->path;
        GError* err = NULL;
        GAppLaunchContext* ctx = gdk_app_launch_context_new();
        char* cwd_str;

        if(fm_path_is_native(cwd))
            cwd_str = fm_path_to_str(cwd);
        else
        {
            GFile* gf = fm_path_to_gfile(cwd);
            cwd_str = g_file_get_path(gf);
            g_object_unref(gf);
        }
        gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(win));
        gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
        g_chdir(cwd_str); /* FIXME: currently we don't have better way for this. maybe a wrapper script? */
        g_free(cwd_str);
        if(!g_app_info_launch(app, NULL, ctx, &err))
        {
            fm_show_error(win, err->message);
            g_error_free(err);
        }
        g_object_unref(ctx);
        g_object_unref(app);
    }
}

void on_open_as_root(GtkAction* act, FmMainWin* win)
{
    GAppInfo* app;
    char* cmd;
    char** argv;
    int argc;
    if(!app_config->su_cmd)
    {
        fm_show_error(win, _("Switch user command is not set."));
        fm_edit_preference(win, PREF_ADVANCED);
        return;
    }
    if(strstr(app_config->su_cmd, "%s")) /* FIXME: need to rename to pcmanfm when we reach stable release. */
        cmd = g_strdup_printf(app_config->su_cmd, "pcmanfm2 %U");
    else
        cmd = g_strconcat(app_config->su_cmd, " ", "pcmanfm2 %U", NULL);
    app = g_app_info_create_from_commandline(cmd, NULL, 0, NULL);
    g_free(cmd);
    if(app)
    {
        const FmNavHistoryItem* item = fm_nav_history_get_cur(win->nav_history);
        FmPath* cwd = item->path;
        GError* err = NULL;
        GAppLaunchContext* ctx = gdk_app_launch_context_new();
        char* uri = fm_path_to_uri(cwd);
        GList* uris = g_list_prepend(NULL, uri);
        gdk_app_launch_context_set_screen(ctx, gtk_widget_get_screen(win));
        gdk_app_launch_context_set_timestamp(ctx, gtk_get_current_event_time());
        if(!g_app_info_launch_uris(app, uris, ctx, &err))
        {
            fm_show_error(win, err->message);
            g_error_free(err);
        }
        g_list_free(uris);
        g_free(uri);
        g_object_unref(ctx);
        g_object_unref(app);
    }
}

void on_show_hidden(GtkToggleAction* act, FmMainWin* win)
{
    gboolean active = gtk_toggle_action_get_active(act);
    fm_folder_view_set_show_hidden( win->folder_view, active );
    app_config->show_hidden = active;
}

void on_change_mode(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int mode = gtk_radio_action_get_current_value(cur);
    fm_folder_view_set_mode( win->folder_view, mode );
}

void on_sort_by(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int val = gtk_radio_action_get_current_value(cur);
    fm_folder_view_sort(win->folder_view, -1, val);
    app_config->sort_by = val;
}

void on_sort_type(GtkRadioAction* act, GtkRadioAction *cur, FmMainWin* win)
{
    int val = gtk_radio_action_get_current_value(cur);
    fm_folder_view_sort(win->folder_view, val, -1);
    app_config->sort_type = val;
}

void on_focus_in(GtkWidget* w, GdkEventFocus* evt)
{
    if(all_wins->data != w)
    {
        all_wins = g_slist_remove(all_wins, w);
        all_wins = g_slist_prepend(all_wins, w);
    }
    ((GtkWidgetClass*)fm_main_win_parent_class)->focus_in_event(w, evt);
}

gboolean on_delete_event(GtkWidget* w, GdkEvent* evt)
{
    FmMainWin* win = (FmMainWin*)w;
    /* store the size of last used window in config. */
    gtk_window_get_size(w, &app_config->win_width, &app_config->win_height);
    all_wins = g_slist_remove(all_wins, win);
//    ((GtkWidgetClass*)fm_main_win_parent_class)->delete_event(w, evt);
    return FALSE;
}

void on_new_win(GtkAction* act, FmMainWin* win)
{
    fm_main_win_add_win(win, fm_path_get_home());
}

void on_new_tab(GtkAction* act, FmMainWin* win)
{
    FmPath* path = fm_folder_view_get_cwd(win->folder_view);
    fm_main_win_add_tab(win, path);
}

void on_close_win(GtkAction* act, FmMainWin* win)
{
    gtk_widget_destroy(win);
}

void on_close_tab(GtkAction* act, FmMainWin* win)
{
    /* FIXME: this is a little bit dirty */
    GtkNotebook* nb = (GtkNotebook*)win->notebook;
    gtk_widget_destroy(win->folder_view);

    if(win->vol_status_cancellable)
    {
        g_cancellable_cancel(win->vol_status_cancellable);
        g_object_unref(win->vol_status_cancellable);
        win->vol_status_cancellable = NULL;
    }
    if(gtk_notebook_get_n_pages(nb) == 0)
    {
        GtkWidget* main_win = gtk_widget_get_toplevel(nb);
        gtk_widget_destroy(main_win);
    }
}

void on_open_in_new_tab(GtkAction* act, FmMainWin* win)
{
    FmPathList* sels = fm_folder_view_get_selected_file_paths(win->folder_view);
    GList* l;
    for( l = fm_list_peek_head_link(sels); l; l=l->next )
    {
        FmPath* path = (FmPath*)l->data;
        fm_main_win_add_tab(win, path);
    }
    fm_list_unref(sels);
}


void on_open_in_new_win(GtkAction* act, FmMainWin* win)
{
    FmPathList* sels = fm_folder_view_get_selected_file_paths(win->folder_view);
    GList* l;
    for( l = fm_list_peek_head_link(sels); l; l=l->next )
    {
        FmPath* path = (FmPath*)l->data;
        fm_main_win_add_win(win, path);
    }
    fm_list_unref(sels);
}


void on_go(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, gtk_entry_get_text(win->location));
}

void on_go_back(GtkAction* act, FmMainWin* win)
{
    if(fm_nav_history_get_can_back(win->nav_history))
    {
        FmNavHistoryItem* item;
        int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(win->folder_view));
        fm_nav_history_back(win->nav_history, scroll_pos);
        item = fm_nav_history_get_cur(win->nav_history);
        /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
        fm_main_win_chdir_without_history(win, item->path);

        /* scroll to recorded position */
        gtk_adjustment_set_value( gtk_scrolled_window_get_vadjustment(win->folder_view), item->scroll_pos);
    }
}

void on_go_forward(GtkAction* act, FmMainWin* win)
{
    if(fm_nav_history_get_can_forward(win->nav_history))
    {
        FmNavHistoryItem* item;
        int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(win->folder_view));
        fm_nav_history_forward(win->nav_history, scroll_pos);
        /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
        item = fm_nav_history_get_cur(win->nav_history);
        /* FIXME: should this be driven by a signal emitted on FmNavHistory? */
        fm_main_win_chdir_without_history(win, item->path);

        /* scroll to recorded position */
        gtk_adjustment_set_value( gtk_scrolled_window_get_vadjustment(win->folder_view), item->scroll_pos);
    }
}

void on_go_up(GtkAction* act, FmMainWin* win)
{
    FmPath* parent = fm_path_get_parent(fm_folder_view_get_cwd(win->folder_view));
    if(parent)
        fm_main_win_chdir( win, parent);
}

void on_go_home(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, g_get_home_dir());
}

void on_go_desktop(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
}

void on_go_trash(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "trash:///");
}

void on_go_computer(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "computer:///");
}

void on_go_network(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "network:///");
}

void on_go_apps(GtkAction* act, FmMainWin* win)
{
    fm_main_win_chdir_by_name( win, "applications:///");
}

void fm_main_win_chdir_by_name(FmMainWin* win, const char* path_str)
{
    FmPath* path = fm_path_new(path_str);
    fm_main_win_chdir(win, path);
    fm_path_unref(path);
}

void fm_main_win_chdir_without_history(FmMainWin* win, FmPath* path)
{
    /* FIXME: how to handle UTF-8 here? */
    char* disp_path = fm_path_to_str(path);
    char* disp_name = fm_path_display_basename(path);
    gtk_entry_set_text(win->location, disp_path);

    update_tab_label(win, win->folder_view, disp_name);
    gtk_window_set_title(win, disp_name);
    g_free(disp_path);
    g_free(disp_name);

    fm_folder_view_chdir(win->folder_view, path);
    /* fm_nav_history_set_cur(); */
}

void fm_main_win_chdir(FmMainWin* win, FmPath* path)
{
    int scroll_pos = gtk_adjustment_get_value(gtk_scrolled_window_get_vadjustment(win->folder_view));
    fm_nav_history_chdir(win->nav_history, path, scroll_pos);
    fm_main_win_chdir_without_history(win, path);
}

static void close_btn_style_set(GtkWidget *btn, GtkRcStyle *prev, gpointer data)
{
	gint w, h;
	gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(btn), GTK_ICON_SIZE_MENU, &w, &h);
	gtk_widget_set_size_request(btn, w + 2, h + 2);
}

static void on_close_tab_btn(GtkButton* btn, GtkWidget* view)
{
    GtkNotebook* nb = GTK_NOTEBOOK(gtk_widget_get_parent(view));
    FmMainWin* win = FM_MAIN_WIN(gtk_widget_get_toplevel(nb));
    gtk_widget_destroy(view);

    if(win->vol_status_cancellable)
    {
        g_cancellable_cancel(win->vol_status_cancellable);
        g_object_unref(win->vol_status_cancellable);
        win->vol_status_cancellable = NULL;
    }
    if(gtk_notebook_get_n_pages(nb) == 0)
    {
        GtkWidget* main_win = gtk_widget_get_toplevel(nb);
        gtk_widget_destroy(main_win);
    }
}

GtkWidget* create_tab_label(FmMainWin* win, FmPath* path, FmFolderView* view)
{
    GtkWidget * evt_box;
    GtkWidget* tab_label;
    GtkWidget* tab_text;
    GtkWidget* close_btn;
    char* disp_name;
    int w, h;

    /* Create tab label */
    evt_box = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(evt_box), FALSE);

    tab_label = gtk_hbox_new( FALSE, 0 );

    disp_name = g_filename_display_name(path->name);
    tab_text = gtk_label_new(disp_name);
    g_free(disp_name);
    gtk_box_pack_start(GTK_BOX(tab_label), tab_text, FALSE, FALSE, 4 );

    close_btn = gtk_button_new ();
    gtk_button_set_focus_on_click ( GTK_BUTTON ( close_btn ), FALSE );
    gtk_button_set_relief( GTK_BUTTON ( close_btn ), GTK_RELIEF_NONE );
    gtk_container_add ( GTK_CONTAINER ( close_btn ),
                        gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));
    gtk_container_set_border_width(close_btn, 0);
    gtk_widget_set_name(close_btn, "close-btn");
    g_signal_connect(close_btn, "style-set", G_CALLBACK(close_btn_style_set), NULL);

    gtk_box_pack_end( GTK_BOX( tab_label ), close_btn, FALSE, FALSE, 0 );

    g_signal_connect(close_btn, "clicked", G_CALLBACK(on_close_tab_btn), view);

    gtk_container_add(GTK_CONTAINER(evt_box), tab_label);
    gtk_widget_set_events( GTK_WIDGET(evt_box), GDK_ALL_EVENTS_MASK);
/*
    gtk_drag_dest_set ( GTK_WIDGET( evt_box ), GTK_DEST_DEFAULT_ALL,
                        drag_targets,
                        sizeof( drag_targets ) / sizeof( GtkTargetEntry ),
                        GDK_ACTION_DEFAULT | GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK );
    g_signal_connect ( ( gpointer ) evt_box, "drag-motion",
                       G_CALLBACK ( on_tab_drag_motion ),
                       file_browser );
*/
    gtk_widget_show_all(GTK_WIDGET(evt_box));
    return GTK_WIDGET(evt_box);
}

void update_tab_label(FmMainWin* win, FmFolderView* fv, const char* title)
{
    GtkWidget* tab = gtk_notebook_get_tab_label((GtkNotebook*)win->notebook, win->folder_view);
    GtkWidget* hbox = gtk_bin_get_child((GtkBin*)tab);
    GList* children = gtk_container_get_children(hbox);
    GtkWidget* label = (GtkWidget*)children->data;
    g_list_free(children);
    gtk_label_set_text((GtkLabel*)label, title);
}

/* FIXME: remote filesystems are sometimes regarded as local ones. */
static void on_vol_info_available(GObject *src, GAsyncResult *res, FmMainWin* win)
{
    GFileInfo* inf = g_file_query_filesystem_info_finish((GFile*)src, res, NULL);
    guint64 total, free;
    char total_str[ 64 ];
    char free_str[ 64 ];
    char buf[128];
    if(!inf)
        return;
    if(g_file_info_has_attribute(inf, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE))
    {
        total = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        free = g_file_info_get_attribute_uint64(inf, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);

        fm_file_size_to_str(free_str, free, TRUE);
        fm_file_size_to_str(total_str, total, TRUE);
        g_snprintf( buf, G_N_ELEMENTS(buf),
                    _("Free space: %s (Total: %s)"), free_str, total_str );
        gtk_label_set_text(gtk_bin_get_child(win->vol_status), buf);
        gtk_widget_show(win->vol_status);
    }
    else
        gtk_widget_hide(win->vol_status);
    g_object_unref(inf);
}

void update_volume_info(FmMainWin* win)
{
    FmFolderModel* model;
    if(win->vol_status_cancellable)
    {
        g_cancellable_cancel(win->vol_status_cancellable);
        g_object_unref(win->vol_status_cancellable);
        win->vol_status_cancellable = NULL;
    }
    if(win->folder_view && (model = FM_FOLDER_VIEW(win->folder_view)->model))
    {
        /* FIXME: we should't access private data member directly. */
        GFile* gf = model->dir->gf;
        win->vol_status_cancellable = g_cancellable_new();
        g_file_query_filesystem_info_async(gf,
                G_FILE_ATTRIBUTE_FILESYSTEM_SIZE","
                G_FILE_ATTRIBUTE_FILESYSTEM_FREE,
                G_PRIORITY_LOW, win->vol_status_cancellable,
                on_vol_info_available, win);
    }
    else
        gtk_widget_hide(win->vol_status);
}

gint fm_main_win_add_tab(FmMainWin* win, FmPath* path)
{
    /* create label for the tab */
    GtkWidget* label;
    GtkWidget* folder_view;
    gint ret;
    FmNavHistory* nh;

    /* create folder view */
    folder_view = fm_folder_view_new( app_config->view_mode );
    fm_folder_view_set_show_hidden(folder_view, app_config->show_hidden);
    fm_folder_view_sort(folder_view, app_config->sort_type, app_config->sort_by);
    fm_folder_view_set_selection_mode(folder_view, GTK_SELECTION_MULTIPLE);
    g_signal_connect(folder_view, "clicked", on_file_clicked, win);
    g_signal_connect(folder_view, "status", on_status, win);
    g_signal_connect(folder_view, "sel-changed", on_sel_changed, win);
    g_signal_connect(folder_view, "key-press-event", on_view_key_press_event, win);

    nh = fm_nav_history_new();
    g_object_set_qdata_full((GObject*)folder_view, nav_history_id, nh, (GDestroyNotify)g_object_unref);

    fm_folder_view_chdir(folder_view, path);
    fm_nav_history_chdir(nh, path, 0);

    gtk_widget_show(folder_view);

    label = create_tab_label(win, path, folder_view);

    ret = gtk_notebook_append_page(win->notebook, folder_view, label);
    gtk_notebook_set_tab_reorderable(win->notebook, folder_view, TRUE);
    gtk_notebook_set_current_page(win->notebook, ret);

    if(gtk_notebook_get_n_pages(win->notebook) > 1)
        gtk_notebook_set_show_tabs(win->notebook, TRUE);
    else
        gtk_notebook_set_show_tabs(win->notebook, FALSE);

    /* set current folder view */
    win->folder_view = folder_view;
    /* create navigation history */
    win->nav_history = nh;
    return ret;
}


FmMainWin* fm_main_win_add_win(FmMainWin* win, FmPath* path)
{
    win = fm_main_win_new();
    gtk_window_set_default_size(win, app_config->win_width, app_config->win_height);
    fm_main_win_chdir(win, path);
    gtk_window_present(win);
    return win;
}

void on_cut(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) &&
       gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL) )
    {
        gtk_editable_cut_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
        if(files)
        {
            fm_clipboard_cut_files(win, files);
            fm_list_unref(files);
        }
    }
}

void on_copy(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) &&
       gtk_editable_get_selection_bounds((GtkEditable*)focus, NULL, NULL) )
    {
        gtk_editable_copy_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
        if(files)
        {
            fm_clipboard_copy_files(win, files);
            fm_list_unref(files);
        }
    }
}

void on_copy_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_copy_files_to(files);
        fm_list_unref(files);
    }
}

void on_move_to(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
    if(files)
    {
        fm_move_files_to(files);
        fm_list_unref(files);
    }
}

void on_paste(GtkAction* act, FmMainWin* win)
{
    GtkWidget* focus = gtk_window_get_focus((GtkWindow*)win);
    if(GTK_IS_EDITABLE(focus) )
    {
        gtk_editable_paste_clipboard((GtkEditable*)focus);
    }
    else
    {
        FmPath* path = fm_folder_view_get_cwd(win->folder_view);
        fm_clipboard_paste_files(win->folder_view, path);
    }
}

void on_del(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
    GdkModifierType state = 0;
    if(!gtk_get_current_event_state (&state))
        state = 0;
    if( state & GDK_SHIFT_MASK ) /* Shift + Delete = delete directly */
        fm_delete_files(files);
    else
        fm_trash_or_delete_files(files);
    fm_list_unref(files);
}

void on_rename(GtkAction* act, FmMainWin* win)
{
    FmPathList* files = fm_folder_view_get_selected_file_paths(win->folder_view);
    if( !fm_list_is_empty(files) )
    {
        fm_rename_file(fm_list_peek_head(files));
        /* FIXME: is it ok to only rename the first selected file here. */
    }
    fm_list_unref(files);
}

void on_select_all(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_all(win->folder_view);
}

void on_invert_select(GtkAction* act, FmMainWin* win)
{
    fm_folder_view_select_invert(win->folder_view);
}

void on_preference(GtkAction* act, FmMainWin* win)
{
    fm_edit_preference(win, 0);
}

void on_add_bookmark(GtkAction* act, FmMainWin* win)
{
    FmNavHistoryItem* item = fm_nav_history_get_cur(win->nav_history);
    if(item)
    {
        FmPath* cwd = item->path;
        char* disp_path = fm_path_display_name(cwd, TRUE);
        char* msg = g_strdup_printf(_("Add following folder to bookmarks:\n\'%s\'\nEnter a name for the new bookmark item:"), disp_path);
        char* disp_name = fm_path_display_basename(cwd);
        char* name;
        g_free(disp_path);
        name = fm_get_user_input(win, _("Add to Bookmarks"), msg, disp_name);
        g_free(disp_name);
        if(name)
        {
            fm_bookmarks_append(win->bookmarks, cwd, name);
            g_free(name);
        }
    }
}

void on_location(GtkAction* act, FmMainWin* win)
{
    gtk_widget_grab_focus(win->location);
}

void on_prop(GtkAction* action, FmMainWin* win)
{
    FmFolderView* fv = FM_FOLDER_VIEW(win->folder_view);
    /* FIXME: should prevent directly accessing data members */
    FmFileInfo* fi = FM_FOLDER_MODEL(fv->model)->dir->dir_fi;
    FmFileInfoList* files = fm_file_info_list_new();
    fm_list_push_tail(files, fi);
    fm_show_file_properties(files);
    fm_list_unref(files);
}

void on_switch_page(GtkNotebook* nb, GtkNotebookPage* page, guint num, FmMainWin* win)
{
    FmFolderView* fv = FM_FOLDER_VIEW(gtk_notebook_get_nth_page(nb, num));

    if(win->folder_view)
        g_signal_handlers_disconnect_by_func(win->folder_view, on_view_loaded, win);
    win->folder_view = fv;

    if(fv)
    {
        FmPath* cwd = fm_folder_view_get_cwd(fv);
        win->nav_history = (FmNavHistory*)g_object_get_qdata((GObject*)fv, nav_history_id);

        /* FIXME: we shouldn't access private data member. */
        fm_path_entry_set_model( win->location, cwd, fv->model );
        g_signal_connect(fv, "loaded", G_CALLBACK(on_view_loaded), win);

        if(cwd)
        {
            char* disp_name;
            disp_name = g_filename_display_name(cwd->name);
            gtk_window_set_title((GtkWindow*)win, disp_name);
            g_free(disp_name);
        }

        update_volume_info(win);
    }
}

void on_page_removed(GtkNotebook* nb, GtkWidget* page, guint num, FmMainWin* win)
{
    if(gtk_notebook_get_n_pages(nb) > 1)
        gtk_notebook_set_show_tabs(nb, TRUE);
    else
        gtk_notebook_set_show_tabs(nb, FALSE);
}

void on_create_new(GtkAction* action, FmMainWin* win)
{
    FmFolderView* fv = FM_FOLDER_VIEW(win->folder_view);
    const char* name = gtk_action_get_name(action);
    GError* err = NULL;
    FmPath* cwd = fm_folder_view_get_cwd(fv);
    FmPath* dest;
    char* basename;
_retry:
    basename = fm_get_user_input(win, _("Create New..."), _("Enter a name for the newly created file:"), _("New"));
    if(!basename)
        return;

    dest = fm_path_new_child(cwd, basename);
    g_free(basename);

    if( strcmp(name, "NewFolder") == 0 )
    {
        GFile* gf = fm_path_to_gfile(dest);
        if(!g_file_make_directory(gf, NULL, &err))
        {
            if(err->domain = G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(win, err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its 
             * folder's monitor and after FmFolder handles it in idle 
             * handler. So, we cannot select it since it's not yet in 
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else if( strcmp(name, "NewBlank") == 0 )
    {
        GFile* gf = fm_path_to_gfile(dest);
        GFileOutputStream* f = g_file_create(gf, G_FILE_CREATE_NONE, NULL, &err);
        if(f)
        {
            g_output_stream_close(f, NULL, NULL);
            g_object_unref(f);
        }
        else
        {
            if(err->domain = G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
            {
                fm_path_unref(dest);
                g_error_free(err);
                g_object_unref(gf);
                err = NULL;
                goto _retry;
            }
            fm_show_error(win, err->message);
            g_error_free(err);
        }

        if(!err) /* select the newly created file */
        {
            /*FIXME: this doesn't work since the newly created file will
             * only be shown after file-created event was fired on its 
             * folder's monitor and after FmFolder handles it in idle 
             * handler. So, we cannot select it since it's not yet in 
             * the folder model now. */
            /* fm_folder_view_select_file_path(fv, dest); */
        }
        g_object_unref(gf);
    }
    else /* templates */
    {
        
    }
    fm_path_unref(dest);
}

FmMainWin* fm_main_win_get_last_active()
{
    return all_wins ? (FmMainWin*)all_wins->data : NULL;
}

void fm_main_win_open_in_last_active(FmPath* path)
{
    FmMainWin* win = fm_main_win_get_last_active();
    if(!win)
    {
        win = fm_main_win_new();
        gtk_window_set_default_size(win, app_config->win_width, app_config->win_height);
        fm_main_win_chdir(win, path);
    }
    else
        fm_main_win_add_tab(win, path);
    gtk_window_present(win);
}
