# build system (Jenkins) should pass the following
#export PROJECT_BUILD_NAME="${build_version_prefix}${BUILD_NUMBER}"
#export PROJECT_BUILD_TARGET="${config_target}"

PROJECT_BUILD_NAME?=local
PROJECT_CONFIG_TARGET?=custom
PROJECT_NAME?=squeezelite.$(PROJECT_CONFIG_TARGET)
PROJECT_VER?="$(PROJECT_BUILD_NAME)-$(PROJECT_CONFIG_TARGET)"
RECOVERY_APPLICATION?=0
CFLAGS?=

	

include $(IDF_PATH)/make/project.mk 
