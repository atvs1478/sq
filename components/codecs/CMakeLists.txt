idf_component_register(SRC_DIRS . 
					   INCLUDE_DIRS .  ./inc  inc/alac inc/faad2 inc/FLAC  inc/helix-aac inc/mad inc/ogg inc/opus inc/opusfile inc/resample16 inc/soxr inc/vorbis
	                       REQUIRES esp_common platform_config freertos nvs_flash esp32 spi_flash newlib log pthread
)

add_prebuilt_library(libmad 		lib/libmad.a)
add_prebuilt_library(libesp-flac 	lib/libesp-flac.a ) 
add_prebuilt_library(libhelix-aac 	lib/libhelix-aac.a ) 
add_prebuilt_library(libvorbisidec 	lib/libvorbisidec.a ) 
add_prebuilt_library(libogg 		lib/libogg.a )
add_prebuilt_library(libalac 		lib/libalac.a ) 
add_prebuilt_library(libremple16 	lib/libresample16.a ) 
add_prebuilt_library(libsoxr 		lib/libsoxr.a ) 
add_prebuilt_library(libopusfile 	lib/libopusfile.a ) 
add_prebuilt_library(libopus 		lib/libopus.a ) 
target_link_libraries(${COMPONENT_LIB} PRIVATE libmad)
target_link_libraries(${COMPONENT_LIB} PRIVATE libesp-flac)
target_link_libraries(${COMPONENT_LIB} PRIVATE libhelix-aac)
target_link_libraries(${COMPONENT_LIB} PRIVATE libvorbisidec)

target_link_libraries(${COMPONENT_LIB} PRIVATE libogg)
target_link_libraries(${COMPONENT_LIB} PRIVATE libalac)
target_link_libraries(${COMPONENT_LIB} PRIVATE libremple16)
target_link_libraries(${COMPONENT_LIB} PRIVATE libsoxr)
target_link_libraries(${COMPONENT_LIB} PRIVATE libopusfile)
target_link_libraries(${COMPONENT_LIB} PRIVATE libopus)
