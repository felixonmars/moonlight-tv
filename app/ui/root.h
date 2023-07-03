#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <SDL.h>
#include <lvgl.h>

#include "ui/config.h"

#include "util/navkey.h"

typedef struct app_t app_t;

enum UI_INPUT_MODE {
    UI_INPUT_MODE_POINTER_FLAG = 0x10,
    UI_INPUT_MODE_MOUSE = 0x11,
    UI_INPUT_MODE_REMOTE = 0x11,
    UI_INPUT_MODE_BUTTON_FLAG = 0x20,
    UI_INPUT_MODE_KEY = 0x21,
    UI_INPUT_MODE_GAMEPAD = 0x22,
};

typedef struct {
    void *data1;
    void *data2;
} ui_userevent_t;

#define NAV_WIDTH_COLLAPSED 44
#define NAV_LOGO_SIZE 24

extern short ui_display_width, ui_display_height;
extern enum UI_INPUT_MODE ui_input_mode;

const lv_img_dsc_t *ui_logo_src();

bool ui_has_stream_renderer();

bool ui_render_background();

bool ui_dispatch_userevent(app_t *app, int which, void *data1, void *data2);

/**
 * @brief Check if GUI should consume input events, so it will not pass onto streaming
 */
bool ui_should_block_input();

void ui_display_size(short width, short height);

bool ui_set_input_mode(enum UI_INPUT_MODE mode);

void ui_cb_destroy_fragment(lv_event_t *e);