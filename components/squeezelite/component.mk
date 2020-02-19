#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)
CFLAGS += -O3 -DLINKALL -DLOOPBACK -DNO_FAAD -DRESAMPLE16 -DEMBEDDED -DTREMOR_ONLY -DBYTES_PER_FRAME=4 	\
	-I$(PROJECT_PATH)/components/codecs/inc			\
	-I$(PROJECT_PATH)/components/codecs/inc/mad 		\
	-I$(PROJECT_PATH)/components/codecs/inc/alac		\
	-I$(PROJECT_PATH)/components/codecs/inc/helix-aac	\
	-I$(PROJECT_PATH)/components/codecs/inc/vorbis 	\
	-I$(PROJECT_PATH)/components/codecs/inc/soxr 		\
	-I$(PROJECT_PATH)/components/codecs/inc/resample16	\
	-I$(PROJECT_PATH)/components/tools				\
	-I$(PROJECT_PATH)/components/codecs/inc/opus 		\
	-I$(PROJECT_PATH)/components/codecs/inc/opusfile	\
	-I$(PROJECT_PATH)/components/driver_bt			\
	-I$(PROJECT_PATH)/components/raop					\
	-I$(PROJECT_PATH)/components/services

#	-I$(PROJECT_PATH)/components/codecs/inc/faad2

COMPONENT_SRCDIRS := . tas57xx a1s external
COMPONENT_ADD_INCLUDEDIRS := . ./tas57xx ./a1s

