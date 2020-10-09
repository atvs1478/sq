idf_component_register( SRC_DIRS .
						INCLUDE_DIRS . ${IDF_PATH}/components/esp_http_server/src ${IDF_PATH}/components/esp_http_server/src/port/esp32  ${IDF_PATH}/components/esp_http_server/src/util ${IDF_PATH}/components/esp_http_server/src/
						REQUIRES squeezelite-ota json mdns 
						PRIV_REQUIRES tools services platform_config esp_common json newlib freertos  spi_flash nvs_flash mdns pthread wpa_supplicant platform_console esp_http_server console   
						EMBED_FILES res/style.css.gz res/code.js.gz index.html res/bootstrap.css.gz res/yeti/bootstrap.css.gz res/jquery.js.gz res/bootstrap.js.gz res/favicon.ico
						
)
 
