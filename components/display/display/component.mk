#
# Component Makefile
#
# This Makefile should, at the very least, just include $(SDK_PATH)/Makefile. By default,
# this will take the sources in the src/ directory, compile them and link them into
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the SDK documents if you need to do this.
#
CFLAGS += 	-I$(COMPONENT_PATH)/../squeezelite
COMPONENT_SRCDIRS := . tarablessd1306 tarablessd1306/fonts tarablessd1306/ifaces
COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_ADD_INCLUDEDIRS += ./tarablessd1306

