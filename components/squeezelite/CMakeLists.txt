
idf_component_register(SRCS "alac.c" 
							"buffer.c"
							"controls.c" 
							"decode_external.c" 
							"decode.c"
							"display.c"
							"embedded.c" 
							"flac.c" 
							"helix-aac.c"
							"mad.c" 
							"main.c"
							"mpg.c" 
							"opus.c"
							"output_bt.c"
							"output_embedded.c" 
							"output_i2s.c" 
							"output_pack.c" 
							"output_visu.c"
							"output.c" 
							"pcm.c" 
							"process.c"
							"resample.c" 
							"resample16.c"
							"slimproto.c"
							"stream.c"
							"utils.c"
							"vorbis.c"
							"a1s/ac101.c"
							"tas57xx/dac_57xx.c" 
							"external/dac_external.c" 
						INCLUDE_DIRS .  a1s 
						REQUIRES 	newlib 
									esp_common 
									esp-dsp
						  			display 
						 			platform_config 
						 			platform_bluetooth 
						 			codecs 
						 			services 
						 			raop   
)



set_source_files_properties(mad.c
    PROPERTIES COMPILE_FLAGS
    -Wno-maybe-uninitialized
)
set_source_files_properties(pcm.c
    PROPERTIES COMPILE_FLAGS
    -Wno-maybe-uninitialized
)

set_source_files_properties(flac.c
    PROPERTIES COMPILE_FLAGS
    -Wno-maybe-uninitialized
)

add_definitions(-DLINKALL -DLOOPBACK -DNO_FAAD -DRESAMPLE16 -DEMBEDDED -DTREMOR_ONLY -DBYTES_PER_FRAME=4)
add_compile_options (-O3)
