#
# Main Makefile. This is basically the same as a component makefile.
#
COMPONENT_ADD_INCLUDEDIRS := .
CFLAGS += -D LOG_LOCAL_LEVEL=ESP_LOG_INFO 
COMPONENT_SRCDIRS := . 