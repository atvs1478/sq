idf_component_register(SRCS "nvs_utilities.c" "platform_config.c"
						INCLUDE_DIRS . 
						INCLUDE_DIRS . ../tools/
                   REQUIRES nvs_flash json platform_console services vfs
)


