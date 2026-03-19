# Gather all source files from the generated directories
file(GLOB UI_SRCS
    "generated/*.c"
    "generated/components/*.c"
    "generated/images/*.c"
    "generated/screens/*.c"
)

idf_component_register(
    SRCS
        "display_init.cpp"
        ${UI_SRCS}
    REQUIRES
        lvgl
        lcd_touch_bsp
        esp_lcd_sh8601
        i2c_bsp
        lcd_bl_pwm_bsp
    INCLUDE_DIRS
        "."
        "generated/"
        "generated/components/"
        "generated/images/"
        "generated/screens/"
)
