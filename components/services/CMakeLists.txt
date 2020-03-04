idf_component_register(SRCS "accessors.c" "led.c" "audio_controls.c" "battery.c" "buttons.c" "services.c" "monitor.c" "rotary_encoder.c" 
						INCLUDE_DIRS . ../tools/
						REQUIRES json platform_config

)


