#ifndef LIBC_H
#define LIBC_H

#include "common.h"

/* --- String functions --- */
extern s32 strlen(/* char * */);
extern char *strcpy(/* char *, char * */);
extern char *strcat(char *dst, const char *src);
extern s32 strcmp(/* char *, char * */);
extern s32 strncmp(const char *s1, const char *s2, s32 n);
extern s32 strtol(const char *s, char **endptr, s32 base);

/* --- Memory functions --- */
extern void *memcpy(/* unsigned char *, unsigned char *, int */);
extern void *memset(/* unsigned char *, unsigned char, int */);
extern void *memchr(const u8 *s, u8 c, s32 n);
extern void *memmove(u8 *dst, const u8 *src, s32 n);
extern void *bzero(u8 *s, s32 n);

/* --- I/O functions --- */
extern s32 printf(const char *fmt, ...);
extern s32 sprintf(char *buf, const char *fmt, ...);

/* --- Random number functions --- */
extern s32 rand(void);
extern void srand(u32 seed);

#endif /* LIBC_H */
