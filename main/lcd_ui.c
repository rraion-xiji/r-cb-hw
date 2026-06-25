#include "lcd_ui.h"

#include <assert.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/lock.h>
#include <sys/param.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/gptimer.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_io_i2c.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

extern const lv_image_dsc_t wallpaper_1;
extern const lv_font_t pix_font;

#define LCD_UI_TAG "LCD_UI"
#define LCD_HOST SPI2_HOST

#define LCD_PIN_DC   GPIO_NUM_4
#define LCD_PIN_CS   GPIO_NUM_5
#define LCD_PIN_SCL  GPIO_NUM_6
#define LCD_PIN_SDA  GPIO_NUM_7
#define LCD_PIN_RST  GPIO_NUM_38
#define LCD_PIN_BL   GPIO_NUM_40

#define TOUCH_I2C_HOST   I2C_NUM_0
#define TOUCH_PIN_RST    GPIO_NUM_39
#define TOUCH_PIN_INT    GPIO_NUM_13
#define TOUCH_PIN_SDA    GPIO_NUM_15
#define TOUCH_PIN_SCL    GPIO_NUM_14
#define TOUCH_I2C_CLK_HZ (400 * 1000)
#define LCD_TOUCH_DRAW_POINT_DEBUG 0
#define LCD_TOUCH_LOG_DEBUG 0
#define LCD_SCREEN_IDLE_TIMEOUT_MS (30 * 1000)
#define LCD_BATTERY_POLL_INTERVAL_MS 5000
#define LCD_I2C_TIMEOUT_MS 100
#define AXP2101_I2C_ADDRESS 0x34
#define AXP2101_REG_FUEL_GAUGE_CTRL 0x18
#define AXP2101_REG_BATTERY_PERCENT 0xA4

#define LCD_BL_ON_LEVEL  1
#define LCD_BL_OFF_LEVEL 0

#define LCD_H_RES 280
#define LCD_V_RES 240
#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

#define UI_FRAME_WIDTH  280
#define UI_FRAME_HEIGHT 240
#define UI_FRAME_TOP    0

#define BAR_WIDTH      104
#define BAR_HEIGHT     10
#define BAR_X          18
#define BAR_Y          30
#define BOTTOM_CARD_X  14
#define BOTTOM_CARD_Y  136
#define BOTTOM_CARD_W  252
#define BOTTOM_CARD_H  86
#define TOUCH_X_OFFSET 0
#define TOUCH_Y_OFFSET (-24)

#define LVGL_DRAW_BUF_LINES    20
#define LVGL_TICK_PERIOD_MS    2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS (1000 / CONFIG_FREERTOS_HZ)
#define LVGL_TASK_STACK_SIZE   (6 * 1024)
#define LVGL_TASK_PRIORITY     2
#define COUNTDOWN_TASK_STACK_SIZE (3 * 1024)
#define COUNTDOWN_TASK_PRIORITY   3
#define COUNTDOWN_TIMER_RESOLUTION_HZ 1000000

static _lock_t s_lvgl_api_lock;
static esp_lcd_panel_io_handle_t s_io_handle;
static esp_lcd_panel_handle_t s_panel;
static i2c_master_bus_handle_t s_touch_i2c_bus;
static i2c_master_dev_handle_t s_axp2101_handle;
static esp_lcd_panel_io_handle_t s_touch_io_handle;
static esp_lcd_touch_handle_t s_touch_handle;
static lv_display_t *s_display;
static lv_indev_t *s_touch_indev;

static lv_obj_t *s_dom_value;
static lv_obj_t *s_sub_value;
static lv_obj_t *s_ble_value;
static lv_obj_t *s_lock_value;
static lv_obj_t *s_remain_value;
static lv_obj_t *s_battery_value;
static lv_obj_t *s_message_value;
static lv_obj_t *s_bar_fill;
static lv_obj_t *s_unlock_button;

static bool s_ui_ready;
static gptimer_handle_t s_countdown_timer;
static TaskHandle_t s_countdown_task_handle;
static SemaphoreHandle_t s_touch_irq_sem;
static volatile int s_remaining_seconds = -1;
static volatile int s_remaining_total_seconds = -1;
static bool s_countdown_running;
static bool s_ble_connected;
static bool s_touch_pressed;
static bool s_screen_awake;
static volatile bool s_touch_wakeup_pending;
static int64_t s_last_touch_activity_us;
static int64_t s_last_battery_poll_us;
static void (*s_unlock_button_callback)(void *ctx);
static void *s_unlock_button_callback_ctx;

static void lcd_set_label_text(lv_obj_t *label, const char *text);
static void lcd_update_ble_indicator(bool connected);
static void lcd_update_lock_indicator(bool locked);
static void lcd_update_remaining_visuals(int remaining_seconds);
static void lcd_apply_cn_label_style(lv_obj_t *label, lv_color_t color);
static void lcd_unlock_button_event_cb(lv_event_t *e);
static void lcd_lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data);
static void lcd_touch_interrupt_cb(esp_lcd_touch_handle_t tp);
static void lcd_debug_draw_touch_point(uint16_t x, uint16_t y);
static void lcd_set_screen_awake(bool awake);
static esp_err_t lcd_axp2101_register_read(uint8_t reg_addr, uint8_t *data, size_t len);
static esp_err_t lcd_axp2101_register_write_byte(uint8_t reg_addr, uint8_t data);
static void lcd_update_battery_value_text(int percent);
static void lcd_poll_battery_status(void);

static bool lcd_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void lcd_lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel_handle = lv_display_get_user_data(disp);
    lv_draw_sw_rgb565_swap(px_map, (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
    esp_lcd_panel_draw_bitmap(panel_handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

static void lcd_increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lcd_touch_interrupt_cb(esp_lcd_touch_handle_t tp)
{
    (void)tp;

    BaseType_t high_task_wakeup = pdFALSE;
    s_touch_wakeup_pending = true;
    if (s_touch_irq_sem) {
        xSemaphoreGiveFromISR(s_touch_irq_sem, &high_task_wakeup);
    }
    if (high_task_wakeup == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

static void lcd_debug_draw_touch_point(uint16_t x, uint16_t y)
{
#if LCD_TOUCH_DRAW_POINT_DEBUG
    lv_obj_t *dot = lv_obj_create(lv_screen_active());
    lv_obj_remove_style_all(dot);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0xE23D28), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_pos(dot, (int32_t)x - 4, (int32_t)y - 4);
#else
    (void)x;
    (void)y;
#endif
}

static void lcd_lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_point_data_t touch_point = {0};
    uint8_t touchpad_cnt = 0;
    esp_lcd_touch_handle_t touch_pad = lv_indev_get_user_data(indev);

    if (s_touch_irq_sem && xSemaphoreTake(s_touch_irq_sem, 0) == pdTRUE) {
        esp_lcd_touch_read_data(touch_pad);
    }

    bool touchpad_pressed = (esp_lcd_touch_get_data(touch_pad, &touch_point, &touchpad_cnt, 1) == ESP_OK) && (touchpad_cnt > 0);
    if (touchpad_pressed && touchpad_cnt > 0) {
        int32_t corrected_x = (int32_t)touch_point.x + TOUCH_X_OFFSET;
        int32_t corrected_y = (int32_t)touch_point.y + TOUCH_Y_OFFSET;
        if (corrected_x < 0) {
            corrected_x = 0;
        } else if (corrected_x >= LCD_H_RES) {
            corrected_x = LCD_H_RES - 1;
        }
        if (corrected_y < 0) {
            corrected_y = 0;
        } else if (corrected_y >= LCD_V_RES) {
            corrected_y = LCD_V_RES - 1;
        }

        s_last_touch_activity_us = esp_timer_get_time();
        if (!s_touch_pressed) {
#if LCD_TOUCH_LOG_DEBUG
            ESP_LOGI(LCD_UI_TAG, "touch raw x=%u y=%u corrected x=%ld y=%ld",
                     touch_point.x, touch_point.y, (long)corrected_x, (long)corrected_y);
#endif
            lcd_debug_draw_touch_point((uint16_t)corrected_x, (uint16_t)corrected_y);
        }
        s_touch_pressed = true;
        data->point.x = (int16_t)corrected_x;
        data->point.y = (int16_t)corrected_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        s_touch_pressed = false;
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lcd_set_screen_awake(bool awake)
{
    if (s_screen_awake == awake) {
        return;
    }

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, awake));
    ESP_ERROR_CHECK(gpio_set_level(LCD_PIN_BL, awake ? LCD_BL_ON_LEVEL : LCD_BL_OFF_LEVEL));
    s_screen_awake = awake;
    ESP_LOGI(LCD_UI_TAG, "screen %s", awake ? "on" : "off");
}

static esp_err_t lcd_axp2101_register_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    if (!s_axp2101_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(s_axp2101_handle, &reg_addr, 1, data, len, LCD_I2C_TIMEOUT_MS);
}

static esp_err_t lcd_axp2101_register_write_byte(uint8_t reg_addr, uint8_t data)
{
    uint8_t write_buf[2] = {reg_addr, data};

    if (!s_axp2101_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit(s_axp2101_handle, write_buf, sizeof(write_buf), LCD_I2C_TIMEOUT_MS);
}

static void lcd_update_battery_value_text(int percent)
{
    char text[16];

    if (!s_battery_value) {
        return;
    }

    if (percent < 0 || percent > 100) {
        lv_label_set_text(s_battery_value, "--%");
        return;
    }

    snprintf(text, sizeof(text), "%d%%", percent);
    lv_label_set_text(s_battery_value, text);
}

static void lcd_poll_battery_status(void)
{
    uint8_t percent = 0;
    esp_err_t ret = lcd_axp2101_register_read(AXP2101_REG_BATTERY_PERCENT, &percent, 1);

    _lock_acquire(&s_lvgl_api_lock);
    if (ret == ESP_OK) {
        lcd_update_battery_value_text((int)percent);
    } else {
        lcd_update_battery_value_text(-1);
    }
    _lock_release(&s_lvgl_api_lock);
}

static void lcd_format_remaining_time_from_seconds(int total_seconds, char *out, size_t out_size)
{
    if (total_seconds < 0) {
        snprintf(out, out_size, "--:--:--");
        return;
    }

    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;
    snprintf(out, out_size, "%02d:%02d:%02d", hours, minutes, seconds);
}

static bool lcd_parse_remaining_time_to_seconds(const char *time_text, int *out_seconds)
{
    int hours;
    int minutes;
    int seconds;

    if (!time_text || sscanf(time_text, "%d:%d:%d", &hours, &minutes, &seconds) != 3) {
        return false;
    }
    if (hours < 0 || minutes < 0 || minutes > 59 || seconds < 0 || seconds > 59) {
        return false;
    }

    *out_seconds = hours * 3600 + minutes * 60 + seconds;
    return true;
}

static IRAM_ATTR bool lcd_countdown_timer_cb(gptimer_handle_t timer, const gptimer_alarm_event_data_t *edata, void *user_ctx)
{
    BaseType_t high_task_wakeup = pdFALSE;
    if (s_countdown_task_handle) {
        vTaskNotifyGiveFromISR(s_countdown_task_handle, &high_task_wakeup);
    }
    return high_task_wakeup == pdTRUE;
}

static void lcd_lvgl_port_task(void *arg)
{
    ESP_LOGI(LCD_UI_TAG, "Starting LVGL task");
    while (1) {
        uint32_t delay_ms;
        int64_t now_us = esp_timer_get_time();

        if (s_touch_wakeup_pending) {
            s_touch_wakeup_pending = false;
            s_last_touch_activity_us = now_us;
            if (!s_screen_awake) {
                lcd_set_screen_awake(true);
            }
        } else if (s_screen_awake && (now_us - s_last_touch_activity_us) >= (LCD_SCREEN_IDLE_TIMEOUT_MS * 1000LL)) {
            lcd_set_screen_awake(false);
        }

        if ((now_us - s_last_battery_poll_us) >= (LCD_BATTERY_POLL_INTERVAL_MS * 1000LL)) {
            s_last_battery_poll_us = now_us;
            lcd_poll_battery_status();
        }

        _lock_acquire(&s_lvgl_api_lock);
        delay_ms = lv_timer_handler();
        _lock_release(&s_lvgl_api_lock);
        delay_ms = MAX(delay_ms, LVGL_TASK_MIN_DELAY_MS);
        delay_ms = MIN(delay_ms, LVGL_TASK_MAX_DELAY_MS);
        usleep(1000 * delay_ms);
    }
}

static void lcd_countdown_task(void *arg)
{
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        if (s_remaining_seconds <= 0) {
            if (s_countdown_running) {
                gptimer_stop(s_countdown_timer);
                s_countdown_running = false;
            }
            s_remaining_seconds = 0;
        } else {
            s_remaining_seconds--;
        }

        _lock_acquire(&s_lvgl_api_lock);
        lcd_update_remaining_visuals((int)s_remaining_seconds);
        if (s_remaining_seconds == 0) {
            lcd_update_lock_indicator(false);
        }
        _lock_release(&s_lvgl_api_lock);
    }
}

static void lcd_set_label_text(lv_obj_t *label, const char *text)
{
    if (label) {
        lv_label_set_text(label, text);
    }
}

static void lcd_apply_label_style(lv_obj_t *label, lv_color_t color)
{
    lv_obj_set_style_text_font(label, &pix_font, 0);
    lv_obj_set_style_text_color(label, color, 0);
}

static void lcd_apply_cn_label_style(lv_obj_t *label, lv_color_t color)
{
    lv_obj_set_style_text_font(label, &pix_font, 0);
    lv_obj_set_style_text_color(label, color, 0);
}

static void lcd_update_ble_indicator(bool connected)
{
    if (!s_ble_value) {
        return;
    }

    lcd_set_label_text(s_ble_value, "B");
    lv_obj_set_style_text_color(s_ble_value, connected ? lv_color_hex(0x46A447) : lv_color_hex(0xD94C3D), 0);
}

static void lcd_update_lock_indicator(bool locked)
{
    if (!s_lock_value) {
        return;
    }

    lcd_set_label_text(s_lock_value, "L");
    lv_obj_set_style_text_color(s_lock_value, locked ? lv_color_hex(0x46A447) : lv_color_hex(0xD94C3D), 0);
}

static void lcd_update_remaining_visuals(int remaining_seconds)
{
    char formatted[16];
    int fill_width = 0;
    int elapsed_seconds = 0;
    lv_color_t fill_color = lv_color_hex(0xC68A4B);

    lcd_format_remaining_time_from_seconds(remaining_seconds, formatted, sizeof(formatted));
    lcd_set_label_text(s_remain_value, formatted);

    if (remaining_seconds >= 0 && s_remaining_total_seconds > 0) {
        elapsed_seconds = (int)s_remaining_total_seconds - remaining_seconds;
        if (elapsed_seconds < 0) {
            elapsed_seconds = 0;
        }
        fill_width = (elapsed_seconds * BAR_WIDTH) / (int)s_remaining_total_seconds;
        if (fill_width < 0) {
            fill_width = 0;
        } else if (fill_width > BAR_WIDTH) {
            fill_width = BAR_WIDTH;
        }
    }

    if (remaining_seconds >= 0 && s_remaining_total_seconds > 0) {
        int ratio_width = fill_width;
        if (remaining_seconds > 0 && ratio_width < 14) {
            ratio_width = 14;
        }
        fill_width = ratio_width;
    }

    if (remaining_seconds >= 0 && s_remaining_total_seconds > 0) {
        int percent = (remaining_seconds * 100) / (int)s_remaining_total_seconds;
        if (percent <= 20) {
            fill_color = lv_color_hex(0xD06A45);
        }
    }

    lv_obj_set_width(s_bar_fill, fill_width);
    lv_obj_set_style_bg_color(s_bar_fill, fill_color, 0);
}

static void lcd_unlock_button_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ESP_LOGI(LCD_UI_TAG, "unlock button clicked");
    if (s_unlock_button_callback) {
        s_unlock_button_callback(s_unlock_button_callback_ctx);
    }
}

static void lcd_build_ui(lv_display_t *disp)
{
    lv_obj_t *screen = lv_display_get_screen_active(disp);
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    lv_obj_t *wallpaper = lv_image_create(screen);
    lv_image_set_src(wallpaper, &wallpaper_1);
    lv_obj_set_pos(wallpaper, 0, 0);
    lv_obj_set_size(wallpaper, LCD_H_RES, LCD_V_RES);

    lv_obj_t *frame = lv_obj_create(screen);
    lv_obj_set_size(frame, UI_FRAME_WIDTH, UI_FRAME_HEIGHT);
    lv_obj_align(frame, LV_ALIGN_TOP_MID, 0, UI_FRAME_TOP);
    lv_obj_set_style_bg_opa(frame, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(frame, 0, 0);
    lv_obj_set_style_radius(frame, 0, 0);
    lv_obj_set_style_shadow_width(frame, 0, 0);
    lv_obj_set_style_pad_all(frame, 0, 0);
    lv_obj_clear_flag(frame, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dom_title = lv_label_create(frame);
    lv_label_set_text(dom_title, "\xE4\xB8\xBB\xE4\xBA\xBA\xEF\xBC\x9A");
    lcd_apply_cn_label_style(dom_title, lv_color_white());
    lv_obj_set_pos(dom_title, 20, 8);

    s_dom_value = lv_label_create(frame);
    lv_label_set_text(s_dom_value, "--");
    lcd_apply_cn_label_style(s_dom_value, lv_color_hex(0x88C942));
    lv_obj_set_pos(s_dom_value, 66, 8);

    s_battery_value = lv_label_create(frame);
    lv_label_set_text(s_battery_value, "--%");
    lcd_apply_label_style(s_battery_value, lv_color_white());
    lv_obj_set_pos(s_battery_value, 188, 8);

    s_ble_value = lv_label_create(frame);
    lcd_apply_label_style(s_ble_value, lv_color_hex(0xD94C3D));
    lv_obj_set_pos(s_ble_value, 230, 8);

    s_lock_value = lv_label_create(frame);
    lcd_apply_label_style(s_lock_value, lv_color_hex(0xD94C3D));
    lv_obj_set_pos(s_lock_value, 246, 8);

    s_sub_value = lv_label_create(frame);
    lv_label_set_text(s_sub_value, "\xE5\xB0\x8F\xE7\x8B\x97\xEF\xBC\x9A");
    lcd_apply_cn_label_style(s_sub_value, lv_color_white());
    lv_obj_set_pos(s_sub_value, 20, 28);

    lv_obj_t *bottom_card = lv_obj_create(frame);
    lv_obj_set_size(bottom_card, BOTTOM_CARD_W, BOTTOM_CARD_H);
    lv_obj_set_pos(bottom_card, BOTTOM_CARD_X, BOTTOM_CARD_Y);
    lv_obj_set_style_bg_color(bottom_card, lv_color_hex(0x202733), 0);
    lv_obj_set_style_bg_opa(bottom_card, LV_OPA_80, 0);
    lv_obj_set_style_border_color(bottom_card, lv_color_hex(0x384050), 0);
    lv_obj_set_style_border_width(bottom_card, 1, 0);
    lv_obj_set_style_radius(bottom_card, 10, 0);
    lv_obj_set_style_shadow_width(bottom_card, 0, 0);
    lv_obj_set_style_pad_all(bottom_card, 0, 0);
    lv_obj_clear_flag(bottom_card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *remain_title = lv_label_create(bottom_card);
    lv_label_set_text(remain_title, "剩余时间");
    lcd_apply_cn_label_style(remain_title, lv_color_white());
    lv_obj_set_pos(remain_title, 12, 10);

    s_remain_value = lv_label_create(bottom_card);
    lv_label_set_text(s_remain_value, "--:--:--");
    lcd_apply_label_style(s_remain_value, lv_color_white());
    lv_obj_set_pos(s_remain_value, 74, 10);

    lv_obj_t *bar_bg = lv_obj_create(bottom_card);
    lv_obj_set_size(bar_bg, BAR_WIDTH, BAR_HEIGHT);
    lv_obj_set_pos(bar_bg, BAR_X, BAR_Y);
    lv_obj_set_style_bg_color(bar_bg, lv_color_hex(0x0A1016), 0);
    lv_obj_set_style_border_width(bar_bg, 0, 0);
    lv_obj_set_style_radius(bar_bg, BAR_HEIGHT / 2, 0);
    lv_obj_set_style_shadow_width(bar_bg, 0, 0);
    lv_obj_set_style_pad_all(bar_bg, 0, 0);

    s_bar_fill = lv_obj_create(bottom_card);
    lv_obj_set_size(s_bar_fill, 0, BAR_HEIGHT);
    lv_obj_set_pos(s_bar_fill, BAR_X, BAR_Y);
    lv_obj_set_style_bg_color(s_bar_fill, lv_color_hex(0xD89A52), 0);
    lv_obj_set_style_border_width(s_bar_fill, 0, 0);
    lv_obj_set_style_radius(s_bar_fill, BAR_HEIGHT / 2, 0);
    lv_obj_set_style_shadow_width(s_bar_fill, 0, 0);
    lv_obj_set_style_pad_all(s_bar_fill, 0, 0);

    s_unlock_button = lv_button_create(bottom_card);
    lv_obj_set_size(s_unlock_button, 72, 30);
    lv_obj_set_pos(s_unlock_button, 166, 24);
    lv_obj_set_style_bg_color(s_unlock_button, lv_color_hex(0xF2D3A2), 0);
    lv_obj_set_style_border_width(s_unlock_button, 0, 0);
    lv_obj_set_style_radius(s_unlock_button, 6, 0);
    lv_obj_clear_flag(s_unlock_button, LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(s_unlock_button, lcd_unlock_button_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *button_label = lv_label_create(s_unlock_button);
    lv_label_set_text(button_label, "unlock");
    lcd_apply_label_style(button_label, lv_color_black());
    lv_obj_center(button_label);

    s_message_value = lv_label_create(bottom_card);
    lv_label_set_text(s_message_value, "\xE4\xB8\xBB\xE4\xBA\xBA\xE7\x95\x99\xE8\xA8\x80\xEF\xBC\x9A");
    lcd_apply_cn_label_style(s_message_value, lv_color_white());
    lv_label_set_long_mode(s_message_value, LV_LABEL_LONG_DOT);
    lv_obj_set_width(s_message_value, BOTTOM_CARD_W - 24);
    lv_obj_set_pos(s_message_value, 12, 58);

    lcd_update_ble_indicator(false);
    lcd_update_lock_indicator(false);
    lcd_update_remaining_visuals(-1);
}

esp_err_t lcd_ui_init(void)
{
    if (s_ui_ready) {
        return ESP_OK;
    }

    gpio_config_t bk_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BL,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_gpio_config));
    ESP_ERROR_CHECK(gpio_set_level(LCD_PIN_BL, LCD_BL_OFF_LEVEL));

    spi_bus_config_t buscfg = {
        .sclk_io_num = LCD_PIN_SCL,
        .mosi_io_num = LCD_PIN_SDA,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(uint16_t),
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO), LCD_UI_TAG, "spi bus init failed");

    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = LCD_PIN_DC,
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &s_io_handle), LCD_UI_TAG, "new panel io failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_st7789(s_io_handle, &panel_config, &s_panel), LCD_UI_TAG, "new panel failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_reset(s_panel), LCD_UI_TAG, "panel reset failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_init(s_panel), LCD_UI_TAG, "panel init failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_invert_color(s_panel, true), LCD_UI_TAG, "panel invert failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_set_gap(s_panel, 0, 0), LCD_UI_TAG, "panel set gap failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_swap_xy(s_panel, true), LCD_UI_TAG, "panel swap xy failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_mirror(s_panel, true, false), LCD_UI_TAG, "panel mirror failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_disp_on_off(s_panel, true), LCD_UI_TAG, "panel display on failed");
    ESP_RETURN_ON_ERROR(gpio_set_level(LCD_PIN_BL, LCD_BL_ON_LEVEL), LCD_UI_TAG, "backlight on failed");
    s_screen_awake = true;
    s_last_touch_activity_us = esp_timer_get_time();

    i2c_master_bus_config_t touch_i2c_bus_config = {
        .i2c_port = TOUCH_I2C_HOST,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&touch_i2c_bus_config, &s_touch_i2c_bus), LCD_UI_TAG, "touch i2c bus init failed");

    esp_lcd_panel_io_i2c_config_t touch_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    touch_io_config.scl_speed_hz = TOUCH_I2C_CLK_HZ;
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_i2c(s_touch_i2c_bus, &touch_io_config, &s_touch_io_handle), LCD_UI_TAG, "touch panel io init failed");

    i2c_device_config_t axp2101_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP2101_I2C_ADDRESS,
        .scl_speed_hz = TOUCH_I2C_CLK_HZ,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_touch_i2c_bus, &axp2101_config, &s_axp2101_handle), LCD_UI_TAG, "axp2101 add device failed");

    s_touch_irq_sem = xSemaphoreCreateBinary();
    ESP_RETURN_ON_FALSE(s_touch_irq_sem != NULL, ESP_ERR_NO_MEM, LCD_UI_TAG, "touch semaphore alloc failed");

    uint8_t gauge_ctrl = 0;
    if (lcd_axp2101_register_read(AXP2101_REG_FUEL_GAUGE_CTRL, &gauge_ctrl, 1) == ESP_OK) {
        if ((gauge_ctrl & BIT(3)) == 0) {
            lcd_axp2101_register_write_byte(AXP2101_REG_FUEL_GAUGE_CTRL, gauge_ctrl | BIT(3));
        }
    }

    esp_lcd_touch_config_t touch_config = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_PIN_RST,
        .int_gpio_num = TOUCH_PIN_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
        .interrupt_callback = lcd_touch_interrupt_cb,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_touch_new_i2c_cst816s(s_touch_io_handle, &touch_config, &s_touch_handle), LCD_UI_TAG, "touch init failed");

    lv_init();
    s_display = lv_display_create(LCD_H_RES, LCD_V_RES);
    size_t draw_buffer_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
    void *buf1 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    void *buf2 = spi_bus_dma_memory_alloc(LCD_HOST, draw_buffer_sz, 0);
    assert(buf1 && buf2);
    lv_display_set_buffers(s_display, buf1, buf2, draw_buffer_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(s_display, s_panel);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, lcd_lvgl_flush_cb);
    s_touch_indev = lv_indev_create();
    lv_indev_set_type(s_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(s_touch_indev, s_display);
    lv_indev_set_user_data(s_touch_indev, s_touch_handle);
    lv_indev_set_read_cb(s_touch_indev, lcd_lvgl_touch_cb);

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = lcd_notify_lvgl_flush_ready,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_register_event_callbacks(s_io_handle, &cbs, s_display), LCD_UI_TAG, "panel callback register failed");

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = lcd_increase_lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_RETURN_ON_ERROR(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer), LCD_UI_TAG, "tick timer create failed");
    ESP_RETURN_ON_ERROR(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000), LCD_UI_TAG, "tick timer start failed");

    _lock_acquire(&s_lvgl_api_lock);
    lcd_build_ui(s_display);
    _lock_release(&s_lvgl_api_lock);

    xTaskCreate(lcd_lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
    xTaskCreate(lcd_countdown_task, "remain_cd", COUNTDOWN_TASK_STACK_SIZE, NULL, COUNTDOWN_TASK_PRIORITY, &s_countdown_task_handle);

    gptimer_config_t countdown_timer_config = {
        .clk_src = GPTIMER_CLK_SRC_DEFAULT,
        .direction = GPTIMER_COUNT_UP,
        .resolution_hz = COUNTDOWN_TIMER_RESOLUTION_HZ,
    };
    ESP_RETURN_ON_ERROR(gptimer_new_timer(&countdown_timer_config, &s_countdown_timer), LCD_UI_TAG, "countdown timer create failed");

    gptimer_alarm_config_t countdown_alarm_config = {
        .alarm_count = COUNTDOWN_TIMER_RESOLUTION_HZ,
        .reload_count = 0,
        .flags.auto_reload_on_alarm = true,
    };
    ESP_RETURN_ON_ERROR(gptimer_set_alarm_action(s_countdown_timer, &countdown_alarm_config), LCD_UI_TAG, "countdown timer alarm failed");

    gptimer_event_callbacks_t countdown_cbs = {
        .on_alarm = lcd_countdown_timer_cb,
    };
    ESP_RETURN_ON_ERROR(gptimer_register_event_callbacks(s_countdown_timer, &countdown_cbs, NULL), LCD_UI_TAG, "countdown timer callback failed");
    ESP_RETURN_ON_ERROR(gptimer_enable(s_countdown_timer), LCD_UI_TAG, "countdown timer enable failed");

    s_ui_ready = true;
    s_last_battery_poll_us = 0;
    lcd_poll_battery_status();
    ESP_LOGI(LCD_UI_TAG, "ST7789 LVGL UI initialized");
    return ESP_OK;
}

void lcd_ui_set_status(const char *fmt, ...)
{
    char text[128];
    va_list args;

    if (!s_ui_ready || !s_message_value) {
        return;
    }

    va_start(args, fmt);
    vsnprintf(text, sizeof(text), fmt ? fmt : "", args);
    va_end(args);

    _lock_acquire(&s_lvgl_api_lock);
    if (text[0]) {
        char message[160];
        snprintf(message, sizeof(message), "\xE4\xB8\xBB\xE4\xBA\xBA\xE7\x95\x99\xE8\xA8\x80\xEF\xBC\x9A%s", text);
        lcd_set_label_text(s_message_value, message);
    } else {
        lcd_set_label_text(s_message_value, "\xE4\xB8\xBB\xE4\xBA\xBA\xE7\x95\x99\xE8\xA8\x80\xEF\xBC\x9A");
    }
    _lock_release(&s_lvgl_api_lock);
}

void lcd_ui_log(const char *fmt, ...)
{
    (void)fmt;
}

void lcd_ui_log_hex(const char *prefix, const uint8_t *data, size_t len, size_t max_bytes)
{
    (void)prefix;
    (void)data;
    (void)len;
    (void)max_bytes;
}

void lcd_ui_set_locked(bool locked)
{
    if (!s_ui_ready) {
        return;
    }

    _lock_acquire(&s_lvgl_api_lock);
    lcd_update_lock_indicator(locked);
    _lock_release(&s_lvgl_api_lock);
}

void lcd_ui_set_unlock_time(const char *time_text)
{
    (void)time_text;
}

void lcd_ui_set_dom(const char *dom_text)
{
    if (!s_ui_ready) {
        return;
    }

    _lock_acquire(&s_lvgl_api_lock);
    lcd_set_label_text(s_dom_value, (dom_text && dom_text[0]) ? dom_text : "--");
    _lock_release(&s_lvgl_api_lock);
}

void lcd_ui_set_sub(const char *sub_text)
{
    char text[64];

    if (!s_ui_ready) {
        return;
    }

    _lock_acquire(&s_lvgl_api_lock);
    if (sub_text && sub_text[0]) {
        snprintf(text, sizeof(text), "\xE5\xB0\x8F\xE7\x8B\x97\xEF\xBC\x9A%s", sub_text);
        lcd_set_label_text(s_sub_value, text);
    } else {
        lcd_set_label_text(s_sub_value, "\xE5\xB0\x8F\xE7\x8B\x97\xEF\xBC\x9A");
    }
    _lock_release(&s_lvgl_api_lock);
}

void lcd_ui_set_remaining_time(const char *time_text)
{
    if (!s_ui_ready) {
        return;
    }

    int remaining_seconds = -1;
    if (!lcd_parse_remaining_time_to_seconds(time_text, &remaining_seconds)) {
        if (s_countdown_running) {
            gptimer_stop(s_countdown_timer);
            s_countdown_running = false;
        }
        s_remaining_seconds = -1;
        s_remaining_total_seconds = -1;

        _lock_acquire(&s_lvgl_api_lock);
        lcd_update_remaining_visuals(-1);
        _lock_release(&s_lvgl_api_lock);
        return;
    }

    bool is_new_countdown = false;
    if (s_remaining_total_seconds <= 0) {
        is_new_countdown = true;
    } else if (remaining_seconds > (int)s_remaining_total_seconds) {
        is_new_countdown = true;
    } else if (s_remaining_seconds >= 0 && remaining_seconds > ((int)s_remaining_seconds + 1)) {
        is_new_countdown = true;
    }

    s_remaining_seconds = remaining_seconds;
    if (is_new_countdown) {
        s_remaining_total_seconds = remaining_seconds;
    }

    _lock_acquire(&s_lvgl_api_lock);
    lcd_update_remaining_visuals(remaining_seconds);
    if (remaining_seconds == 0) {
        lcd_update_lock_indicator(false);
    }
    _lock_release(&s_lvgl_api_lock);

    if (s_countdown_running) {
        gptimer_stop(s_countdown_timer);
        s_countdown_running = false;
    }

    if (remaining_seconds > 0 && !s_ble_connected) {
        gptimer_set_raw_count(s_countdown_timer, 0);
        gptimer_start(s_countdown_timer);
        s_countdown_running = true;
    }
}

void lcd_ui_set_ble_connected(bool connected)
{
    s_ble_connected = connected;
    if (!s_ui_ready) {
        return;
    }

    _lock_acquire(&s_lvgl_api_lock);
    lcd_update_ble_indicator(connected);
    _lock_release(&s_lvgl_api_lock);

    if (connected) {
        if (s_countdown_running) {
            gptimer_stop(s_countdown_timer);
            s_countdown_running = false;
        }
    } else if (s_remaining_seconds > 0 && !s_countdown_running) {
        gptimer_set_raw_count(s_countdown_timer, 0);
        gptimer_start(s_countdown_timer);
        s_countdown_running = true;
    }
}

void lcd_ui_set_unlock_button_callback(void (*callback)(void *ctx), void *ctx)
{
    s_unlock_button_callback = callback;
    s_unlock_button_callback_ctx = ctx;
}
