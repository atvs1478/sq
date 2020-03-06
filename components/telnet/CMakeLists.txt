idf_component_register(SRCS "telnet.c" "libtelnet/libtelnet.c"
						INCLUDE_DIRS .  libtelnet 
						REQUIRES platform_config services 
)
