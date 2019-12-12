idf_component_register(SRCS "cmd_i2ctools.c"
                       INCLUDE_DIRS "." "../tools/"
                       REQUIRES esp_common
                       PRIV_REQUIRES freertos esp32 spi_flash newlib log console pthread
)

