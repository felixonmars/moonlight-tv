#include <ui/root.h>
#include <util/user_event.h>
#include "settings.controller.h"

typedef struct {
    const char *icon;
    const char *name;
    const lv_obj_controller_class_t *cls;
} settings_entry_t;

static const settings_entry_t entries[] = {
        {LV_SYMBOL_DUMMY, "Basic Settings",   &settings_pane_basic_cls},
        {LV_SYMBOL_DUMMY, "Host Settings",    &settings_pane_host_cls},
        {LV_SYMBOL_DUMMY, "Input Settings",   &settings_pane_input_cls},
        {LV_SYMBOL_DUMMY, "Decoder Settings", &settings_pane_decoder_cls},
        {LV_SYMBOL_DUMMY, "About",            &settings_pane_about_cls},
};
static const int entries_len = sizeof(entries) / sizeof(settings_entry_t);

static void on_view_created(lv_obj_controller_t *controller, lv_obj_t *view);

static void on_destroy_view(lv_obj_controller_t *controller, lv_obj_t *view);

static void on_entry_focus(lv_event_t *event);

static void on_entry_click(lv_event_t *event);

static void on_nav_key(lv_event_t *event);

static void on_detail_key(lv_event_t *event);

static void on_dropdown_clicked(lv_event_t *event);

static void settings_controller_ctor(lv_obj_controller_t *self, void *args);

static bool on_event(lv_obj_controller_t *self, int which, void *data1, void *data2);

static void detail_defocus(settings_controller_t *controller, lv_event_t *e, bool close_dropdown);

static bool detail_item_needs_lrkey(lv_obj_t *obj);

static void show_pane(settings_controller_t *controller, const lv_obj_controller_class_t *cls);

static void settings_close(lv_event_t *e);

static void restart_confirm_cb(lv_event_t *e);

const lv_obj_controller_class_t settings_controller_cls = {
        .constructor_cb = settings_controller_ctor,
        .create_obj_cb = settings_win_create,
        .obj_created_cb = on_view_created,
        .obj_deleted_cb = on_destroy_view,
        .event_cb = on_event,
        .instance_size = sizeof(settings_controller_t),
};

static void settings_controller_ctor(lv_obj_controller_t *self, void *args) {

}

static void on_view_created(lv_obj_controller_t *self, lv_obj_t *view) {
    settings_controller_t *controller = (settings_controller_t *) self;
    controller->pane_manager = lv_controller_manager_create(controller->detail, self);
    lv_obj_set_child_group(controller->nav, lv_group_create());
    lv_obj_set_child_group(controller->detail, lv_group_create());

    lv_obj_add_event_cb(controller->nav, on_entry_focus, LV_EVENT_FOCUSED, controller);
    lv_obj_add_event_cb(controller->nav, on_entry_click, LV_EVENT_CLICKED, controller);
    lv_obj_add_event_cb(controller->nav, on_nav_key, LV_EVENT_KEY, controller);
    lv_obj_add_event_cb(controller->close_btn, settings_close, LV_EVENT_CLICKED, controller);


    app_input_set_group(lv_obj_get_child_group(controller->nav));

    for (int i = 0; i < entries_len; ++i) {
        settings_entry_t entry = entries[i];
        lv_obj_t *item_view = lv_list_add_btn(controller->nav, entry.icon, entry.name);
        lv_obj_set_style_bg_opa(item_view, LV_OPA_COVER, LV_STATE_FOCUS_KEY);
        lv_obj_add_flag(item_view, LV_OBJ_FLAG_EVENT_BUBBLE);
        item_view->user_data = (void *) entry.cls;
    }
    show_pane(controller, entries[0].cls);
}

static void on_destroy_view(lv_obj_controller_t *self, lv_obj_t *view) {
    settings_controller_t *controller = (settings_controller_t *) self;
    lv_obj_remove_event_cb(controller->nav, on_entry_focus);
    app_input_set_group(lv_group_get_default());

    settings_save(app_configuration);

    lv_controller_manager_del(controller->pane_manager);
}

static bool on_event(lv_obj_controller_t *self, int which, void *data1, void *data2) {
    settings_controller_t *controller = (settings_controller_t *) self;
    switch (which) {
        case USER_SIZE_CHANGED: {
            lv_obj_set_size(self->obj, ui_display_width, ui_display_height);
            break;
        }
    }
    return lv_controller_manager_dispatch_event(controller->pane_manager, which, data1, data2);
}

static void on_entry_focus(lv_event_t *event) {
    settings_controller_t *controller = event->user_data;
    lv_obj_t *target = lv_event_get_target(event);
    if (lv_obj_get_parent(target) != controller->nav) return;
    lv_obj_controller_t *pane = lv_controller_manager_top_controller(controller->pane_manager);
    lv_obj_controller_class_t *cls = target->user_data;
    if (pane && pane->cls == cls) {
        return;
    }
    for (int i = 0, j = (int) lv_obj_get_child_cnt(controller->nav); i < j; i++) {
        lv_obj_t *child = lv_obj_get_child(controller->nav, i);
        if (child == target) {
            lv_obj_add_state(child, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(child, LV_STATE_CHECKED);
        }
    }
    show_pane(controller, cls);
}

static void show_pane(settings_controller_t *controller, const lv_obj_controller_class_t *cls) {
    lv_controller_manager_replace(controller->pane_manager, cls, NULL);
    for (int i = 0, j = (int) lv_obj_get_child_cnt(controller->detail); i < j; i++) {
        lv_obj_t *child = lv_obj_get_child(controller->detail, i);
        if (!lv_obj_get_group(child)) continue;
        lv_obj_add_event_cb(child, on_detail_key, LV_EVENT_KEY, controller);
        if (lv_obj_has_class(child, &lv_dropdown_class)) {
            lv_obj_add_event_cb(child, on_dropdown_clicked, LV_EVENT_CLICKED, controller);
        }
    }
    lv_group_t *group = lv_obj_get_child_group(controller->detail);
    lv_obj_t *focused = lv_group_get_focused(group);
    lv_event_send(focused, LV_EVENT_DEFOCUSED, NULL);
}

static void on_entry_click(lv_event_t *event) {
    settings_controller_t *controller = event->user_data;
    lv_obj_t *target = lv_event_get_target(event);
    if (lv_obj_get_parent(target) != controller->nav) return;
    lv_obj_t *first_focusable = NULL;
    for (int i = 0, j = (int) lv_obj_get_child_cnt(controller->detail); i < j; i++) {
        lv_obj_t *child = lv_obj_get_child(controller->detail, i);
        if (lv_obj_get_group(child)) {
            first_focusable = child;
            break;
        }
    }
    if (!first_focusable) return;
    app_input_set_group(lv_obj_get_child_group(controller->detail));
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev || lv_indev_get_type(indev) != LV_INDEV_TYPE_KEYPAD) return;
    lv_group_focus_obj(first_focusable);
}

static void on_nav_key(lv_event_t *event) {
    settings_controller_t *controller = event->user_data;
    switch (lv_event_get_key(event)) {
        case LV_KEY_ESC: {
            settings_close(event);
            break;
        }
        case LV_KEY_DOWN: {
            lv_obj_t *target = lv_event_get_target(event);
            if (lv_obj_get_parent(target) != controller->nav) return;
            lv_group_t *group = lv_obj_find_top_child_group(controller->nav);
            lv_group_focus_next(group);
            break;
        }
        case LV_KEY_UP: {
            lv_obj_t *target = lv_event_get_target(event);
            if (lv_obj_get_parent(target) != controller->nav) return;
            lv_group_t *group = lv_obj_find_top_child_group(controller->nav);
            lv_group_focus_prev(group);
            break;
        }
        case LV_KEY_RIGHT: {
            lv_obj_t *target = lv_event_get_target(event);
            if (lv_obj_get_parent(target) != controller->nav) return;
            on_entry_click(event);
            break;
        }
    }
}

static void on_detail_key(lv_event_t *e) {
    settings_controller_t *controller = e->user_data;
    switch (lv_event_get_key(e)) {
        case LV_KEY_ESC: {
            detail_defocus(controller, e, true);
            break;
        }
        case LV_KEY_UP: {
            if (controller->active_dropdown) return;
            lv_group_t *group = lv_obj_find_top_child_group(controller->detail);
            lv_group_focus_prev(group);
            break;
        }
        case LV_KEY_DOWN: {
            if (controller->active_dropdown) return;
            lv_group_t *group = lv_obj_find_top_child_group(controller->detail);
            lv_group_focus_next(group);
            break;
        }
        case LV_KEY_LEFT: {
            lv_obj_t *target = lv_event_get_target(e);
            if (detail_item_needs_lrkey(target)) return;
            detail_defocus(controller, e, false);
            break;
        }
        case LV_KEY_RIGHT: {
            lv_obj_t *target = lv_event_get_target(e);
            if (detail_item_needs_lrkey(target)) return;
            if (controller->active_dropdown) return;
            if (lv_obj_has_class(target, &lv_dropdown_class)) {
                lv_dropdown_close(target);
                controller->active_dropdown = NULL;
            }
            break;
        }
    }
}

static bool detail_item_needs_lrkey(lv_obj_t *obj) {
    if (lv_obj_has_class(obj, &lv_slider_class)) {
        return true;
    } else {
        return false;
    }
}

static void detail_defocus(settings_controller_t *controller, lv_event_t *e, bool close_dropdown) {
    lv_obj_t *target = lv_event_get_target(e);
    if (!lv_obj_has_state(target, LV_STATE_FOCUS_KEY)) return;
    if (lv_obj_has_class(target, &lv_dropdown_class)) {
        if (controller->active_dropdown) {
            if (close_dropdown) {
                controller->active_dropdown = NULL;
            }
            return;
        }
    }
    lv_event_send(target, LV_EVENT_DEFOCUSED, lv_indev_get_act());
    app_input_set_group(lv_obj_get_child_group(controller->nav));
}

static void on_dropdown_clicked(lv_event_t *event) {
    settings_controller_t *controller = event->user_data;
    lv_obj_t *target = lv_event_get_target(event);
    if (lv_obj_has_state(target, LV_STATE_CHECKED)) {
        controller->active_dropdown = target;
    } else {
        controller->active_dropdown = NULL;
    }
}

static void settings_close(lv_event_t *e) {
    settings_controller_t *controller = lv_event_get_user_data(e);
    if (controller->needs_restart) {
        static const char *btn_txts[] = {"Later", "Quit", ""};
        lv_obj_t *msgbox = lv_msgbox_create(NULL, NULL, "Some settings change requires restart to take effect.",
                                            btn_txts, false);
        lv_obj_center(msgbox);
        lv_obj_add_event_cb(msgbox, restart_confirm_cb, LV_EVENT_VALUE_CHANGED, controller);
        return;
    }
    lv_async_call((lv_async_cb_t) lv_obj_controller_pop, controller);
}

static void restart_confirm_cb(lv_event_t *e) {
    settings_controller_t *controller = lv_event_get_user_data(e);
    lv_obj_t *msgbox = lv_event_get_current_target(e);
    uint16_t selected = lv_msgbox_get_active_btn(msgbox);
    if (selected == 1) {
        app_request_exit();
    } else {
        lv_msgbox_close_async(msgbox);
        lv_async_call((lv_async_cb_t) lv_obj_controller_pop, controller);
    }
}