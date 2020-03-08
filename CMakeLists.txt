cmake_minimum_required(VERSION 3.5)

set(COMPONENT_ADD_INCLUDEDIRS main components/tools)

if((DEFINED "$ENV{PROJECT_BUILD_NAME}") AND (DEFINED "$ENV{PROJECT_CONFIG_TARGET}"))
	set(PROJECT_VER "$ENV{PROJECT_BUILD_NAME}-$ENV{PROJECT_CONFIG_TARGET}" )	
endif()  

 



if(DEFINED "$ENV{PROJECT_NAME}")
	set(PROJECT_NAME "$ENV{PROJECT_NAME}")
	message("Project name is $PROJECT_NAME") 
else()
	message("Project name is not set! Defaulting")
	set(PROJECT_NAME "squeezelite-esp32")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(NOT ${PROJECT_NAME} STREQUAL  "squeezelite-esp32" )
	add_definitions( -DRECOVERY_APPLICATION=1 )
	message(WARNING "Building Recovery")
	project(recovery)
else()
	message(WARNING  "Building Squeezelite")
	add_definitions( -DRECOVERY_APPLICATION=0 )
	project(squeezelite)
endif()



# Include ESP-IDF components in the build, may be thought as an equivalent of
# add_subdirectory() but with some additional procesing and magic for ESP-IDF build
# specific build processes.
#idf_build_process(esp32)

# Create the project executable and plainly link the newlib component to it using
# its alias, idf::newlib.
#add_executable(${CMAKE_PROJECT_NAME}.elf main.c)
#target_link_libraries(${CMAKE_PROJECT_NAME}.elf idf::newlib)

# Let the build system know what the project executable is to attach more targets, dependencies, etc.
#idf_build_executable(${CMAKE_PROJECT_NAME}.elf)






#   add_custom_target(size
#        DEPENDS ${project_elf}
#        COMMAND ${idf_size} ${mapfile}
#        )