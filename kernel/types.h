#ifndef TYPES_H
#define TYPES_H

/* =============================================================================
 * kernel/types.h - Types de base du noyau
 * =============================================================================
 *
 * CONCEPT: Dans un noyau, on n'a pas accès à la libc (stdint.h, stddef.h, etc.)
 * car on compile avec -nostdlib. Il faut donc redéfinir les types fondamentaux.
 *
 * On utilise des types à taille fixe (fixed-width types) pour être explicite
 * sur la taille en mémoire, crucial en programmation système où:
 *   - Les structures doivent correspondre exactement au matériel
 *   - Les registres ont des tailles précises (8, 16, 32, 64 bits)
 *   - Le code doit être portable entre compilateurs
 *
 * Convention de nommage: uint = unsigned integer, int = signed integer
 * Le chiffre = nombre de BITS (8, 16, 32, 64)
 * ============================================================================= */

/* --- Entiers non signés (unsigned) --- */
typedef unsigned char       uint8_t;    /* 1 octet,  0 à 255 */
typedef unsigned short      uint16_t;   /* 2 octets, 0 à 65535 */
typedef unsigned int        uint32_t;   /* 4 octets, 0 à 4294967295 */
typedef unsigned long long  uint64_t;   /* 8 octets, 0 à 18446744073709551615 */

/* --- Entiers signés --- */
typedef signed char         int8_t;     /* 1 octet,  -128 à 127 */
typedef signed short        int16_t;    /* 2 octets, -32768 à 32767 */
typedef signed int          int32_t;    /* 4 octets, -2147483648 à 2147483647 */
typedef signed long long    int64_t;    /* 8 octets */

/* --- Types spéciaux --- */
typedef uint32_t    size_t;     /* Taille en mémoire (toujours non signé) */
typedef int32_t     ssize_t;    /* Taille signée (pour retours d'erreur -1) */
typedef uint32_t    uintptr_t;  /* Entier capable de stocker un pointeur 32 bits */

/* --- Booléen --- */
typedef uint8_t     bool;
#define true    1
#define false   0

/* --- Pointeur nul --- */
#define NULL    ((void*)0)

/* --- Macros utilitaires --- */
#define ARRAY_SIZE(arr)     (sizeof(arr) / sizeof((arr)[0]))
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define MIN(a, b)           ((a) < (b) ? (a) : (b))

/* Attributs GCC pour le noyau */
#define PACKED              __attribute__((packed))     /* Pas de padding dans la struct */
#define NORETURN            __attribute__((noreturn))   /* Fonction qui ne retourne jamais */
#define UNUSED              __attribute__((unused))     /* Supprimer warning "unused" */
#define ALIGN(n)            __attribute__((aligned(n))) /* Alignement mémoire forcé */

#endif /* TYPES_H */
