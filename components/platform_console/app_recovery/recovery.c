#include <stdio.h>
#include <string.h>

#include "esp_app_format.h"


const __attribute__((section(".rodata_desc"))) esp_app_desc_t esp_app_desc = {
    .magic_word = ESP_APP_DESC_MAGIC_WORD,
    .version = PROJECT_VER,
    .project_name = PROJECT_NAME,
    .idf_ver = IDF_VER,

#ifdef CONFIG_BOOTLOADER_APP_SECURE_VERSION
    .secure_version = CONFIG_BOOTLOADER_APP_SECURE_VERSION,
#else
    .secure_version = 0,
#endif

#ifdef CONFIG_APP_COMPILE_TIME_DATE
    .time = __TIME__,
    .date = __DATE__,
#else
    .time = "",
    .date = "",
#endif
};

int main(int argc, char **argv){
	return 1;
}
void register_squeezelite(){
}
