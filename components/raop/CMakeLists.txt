
idf_component_register(SRCS dmap_parser.c raop_sink.c raop.c rtp.c util.c   
						INCLUDE_DIRS .   
						REQUIRES newlib platform_config services codecs tools
						PRIV_REQUIRES platform_display  
)
