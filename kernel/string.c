/* =============================================================================
 * kernel/string.c - Implémentation des fonctions de chaînes et mémoire
 * =============================================================================
 *
 * Ces fonctions reproduisent le comportement de la libc standard.
 * Elles sont intentionnellement simples (pas optimisées en SSE/AVX)
 * pour être lisibles et fonctionner sans dépendances.
 * ============================================================================= */

#include "string.h"

/* -----------------------------------------------------------------------------
 * strlen - Longueur d'une chaîne
 * Compte les octets jusqu'au '\0' terminateur (non inclus)
 * ----------------------------------------------------------------------------- */
size_t strlen(const char *s)
{
    size_t len = 0;
    while (s[len] != '\0')
        len++;
    return len;
}

/* -----------------------------------------------------------------------------
 * strcmp - Comparaison de deux chaînes
 * Retourne: <0 si s1<s2, 0 si égales, >0 si s1>s2
 * Comparaison octet par octet en valeur ASCII non signée
 * ----------------------------------------------------------------------------- */
int strcmp(const char *s1, const char *s2)
{
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    /* Cast en unsigned char: 'é' (0xE9) > 'a' (0x61) comme attendu */
    return (*(const unsigned char *)s1 - *(const unsigned char *)s2);
}

/* -----------------------------------------------------------------------------
 * strncmp - Comparaison de n caractères maximum
 * ----------------------------------------------------------------------------- */
int strncmp(const char *s1, const char *s2, size_t n)
{
    while (n && *s1 && (*s1 == *s2)) {
        s1++;
        s2++;
        n--;
    }
    if (n == 0)
        return 0;
    return (*(const unsigned char *)s1 - *(const unsigned char *)s2);
}

/* -----------------------------------------------------------------------------
 * strcpy - Copier une chaîne (inclut le '\0' final)
 * ATTENTION: pas de vérification de bounds! Préférer strncpy en pratique
 * ----------------------------------------------------------------------------- */
char *strcpy(char *dest, const char *src)
{
    char *d = dest;
    while ((*d++ = *src++) != '\0')
        ;
    return dest;
}

/* -----------------------------------------------------------------------------
 * strncpy - Copier au maximum n caractères
 * Si src < n chars: remplit le reste avec des '\0'
 * ----------------------------------------------------------------------------- */
char *strncpy(char *dest, const char *src, size_t n)
{
    char *d = dest;
    while (n > 0 && *src != '\0') {
        *d++ = *src++;
        n--;
    }
    while (n > 0) {     /* Remplir le reste avec des zéros */
        *d++ = '\0';
        n--;
    }
    return dest;
}

/* -----------------------------------------------------------------------------
 * strcat - Concaténer src à la fin de dest
 * ----------------------------------------------------------------------------- */
char *strcat(char *dest, const char *src)
{
    char *d = dest + strlen(dest);
    while ((*d++ = *src++) != '\0')
        ;
    return dest;
}

/* -----------------------------------------------------------------------------
 * strchr - Trouver la première occurrence d'un caractère dans une chaîne
 * Retourne un pointeur vers le caractère, ou NULL s'il n'est pas trouvé
 * ----------------------------------------------------------------------------- */
char *strchr(const char *s, int c)
{
    while (*s != '\0') {
        if (*s == (char)c)
            return (char *)s;
        s++;
    }
    /* Cas spécial: chercher '\0' lui-même */
    if (c == '\0')
        return (char *)s;
    return NULL;
}

/* -----------------------------------------------------------------------------
 * isdigit - Vrai si c est un chiffre '0'-'9'
 * ----------------------------------------------------------------------------- */
int isdigit(int c)
{
    return (c >= '0' && c <= '9');
}

/* -----------------------------------------------------------------------------
 * isspace - Vrai si c est un espace blanc (espace, tab, newline...)
 * ----------------------------------------------------------------------------- */
int isspace(int c)
{
    return (c == ' ' || c == '\t' || c == '\n' || c == '\r'
            || c == '\f' || c == '\v');
}

/* -----------------------------------------------------------------------------
 * isupper / islower - Majuscule / minuscule ASCII
 * ----------------------------------------------------------------------------- */
int isupper(int c) { return (c >= 'A' && c <= 'Z'); }
int islower(int c) { return (c >= 'a' && c <= 'z'); }

/* =============================================================================
 * Fonctions mémoire
 * ============================================================================= */

/* -----------------------------------------------------------------------------
 * memset - Remplir un bloc mémoire avec une valeur
 * Utilisé notamment pour initialiser les tableaux, les structures, les buffers
 *
 * ASTUCE NOYAU: GCC peut émettre des appels implicites à memset (ex: pour
 * initialiser les tableaux locaux à 0). On doit donc l'implémenter même si
 * on ne l'appelle pas explicitement!
 * ----------------------------------------------------------------------------- */
void *memset(void *s, int c, size_t n)
{
    unsigned char *p = (unsigned char *)s;
    unsigned char val = (unsigned char)c;
    while (n--)
        *p++ = val;
    return s;
}

/* -----------------------------------------------------------------------------
 * memcpy - Copier n octets de src vers dest
 * ATTENTION: comportement indéfini si les zones se chevauchent → utiliser memmove
 * ----------------------------------------------------------------------------- */
void *memcpy(void *dest, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--)
        *d++ = *s++;
    return dest;
}

/* -----------------------------------------------------------------------------
 * memmove - Copier n octets même si les zones se chevauchent
 * Détecte le sens de chevauchement pour éviter la corruption de données
 * ----------------------------------------------------------------------------- */
void *memmove(void *dest, const void *src, size_t n)
{
    unsigned char       *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (d < s) {
        /* Pas de chevauchement problématique: copier vers l'avant */
        while (n--)
            *d++ = *s++;
    } else if (d > s) {
        /* Chevauchement: copier vers l'arrière pour ne pas écraser src */
        d += n;
        s += n;
        while (n--)
            *--d = *--s;
    }
    /* Si d == s: rien à faire */
    return dest;
}

/* -----------------------------------------------------------------------------
 * memcmp - Comparer deux blocs mémoire octet par octet
 * ----------------------------------------------------------------------------- */
int memcmp(const void *s1, const void *s2, size_t n)
{
    const unsigned char *a = (const unsigned char *)s1;
    const unsigned char *b = (const unsigned char *)s2;
    while (n--) {
        if (*a != *b)
            return (*a - *b);
        a++;
        b++;
    }
    return 0;
}

/* =============================================================================
 * Fonctions de conversion
 * ============================================================================= */

/* -----------------------------------------------------------------------------
 * atoi - Convertir une chaîne ASCII en entier (ASCII to Integer)
 * Ignore les espaces initiaux, gère le signe optionnel '-' ou '+'
 * ----------------------------------------------------------------------------- */
int atoi(const char *s)
{
    int  result = 0;
    int  sign   = 1;

    while (isspace(*s))     /* Sauter les espaces */
        s++;
    if (*s == '-') { sign = -1; s++; }
    else if (*s == '+') s++;

    while (isdigit(*s))
        result = result * 10 + (*s++ - '0');

    return sign * result;
}

/* -----------------------------------------------------------------------------
 * itoa - Convertir un entier signé en chaîne dans une base donnée
 * base 10 = décimal, base 16 = hexadécimal, base 2 = binaire
 * ----------------------------------------------------------------------------- */
void itoa(int value, char *buf, int base)
{
    static const char digits[] = "0123456789abcdef";
    char   tmp[34];     /* 32 bits en base 2 + signe + null = 34 max */
    int    i = 0;
    int    neg = 0;

    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; return; }

    /* Gérer les négatifs seulement en base 10 */
    if (value < 0 && base == 10) {
        neg = 1;
        value = -value;
    }

    /* Extraire les chiffres de droite à gauche */
    while (value > 0) {
        tmp[i++] = digits[value % base];
        value /= base;
    }
    if (neg)
        tmp[i++] = '-';

    /* Inverser la chaîne dans buf */
    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];
    buf[j] = '\0';
}

/* -----------------------------------------------------------------------------
 * utoa - Convertir un entier NON SIGNÉ en chaîne (utile pour %u et %x)
 * ----------------------------------------------------------------------------- */
void utoa(uint32_t value, char *buf, int base)
{
    static const char digits[] = "0123456789abcdef";
    char   tmp[33];
    int    i = 0;

    if (value == 0) { buf[0] = '0'; buf[1] = '\0'; return; }

    while (value > 0) {
        tmp[i++] = digits[value % (uint32_t)base];
        value /= (uint32_t)base;
    }

    int j = 0;
    while (i > 0)
        buf[j++] = tmp[--i];
    buf[j] = '\0';
}
