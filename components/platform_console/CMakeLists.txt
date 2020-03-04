set(COMPONENT_ADD_INCLUDEDIRS .)

set(COMPONENT_SRCS "platform_console.c" "cmd_ota.c" "cmd_nvs.c" "cmd_i2ctools.c" "cmd_squeezelite.c" "cmd_system.c" "cmd_wifi.c" )

set(COMPONENT_REQUIRES console nvs_flash spi_flash app_update platform_config vfs pthread wifi-manager)

register_component()
