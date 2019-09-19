#
# "main" pseudo-component makefile.
#
# (Uses default behaviour of compiling all source files in directory, adding 'include' to include path.)

# todo: add support for https
COMPONENT_ADD_INCLUDEDIRS := .
CFLAGS += -D LOG_LOCAL_LEVEL=ESP_LOG_DEBUG -D CONFIG_OTA_ALLOW_HTTP=1
#COMPONENT_EMBED_TXTFILES :=  ${PROJECT_PATH}/server_certs/ca_cert.pem