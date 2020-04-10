/* 
 *  Squeezelite for esp32
 *
 *  (c) Sebastien 2019
 *      Philippe G. 2019, philippe_44@outlook.com
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
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

