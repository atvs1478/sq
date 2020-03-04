cmake_minimum_required(VERSION 3.5)
set(COMPONENT_ADD_INCLUDEDIRS main components/tools)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)



if(NOT $ENV{PROJECT_NAME} or $ENV{PROJECT_NAME} = "recovery")
	add_definitions(-DRECOVERY_APPLICATION=1)
	project(recovery)
else()
	add_definitions(-DRECOVERY_APPLICATION=0)
	project(squeezelite)
endif()
