cmake_minimum_required(VERSION 3.5)
set(EXTRA_COMPONENT_DIRS components/platform_console/app_recovery components/platform_console/app_squeezelite  )
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

message(STATUS  "Building RECOVERY")
project(recovery)
set_property(TARGET recovery.elf PROPERTY RECOVERY_BUILD 0 ) 
set_property(TARGET recovery.elf PROPERTY RECOVERY_PREFIX app_recovery )


include(squeezelite.cmake)
