idf_component_register( SRCS 
							cmd_i2ctools.c
							cmd_nvs.c
							cmd_ota.c
							cmd_system.c
							cmd_wifi.c
							platform_console.c
						INCLUDE_DIRS .   
						REQUIRES nvs_flash
						PRIV_REQUIRES console tools services spi_flash app_update platform_config vfs pthread wifi-manager platform_config newlib  telnet )
target_link_libraries(${COMPONENT_LIB} ${build_dir}/esp-idf/$<TARGET_PROPERTY:RECOVERY_PREFIX>/lib$<TARGET_PROPERTY:RECOVERY_PREFIX>.a 	)

