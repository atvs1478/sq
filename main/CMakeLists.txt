idf_component_register(SRCS "esp_app_main.c"  
						REQUIRES esp_common  pthread squeezelite-ota platform_console
                    	INCLUDE_DIRS .
                    	)