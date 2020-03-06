idf_component_register(SRCS platform_bt_core.c
							platform_bt_sink.c
							platform_bt_source.c
                       INCLUDE_DIRS .
                       REQUIRES bt platform_display  esp_common freertos  nvs_flash esp32 spi_flash newlib log  pthread platform_config

)

