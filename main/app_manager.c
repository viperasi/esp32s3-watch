#include "app_manager.h"
#include "app_launcher.h"
#include "apps/app_settings.h"
#include "lvgl.h"
#include "esp_log.h"

#define TAG "APP_MGR"
#define MAX_SCREEN_STACK 8

typedef enum {
    STATE_WATCHFACE,
    STATE_LAUNCHER,
    STATE_APP,
} app_state_t;

static app_state_t state = STATE_WATCHFACE;
static lv_obj_t *wf_scr = NULL;

/* Screen stack for sub-page navigation within apps */
static lv_obj_t *screen_stack[MAX_SCREEN_STACK];
static int screen_stack_top = -1;

static const struct {
    const char *name;
    const char *symbol;
    lv_obj_t *(*create)(void);
} apps[] = {
    {"Settings", LV_SYMBOL_SETTINGS, app_settings_create},
};
#define APP_COUNT (sizeof(apps) / sizeof(apps[0]))

void app_manager_init(void)
{
    ESP_LOGI(TAG, "App manager initialized, %d app(s)", (int)APP_COUNT);
}

void app_manager_show_launcher(void)
{
    if (state != STATE_WATCHFACE) return;
    wf_scr = lv_screen_active();
    lv_obj_t *launcher = app_launcher_create();
    lv_screen_load_anim(launcher, LV_SCR_LOAD_ANIM_MOVE_TOP, 300, 0, false);
    state = STATE_LAUNCHER;
}

void app_manager_open_app(int index)
{
    if (state != STATE_LAUNCHER) return;
    if (index < 0 || index >= (int)APP_COUNT) return;
    lv_obj_t *app_scr = apps[index].create();
    /* Clear screen stack when opening a new app */
    screen_stack_top = -1;
    lv_screen_load_anim(app_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, true);
    state = STATE_APP;
}

void app_manager_back(void)
{
    if (state == STATE_WATCHFACE) return;

    if (screen_stack_top >= 0) {
        /* Pop sub-screen */
        lv_obj_t *prev = screen_stack[screen_stack_top];
        screen_stack_top--;
        lv_screen_load_anim(prev, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 200, 0, true);
        return;
    }

    /* Back to watchface */
    lv_screen_load_anim(wf_scr, LV_SCR_LOAD_ANIM_MOVE_BOTTOM, 300, 0, true);
    state = STATE_WATCHFACE;
}

bool app_manager_is_foreground(void)
{
    return state == STATE_WATCHFACE;
}

int app_manager_get_app_count(void)
{
    return (int)APP_COUNT;
}

const char *app_manager_get_app_name(int idx)
{
    if (idx < 0 || idx >= (int)APP_COUNT) return NULL;
    return apps[idx].name;
}

const char *app_manager_get_app_symbol(int idx)
{
    if (idx < 0 || idx >= (int)APP_COUNT) return NULL;
    return apps[idx].symbol;
}

void app_manager_push_screen(lv_obj_t *scr)
{
    if (screen_stack_top >= MAX_SCREEN_STACK - 1) {
        ESP_LOGW(TAG, "Screen stack full");
        lv_obj_del(scr);
        return;
    }
    screen_stack_top++;
    screen_stack[screen_stack_top] = lv_screen_active();
    lv_screen_load_anim(scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 200, 0, false);
}

void app_manager_pop_screen(void)
{
    app_manager_back();
}
