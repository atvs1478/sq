idf_component_register(SRCS "dns_server.c" "http_server.c" "wifi_manager.c"
                       INCLUDE_DIRS .
                       REQUIRES esp_common 
                       PRIV_REQUIRES newlib freertos  spi_flash nvs_flash mdns pthread wpa_supplicant cmd_system 
                       EMBED_FILES style.css code.js index.html bootstrap.min.css.gz jquery.min.js.gz popper.min.js.gz bootstrap.min.js.gz battery-0.svg battery-1.svg battery-2.svg battery-3.svg battery-4.svg


)
 
