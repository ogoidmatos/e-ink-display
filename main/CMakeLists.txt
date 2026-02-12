idf_component_register(SRCS "ui/ui.c" "main.c" "utils/button.c" "utils/network_manager.c" "utils/task_manager.c" "utils/timezone_manager.c" "utils/json_parser.c" "utils/jwt_manager.c"
                    INCLUDE_DIRS "."
                    REQUIRES epd_driver
                    PRIV_REQUIRES esp_wifi nvs_flash esp_http_client json mbedtls)

# Copy key.pem to the build directory
set(KEY_PEM_SRC "${CMAKE_CURRENT_SOURCE_DIR}/key.pem")
set(KEY_PEM_DST "${CMAKE_BINARY_DIR}/esp-idf/main/key.pem")
add_custom_command(
    OUTPUT ${KEY_PEM_DST}
    COMMAND ${CMAKE_COMMAND} -E copy ${KEY_PEM_SRC} ${KEY_PEM_DST}
    DEPENDS ${KEY_PEM_SRC}
)

add_custom_target(copy_keypem ALL DEPENDS ${KEY_PEM_DST})

nvs_create_partition_image(nvs ./priv-key-nvs.csv FLASH_IN_PROJECT)