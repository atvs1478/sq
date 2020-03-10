idf_component_register( SRC_DIRS .
						INCLUDE_DIRS .   
 )
#idf_build_get_property(IS_RECOVERY  0 )
#target_link_options(__idf_platform_console PUBLIC $<$<EQUAL:${IS_RECOVERY},1>:${COMPONENT_LIB}>)
#add_library(libcmd_squeezelite STATIC cmd_squeezelite.c )
#__component_add_include_dirs(libcmd_squeezelite "${__INCLUDE_DIRS}" INTERFACE)
#set_property(TARGET libcmd_squeezelite APPEND PROPERTY LINK_LIBRARIES console)
#set_property(TARGET libcmd_squeezelite APPEND PROPERTY INTERFACE_LINK_LIBRARIES $<LINK_ONLY:console>)
#
#
#add_library(librecovery STATIC recovery.c )
#target_link_options(${COMPONENT_LIB} PUBLIC $<IF:${IS_RECOVERY},librecovery.a,libcmd_squeezelite.a>)						 
