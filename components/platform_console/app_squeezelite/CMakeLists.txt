idf_component_register( SRC_DIRS .
						INCLUDE_DIRS .   
						PRIV_REQUIRES console codecs squeezelite newlib pthread tools platform_config)
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--undefined=feof")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--undefined=fdopen")

