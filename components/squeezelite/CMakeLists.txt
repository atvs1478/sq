

idf_component_register( SRC_DIRS . external a1s tas57xx
						INCLUDE_DIRS .  a1s 
						PRIV_REQUIRES 	
									codecs
									newlib 
									esp_common 
									esp-dsp
						  			platform_config 
						 			driver_bt 
						 			services 
						 			raop   
						 			display
						 EMBED_FILES vu.data
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
add_compile_options (-O3 ) 



  