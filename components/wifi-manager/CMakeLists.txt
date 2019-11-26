idf_component_register(SRCS "dns_server.c" "http_server.c" "wifi_manager.c"
                       INCLUDE_DIRS .
                       REQUIRES esp_common 
                       PRIV_REQUIRES newlib freertos  spi_flash nvs_flash mdns pthread wpa_supplicant cmd_system esp_http_server 
                       EMBED_FILES style.css code.js index.html bootstrap.min.css.gz jquery.min.js.gz popper.min.js.gz bootstrap.min.js.gz

)
 