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


