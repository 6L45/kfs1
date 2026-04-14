#ifndef STRING_H
#define STRING_H

/* =============================================================================
 * kernel/string.h - Fonctions de manipulation de chaînes et mémoire
 * =============================================================================
 *
 * CONCEPT: Sans libc, on doit réimplémenter les fonctions de base de <string.h>
 * Ces fonctions sont utilisées partout dans le noyau (drivers, allocateur, etc.)
 * ============================================================================= */

#include "types.h"

/* --- Fonctions sur les chaînes de caractères --- */
size_t  strlen(const char *s);
int     strcmp(const char *s1, const char *s2);
int     strncmp(const char *s1, const char *s2, size_t n);
char   *strcpy(char *dest, const char *src);
char   *strncpy(char *dest, const char *src, size_t n);
char   *strcat(char *dest, const char *src);
char   *strchr(const char *s, int c);
int     isdigit(int c);
int     isspace(int c);
int     isupper(int c);
int     islower(int c);

/* --- Fonctions mémoire --- */
void   *memset(void *s, int c, size_t n);
void   *memcpy(void *dest, const void *src, size_t n);
void   *memmove(void *dest, const void *src, size_t n);
int     memcmp(const void *s1, const void *s2, size_t n);

/* --- Conversion --- */
int     atoi(const char *s);
void    itoa(int value, char *buf, int base);    /* int vers chaîne */
void    utoa(uint32_t value, char *buf, int base); /* unsigned int vers chaîne */

#endif /* STRING_H */
