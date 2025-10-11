idf_component_register(SRCS "ui/ui.c" "main.c" "utils/button.c"
                    INCLUDE_DIRS "."
                    REQUIRES epd_driver)