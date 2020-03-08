idf_component_register( SRC_DIRS .
						INCLUDE_DIRS .
						REQUIRES squeezelite-ota json mdns 
						PRIV_REQUIRES tools services platform_config esp_common json newlib freertos  spi_flash nvs_flash mdns pthread wpa_supplicant platform_console   
						EMBED_FILES style.css code.js index.html bootstrap.min.css.gz jquery.min.js.gz popper.min.js.gz bootstrap.min.js.gz


)
 
