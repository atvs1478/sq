#ifndef EMBEDDED_H
#define EMBEDDED_H

#include <inttypes.h>

/* 	must provide 
		- mutex_create_p
		- pthread_create_name
		- stack size
		- s16_t, s32_t, s64_t and u64_t
		- PLAYER_ID
	can overload (use #define)
		- exit
		- gettime_ms
		- BASE_CAP
		- EXT_BSS 		
	recommended to add platform specific include(s) here
*/	
	
#ifndef PTHREAD_STACK_MIN
#define PTHREAD_STACK_MIN	256
#endif

#define STREAM_THREAD_STACK_SIZE  6 * 1024
#define DECODE_THREAD_STACK_SIZE 16 * 1024
#define OUTPUT_THREAD_STACK_SIZE  6 * 1024
#define IR_THREAD_STACK_SIZE      6 * 1024

// or can be as simple as #define PLAYER_ID 100
#define PLAYER_ID custom_player_id;
extern u8_t custom_player_id;

#define BASE_CAP "Model=squeezeesp32,AccuratePlayPoints=1,HasDigitalOut=1,HasPolarityInversion=1,Firmware=" VERSION
#define EXT_BSS __attribute__((section(".ext_ram.bss"))) 

typedef int16_t   s16_t;
typedef int32_t   s32_t;
typedef int64_t   s64_t;
typedef unsigned long long u64_t;

// all exit() calls are made from main thread (or a function called in main thread)
#define exit(code) { int ret = code; pthread_exit(&ret); }
#define gettime_ms _gettime_ms_
#define mutex_create_p(m) mutex_create(m)

uint32_t 	_gettime_ms_(void);

int			pthread_create_name(pthread_t *thread, _CONST pthread_attr_t  *attr, 
				   void *(*start_routine)( void * ), void *arg, char *name);

// must provide	of #define as empty macros		
void		embedded_init(void);
void 		register_external(void);
void 		deregister_external(void);
void 		decode_restore(int external);

// optional, please chain if used 
bool		(*slimp_handler)(u8_t *data, int len);
void 		(*slimp_loop)(void);
void 		(*server_notify)(in_addr_t ip, u16_t hport, u16_t cport);
				   
#endif // EMBEDDED_H
