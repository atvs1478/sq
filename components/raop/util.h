/*
 *  Misc utilities
 *
 *  (c) Philippe 2016-2017, philippe_44@outlook.com
 *
 *  This software is released under the MIT License.
 *  https://opensource.org/licenses/MIT
 *
 */

#ifndef __UTIL_H
#define __UTIL_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "platform.h"
#include "pthread.h"

#define NFREE(p) if (p) { free(p); p = NULL; }

typedef struct metadata_s {
	char *artist;
	char *album;
	char *title;
	char *genre;
	char *path;
	char *artwork;
	char *remote_title;
	u32_t track;
	u32_t duration;
	u32_t track_hash;
	u32_t sample_rate;
	u8_t  sample_size;
	u8_t  channels;
} metadata_t;

void 		free_metadata(struct metadata_s *metadata);
void 		dup_metadata(struct metadata_s *dst, struct metadata_s *src);
u32_t 		gettime_ms(void);

#ifdef WIN32
char* 		strsep(char** stringp, const char* delim);
char 		*strndup(const char *s, size_t n);
int 		asprintf(char **strp, const char *fmt, ...);
void 		winsock_init(void);
void 		winsock_close(void);
#else
char 		*strlwr(char *str);
#endif
char* 		strextract(char *s1, char *beg, char *end);
in_addr_t 	get_localhost(char **name);
void 		get_mac(u8_t mac[]);
int 		shutdown_socket(int sd);
int 		bind_socket(short unsigned *port, int mode);
int 		conn_socket(unsigned short port);typedef struct {
	char *key;
	char *data;
} key_data_t;

bool 		http_parse(int sock, char *method, key_data_t *rkd, char **body, int *len);char*		http_send(int sock, char *method, key_data_t *rkd);

char*		kd_lookup(key_data_t *kd, char *key);
bool 		kd_add(key_data_t *kd, char *key, char *value);
char* 		kd_dump(key_data_t *kd);
void 		kd_free(key_data_t *kd);

int 		_fprintf(FILE *file, ...);
#endif

