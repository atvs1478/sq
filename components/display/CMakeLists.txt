
idf_component_register(SRC_DIRS . core core/ifaces  fonts
						REQUIRES services
						INCLUDE_DIRS . fonts core                   
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

