idf_component_register(SRCS "gatts_table_creat_demo.c" "lcd_ui.c" "pix_font.c" "wallpaper1.c"
                    PRIV_REQUIRES bt nvs_flash esp_lcd esp_driver_gpio esp_driver_spi esp_driver_gptimer esp_driver_i2c esp_timer lvgl
                    INCLUDE_DIRS ".")
