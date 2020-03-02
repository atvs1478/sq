set(COMPONENT_ADD_INCLUDEDIRS . ${IDF_PATH}/components/esp_http_server/src ${IDF_PATH}/components/esp_http_server/src/port/esp32  ${IDF_PATH}/components/esp_http_server/src/util)

set(COMPONENT_SRCS "esp_app_main.c" "platform_esp32.c" "cmd_wifi.c" "console.c" "nvs_utilities.c" "cmd_squeezelite.c" "config.c")
set(REQUIRES esp_common)
set(REQUIRES_COMPONENTS freertos squeezelite nvs_flash esp32 spi_flash newlib log console ota tools )


register_component()
