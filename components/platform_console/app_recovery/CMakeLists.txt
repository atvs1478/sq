idf_component_register( SRC_DIRS .
						INCLUDE_DIRS .   
						PRIV_REQUIRES bootloader_support
 )
 
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--undefined=esp_app_desc")	
idf_build_get_property(project_ver PROJECT_VER)
string(SUBSTRING "${project_ver}" 0 31 PROJECT_VER_CUT)
set_source_files_properties(recovery.c PROPERTIES COMPILE_DEFINITIONS "PROJECT_VER=\"${PROJECT_VER_CUT}\"; PROJECT_NAME=\"recovery\"")
