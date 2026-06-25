#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

esp_err_t lcd_ui_init(void);
void lcd_ui_set_status(const char *fmt, ...);
void lcd_ui_log(const char *fmt, ...);
void lcd_ui_log_hex(const char *prefix, const uint8_t *data, size_t len, size_t max_bytes);
void lcd_ui_set_locked(bool locked);
void lcd_ui_set_unlock_time(const char *time_text);
void lcd_ui_set_remaining_time(const char *time_text);
void lcd_ui_set_dom(const char *dom_text);
void lcd_ui_set_sub(const char *sub_text);
void lcd_ui_set_ble_connected(bool connected);
void lcd_ui_set_unlock_button_callback(void (*callback)(void *ctx), void *ctx);
