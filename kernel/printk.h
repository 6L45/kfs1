#ifndef PRINTK_H
#define PRINTK_H

/* =============================================================================
 * kernel/printk.h - Fonction printf-like pour le noyau
 * =============================================================================
 *
 * CONCEPT: VARIADIC FUNCTIONS (fonctions à arguments variables)
 *
 * printf() en libc utilise <stdarg.h> pour accéder aux arguments variables.
 * Sans libc, on implémente nos propres macros va_list, va_start, va_arg, va_end.
 *
 * Convention d'appel cdecl (x86 32bits):
 * Les arguments sont pushés SUR LA PILE de droite à gauche.
 * Pour accéder aux arguments variables, on utilise l'adresse du dernier
 * argument fixe (format) pour naviguer sur la pile.
 *
 * SPÉCIFICATEURS SUPPORTÉS:
 *   %c  - Caractère
 *   %s  - Chaîne de caractères
 *   %d  - Entier signé décimal
 *   %u  - Entier non signé décimal
 *   %x  - Entier hexadécimal (minuscules)
 *   %X  - Entier hexadécimal (majuscules)
 *   %p  - Pointeur (0xADDRESS)
 *   %%  - Littéral '%'
 * ============================================================================= */

#include "types.h"

/* =============================================================================
 * Implémentation portable de va_list sans <stdarg.h>
 *
 * Sur x86 32bits cdecl, les arguments sont empilés de droite à gauche
 * et sont alignés sur 4 octets. On peut donc les parcourir avec un pointeur.
 * ============================================================================= */
typedef char *va_list;

/* Taille d'un argument alignée sur 4 octets (taille d'un registre 32bits) */
#define VA_SIZE(type)   (((sizeof(type) + sizeof(int) - 1) / sizeof(int)) * sizeof(int))

/* va_start: pointer juste après le dernier argument fixe */
#define va_start(ap, last)  ((ap) = (va_list)&(last) + VA_SIZE(last))

/* va_arg: lire le prochain argument du type donné et avancer le pointeur */
#define va_arg(ap, type)    (*(type *)(((ap) += VA_SIZE(type)) - VA_SIZE(type)))

/* va_end: nettoyage (rien à faire en x86 simple) */
#define va_end(ap)          ((ap) = (va_list)0)

/* =============================================================================
 * Fonctions
 * ============================================================================= */

/* printk: afficher sur le TTY actif (comme printf, sans return) */
void printk(const char *fmt, ...);

/* kprintf: variante qui retourne le nombre de caractères écrits */
int  kprintf(const char *fmt, ...);

/* vprintk: version avec va_list déjà construite */
int  vprintk(const char *fmt, va_list ap);

#endif /* PRINTK_H */
