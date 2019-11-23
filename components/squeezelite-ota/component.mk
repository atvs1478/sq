#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# todo: add support for https
COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_ADD_INCLUDEDIRS += include
COMPONENT_EXTRA_INCLUDES += $(PROJECT_PATH)/main/
COMPONENT_EXTRA_INCLUDES += $(PROJECT_PATH)/components/tools
CFLAGS += -D LOG_LOCAL_LEVEL=ESP_LOG_INFO -DCONFIG_OTA_ALLOW_HTTP=1
#CFLAGS += -DLOG_LOCAL_LEVEL=ESP_LOG_DEBUG -DCONFIG_OTA_ALLOW_HTTP=1

