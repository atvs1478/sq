idf_component_register(SRCS  "squeezelite-ota.c"
					  INCLUDE_DIRS . 
					  REQUIRES console spi_flash console freertos platform_console esp_https_ota
					  )


