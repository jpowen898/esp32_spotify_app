# Gather all source files from the generated directories
file(GLOB UI_SRCS
    "generated/*.c"
    "generated/components/*.c"
    "generated/fonts/*.c"
    "generated/images/*.c"
    "generated/screens/*.c"
)

idf_component_register(
    SRCS
        ${UI_SRCS}
    PRIV_REQUIRES
        lvgl
    INCLUDE_DIRS
        "generated/"
        "generated/components/"
        "generated/fonts/"
        "generated/images/"
        "generated/screens/"
)
