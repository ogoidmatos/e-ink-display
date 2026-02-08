idf_component_register(SRCS "ui/ui.c" "main.c" "utils/button.c" "utils/network_manager.c" "utils/task_manager.c" "utils/timezone_manager.c" "utils/json_parser.c"
                    INCLUDE_DIRS "."
                    REQUIRES epd_driver
                    PRIV_REQUIRES esp_wifi nvs_flash esp_http_client json)