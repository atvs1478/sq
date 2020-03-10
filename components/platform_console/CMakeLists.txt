idf_component_register( SRCS 
							cmd_i2ctools.c
							cmd_nvs.c
							cmd_ota.c
							cmd_system.c
							cmd_wifi.c
							platform_console.c
						INCLUDE_DIRS .   
						REQUIRES nvs_flash
						PRIV_REQUIRES console  tools services   spi_flash app_update platform_config vfs pthread wifi-manager platform_config codecs newlib )


#target_link_libraries(__idf_platform_console $<IF:${recovery_build}, ${build_dir}/esp-idf/app_recovery/libapp_recovery.a,${build_dir}/esp-idf/app_squeezelite/libapp_squeezelite.a >	)
message($<TARGET_PROPERTY:RECOVERY_PREFIX>)

target_link_libraries(__idf_platform_console ${build_dir}/esp-idf/$<TARGET_PROPERTY:RECOVERY_PREFIX>/lib$<TARGET_PROPERTY:RECOVERY_PREFIX>.a 	)
#target_link_libraries(__idf_platform_console $<NOT ${recovery_build}:${build_dir}/esp-idf/app_squeezelite/libapp_squeezelite )
