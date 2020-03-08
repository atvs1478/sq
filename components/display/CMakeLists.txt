
idf_component_register(SRC_DIRS . core core/ifaces  fonts
						INCLUDE_DIRS . fonts core
						REQUIRES platform_config tools esp_common
						PRIV_REQUIRES services freertos    driver           
)

set_source_files_properties(display.c
    PROPERTIES COMPILE_FLAGS
    -Wno-format-overflow
)

#target_link_libraries(${COMPONENT_LIB} PUBLIC
#  -Wl,--whole-archive
#  $<TARGET_PROPERTY,INTERFACE_LINK_LIBRARIES>
#  -Wl,--no-whole-archive
#)

