idf_component_register(SRC_DIRS . 
					   INCLUDE_DIRS .  ./inc  inc/alac inc/FLAC  inc/helix-aac inc/mad inc/ogg inc/opus inc/opusfile inc/resample16 inc/soxr inc/vorbis
					   PRIV_REQUIRES newlib
)

add_prebuilt_library(libmad 		lib/libmad.a)
add_prebuilt_library(libFLAC 		lib/libFLAC.a ) 
add_prebuilt_library(libhelix-aac 	lib/libhelix-aac.a ) 
add_prebuilt_library(libvorbisidec 	lib/libvorbisidec.a ) 
add_prebuilt_library(libogg 		lib/libogg.a )
add_prebuilt_library(libalac 		lib/libalac.a ) 
add_prebuilt_library(libresample16 	lib/libresample16.a ) 
add_prebuilt_library(libopusfile 	lib/libopusfile.a ) 
add_prebuilt_library(libopus 		lib/libopus.a ) 

target_link_libraries(${COMPONENT_LIB} PRIVATE libmad)
target_link_libraries(${COMPONENT_LIB} PRIVATE libFLAC)
target_link_libraries(${COMPONENT_LIB} PRIVATE libhelix-aac)
target_link_libraries(${COMPONENT_LIB} PRIVATE libvorbisidec)
target_link_libraries(${COMPONENT_LIB} PRIVATE libogg)
target_link_libraries(${COMPONENT_LIB} PRIVATE libalac)
target_link_libraries(${COMPONENT_LIB} PRIVATE libresample16)
target_link_libraries(${COMPONENT_LIB} PRIVATE libopusfile)
target_link_libraries(${COMPONENT_LIB} PRIVATE libopus)

set_source_files_properties(link_helper.c
    PROPERTIES COMPILE_FLAGS -Wno-unused-variable
)
