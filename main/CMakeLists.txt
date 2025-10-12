idf_component_register(SRCS "ui/ui.c" "main.c" "utils/button.c" "utils/network_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES epd_driver)