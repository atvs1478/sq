cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS components/platform_console/app_recovery components/platform_console/app_squeezelite  )
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
message(WARNING  "Building RECOVERY")

project(recovery)
set_property(TARGET recovery.elf PROPERTY RECOVERY_BUILD 0 ) 
set_property(TARGET recovery.elf PROPERTY RECOVERY_PREFIX app_recovery )

#
#message(WARNING  "Building SQUEEZELITE")
#set(project_name squeezelite)
#__project(${project_name} C CXX ASM)
#
set(squeezelite_project_elf squeezelite.elf)
#
#    # Create a dummy file to work around CMake requirement of having a source
#    # file while adding an executable
set(squeezelite_project_elf_src ${CMAKE_BINARY_DIR}/squeezelite_project_elf_src.c)
add_custom_command(OUTPUT ${squeezelite_project_elf_src}
	COMMAND ${CMAKE_COMMAND} -E touch ${squeezelite_project_elf_src}
    VERBATIM)


    add_custom_command(OUTPUT "${build_dir}/.squeezelite_bin_timestamp"
	    COMMAND echo ${ESPTOOLPY} elf2image ${ESPTOOLPY_FLASH_OPTIONS} ${esptool_elf2image_args}
        COMMAND ${ESPTOOLPY} elf2image ${ESPTOOLPY_FLASH_OPTIONS} ${esptool_elf2image_args}
            -o "${build_dir}/squeezelite.bin" "squeezelite.elf"
        COMMAND ${CMAKE_COMMAND} -E echo "Generated ${build_dir}/squeezelite.bin"
        COMMAND ${CMAKE_COMMAND} -E md5sum "${build_dir}/squeezelite.bin" > "${build_dir}/.squeezelite_bin_timestamp"
        VERBATIM
        WORKING_DIRECTORY ${build_dir}
        COMMENT "Generating binary image from built executable"
        )

add_custom_target(_squeezelite_project_elf_src DEPENDS "${squeezelite_project_elf_src}" "${build_dir}/.squeezelite_bin_timestamp")
add_executable(${squeezelite_project_elf} "${squeezelite_project_elf_src}")
add_dependencies(${squeezelite_project_elf} _squeezelite_project_elf_src)
set_property(TARGET ${squeezelite_project_elf} PROPERTY RECOVERY_BUILD 0 )
set_property(TARGET ${squeezelite_project_elf} PROPERTY RECOVERY_PREFIX app_squeezelite)

idf_build_get_property(bca BUILD_COMPONENT_ALIASES)            
target_link_libraries(${squeezelite_project_elf} ${bca})
set(squeezelite_mapfile "${CMAKE_BINARY_DIR}/squeezelite.map")
target_link_libraries(${squeezelite_project_elf} "-Wl,--cref -Wl,--Map=${squeezelite_mapfile}")
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" APPEND PROPERTY
        ADDITIONAL_MAKE_CLEAN_FILES
        "${squeezelite_mapfile}" "${squeezelite_project_elf_src}")

partition_table_get_partition_info(otaapp_offset "--partition-type app --partition-subtype ota_0" "offset")
esptool_py_flash_project_args(squeezelite ${otaapp_offset} ${build_dir}/squeezelite.bin FLASH_IN_PROJECT)
