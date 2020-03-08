idf_component_register(SRC_DIRS .  
						REQUIRES esp_common  pthread squeezelite-ota platform_console telnet
                    	INCLUDE_DIRS .
                    	EMBED_FILES ../server_certs/github.pem
                    	)
                    	