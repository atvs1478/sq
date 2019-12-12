#
# Main Makefile. This is basically the same as a component makefile.
#
CFLAGS += -D LOG_LOCAL_LEVEL=ESP_LOG_DEBUG \
	-I$(COMPONENT_PATH)/../tools				
COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_ADD_INCLUDEDIRS += $(COMPONENT_PATH)/../tools
COMPONENT_ADD_INCLUDEDIRS += $(COMPONENT_PATH)/../squeezelite-ota
COMPONENT_EXTRA_INCLUDES += $(PROJECT_PATH)/main/
