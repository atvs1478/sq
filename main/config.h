#pragma once
#include <stdio.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_utilities.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __cplusplus
}
#endif
bool config_has_changes();
void config_commit_to_nvs();
void config_start_timer();
void config_init();
void * config_alloc_get_default(nvs_type_t type, const char *key, void * default_value, size_t blob_size);
void config_delete_key(const char *key);
void config_set_default(nvs_type_t type, const char *key, void * default_value, size_t blob_size);
void * config_alloc_get(nvs_type_t nvs_type, const char *key) ;
bool wait_for_commit();
char * config_alloc_get_json(bool bFormatted);
esp_err_t config_set_value(nvs_type_t nvs_type, const char *key, void * value);

