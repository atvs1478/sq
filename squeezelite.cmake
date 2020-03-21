include($ENV{IDF_PATH}/tools/cmake/project.cmake)

function(___register_flash target_name sub_type)
	partition_table_get_partition_info(otaapp_offset "--partition-type app --partition-subtype ${sub_type}" "offset")
	esptool_py_flash_project_args(${target_name} ${otaapp_offset} ${build_dir}/${target_name}.bin FLASH_IN_PROJECT)
	esptool_py_custom_target(${target_name}-flash ${target_name} "${target_name}")
endfunction()
#
# Removes the specified compile flag from the specified target.
#   _target     - The target to remove the compile flag from
#   _flag       - The compile flag to remove
#
# Pre: apply_global_cxx_flags_to_all_targets() must be invoked.
#
macro(remove_flag_from_target _target _flag)
    get_target_property(_target_cxx_flags ${_target} COMPILE_OPTIONS)
    if(_target_cxx_flags)
        list(REMOVE_ITEM _target_cxx_flags ${_flag})
        set_target_properties(${_target} PROPERTIES COMPILE_OPTIONS "${_target_cxx_flags}")
    endif()
endmacro()
function(___print_list pref listcontent)
	message("")
	message("${pref}")
	foreach(e  ${listcontent})
	  message("${pref} ${e}")
	endforeach()
	message("")
endfunction()

function(___create_new_target target_name)
	idf_build_get_property(build_dir BUILD_DIR)
	file(TO_CMAKE_PATH "${IDF_PATH}" idf_path)

	set(target_elf ${target_name}.elf)

	
	# Create a dummy file to work around CMake requirement of having a source
	# file while adding an executable
	
	set(target_elf_src ${CMAKE_BINARY_DIR}/${target_name}_src.c)
	add_custom_command(OUTPUT ${target_elf_src}
		BUILD
		COMMAND ${CMAKE_COMMAND} -E touch ${target_elf_src}
	    VERBATIM)
	
	add_custom_target(_${target_name}_elf DEPENDS "${target_elf_src}"  )
	add_executable(${target_elf} "${target_elf_src}")
	add_dependencies(${target_elf} _${target_name}_elf)
	add_dependencies(${target_elf} "recovery.elf")
	
	set_property(TARGET ${target_elf} PROPERTY RECOVERY_PREFIX app_${target_name})
	set(ESPTOOLPY_ELF2IMAGE_OPTIONS --elf-sha256-offset 0xb0)

	# Remove app_recovery so that app_squeezelite and dependencies are properly resolved
	idf_build_get_property(bca BUILD_COMPONENT_ALIASES)
	list(REMOVE_ITEM bca "idf::app_recovery")
	list(REMOVE_ITEM bca "idf::app_squeezelite")
	target_link_libraries(${target_elf} ${bca})
	target_link_libraries(${target_elf} idf::app_squeezelite)
	set(target_name_mapfile "${target_name}.map")
	target_link_libraries(${target_elf} "-Wl,--cref -Wl,--Map=${CMAKE_BINARY_DIR}/${target_name_mapfile}")

#	idf_build_get_property(link_depends __LINK_DEPENDS)
#	idf_build_get_property(link_options LINK_OPTIONS)
#	idf_build_get_property(ldgen_libraries __LDGEN_LIBRARIES GENERATOR_EXPRESSION)	
		
	add_custom_command(
			TARGET ${target_elf}
			
			POST_BUILD 
			COMMAND ${CMAKE_COMMAND} -E echo "Generating ${build_dir}/${target_name}.bin" 
			COMMAND ${ESPTOOLPY} elf2image ${ESPTOOLPY_FLASH_OPTIONS} ${ESPTOOLPY_ELF2IMAGE_OPTIONS} -o "${build_dir}/${target_name}.bin" "${target_name}.elf"
	        DEPENDS "${target_name}.elf" 
	        WORKING_DIRECTORY ${build_dir}
	        COMMENT "Generating binary image from built executable"
	        VERBATIM
	)
	set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY
        ADDITIONAL_MAKE_CLEAN_FILES
        "${build_dir}/${target_name_mapfile}" "${build_dir}/${target_elf_src}" )
        

endfunction()

___create_new_target(squeezelite )
___register_flash(squeezelite ota_0)


add_custom_target(_jtag_scripts  ALL
					BYPRODUCTS "flash_dbg_project_args"  
					POST_BUILD
					COMMAND ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_debug_scripts.cmake"
#					COMMAND ${CMAKE_COMMAND} --graphviz=graph.dot .
#					$ sed -n 's/.*label="\(.*\)"\s.*/\1/p' graph.dot.foo > foo_dependencies.txt
					)
					
add_dependencies(partition_table _jtag_scripts)

idf_build_get_property(build_dir BUILD_DIR)
add_custom_command(
			TARGET recovery.elf
			PRE_LINK
			COMMAND xtensa-esp32-elf-objcopy  --weaken-symbol esp_app_desc  ${build_dir}/esp-idf/app_update/libapp_update.a
	        VERBATIM
)
add_custom_command(
			TARGET squeezelite.elf
			PRE_LINK
			COMMAND xtensa-esp32-elf-objcopy  --weaken-symbol esp_app_desc  ${build_dir}/esp-idf/app_update/libapp_update.a
	        VERBATIM
)