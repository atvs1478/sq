
idf_component_register(SRCS display.c 
							SH1106.c 
							SSD1306.c 
							SSD132x.c  
							core/ifaces/default_if_i2c.c 
							core/ifaces/default_if_spi.c 
							core/gds_draw.c 
							core/gds_font.c 
							core/gds_image.c 
							core/gds_text.c 
							core/gds.c 
							fonts/font_droid_sans_fallback_11x13.c  
							fonts/font_droid_sans_fallback_15x17.c  
							fonts/font_droid_sans_fallback_24x28.c  
							fonts/font_droid_sans_mono_13x24.c  
							fonts/font_droid_sans_mono_16x31.c  
							fonts/font_droid_sans_mono_7x13.c  
							fonts/font_liberation_mono_13x21.c  
							fonts/font_liberation_mono_17x30.c  
							fonts/font_liberation_mono_9x15.c  
							fonts/font_line_1.c  
							fonts/font_line_2.c  
							fonts/font_tarable7seg_16x32.c  
							fonts/font_tarable7seg_32x64.c 
						REQUIRES services
						INCLUDE_DIRS . fonts core                   
)

set_source_files_properties(display.c
    PROPERTIES COMPILE_FLAGS
    -Wno-format-overflow
)