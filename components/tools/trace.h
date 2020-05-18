/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */
 
#pragma once

#ifndef QUOTE
#define QUOTE(name) #name
#endif
#ifndef STR
#define STR(macro)  QUOTE(macro)
#endif
#define ESP_LOG_DEBUG_EVENT(tag,e) ESP_LOGD(tag,"evt: " e)
#ifndef STR_OR_ALT
#define STR_OR_ALT(str,alt) (str?str:alt)
#endif

extern const char unknown_string_placeholder[];
extern const char * str_or_unknown(const char * str);
extern const char * str_or_null(const char * str);

#ifndef FREE_AND_NULL
#define FREE_AND_NULL(x) if(x) { free(x); x=NULL; }
#endif

#ifndef CASE_TO_STR
#define CASE_TO_STR(x) case x: return STR(x); break;
#endif
#define START_FREE_MEM_CHECK(a) size_t a=heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
#define CHECK_RESET_FREE_MEM_CHECK(a,b) ESP_LOGV(__FUNCTION__ ,b "Mem used: %i",a-heap_caps_get_free_size(MALLOC_CAP_INTERNAL)); a=heap_caps_get_free_size(MALLOC_CAP_INTERNAL)

