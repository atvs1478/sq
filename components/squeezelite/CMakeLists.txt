idf_component_register( SRC_DIRS . external ac101 tas57xx
						INCLUDE_DIRS .  ac101 
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
						 			tools
						 			audio
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

if(${DEPTH} EQUAL "32")
add_definitions(-DLINKALL -DLOOPBACK -DNO_FAAD -DEMBEDDED -DTREMOR_ONLY -DBYTES_PER_FRAME=8)
else()
add_definitions(-DLINKALL -DLOOPBACK -DNO_FAAD -DRESAMPLE16 -DEMBEDDED -DTREMOR_ONLY -DBYTES_PER_FRAME=4)
endif()

add_compile_options (-O3 ) 
