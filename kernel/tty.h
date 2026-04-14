#ifndef TTY_H
#define TTY_H

/* =============================================================================
 * kernel/tty.h - Terminaux Virtuels (Virtual Terminals / TTY)
 * =============================================================================
 *
 * CONCEPT: TERMINAUX VIRTUELS
 *
 * Un terminal virtuel simule un écran indépendant. On peut en avoir plusieurs
 * actifs simultanément et basculer entre eux avec Alt+F1..F6, comme sur Linux
 * (Ctrl+Alt+F1 à F6 dans un système complet).
 *
 * IMPLÉMENTATION:
 * Chaque TTY possède:
 *   - Un buffer VGA de 80×25 cellules (sauvegarde de l'écran)
 *   - Une position de curseur (col, row)
 *   - Une couleur courante
 *
 * Un seul TTY est "actif" à la fois: c'est celui dont le buffer est visible
 * dans la mémoire vidéo 0xB8000. Quand on bascule:
 *   1. Sauvegarder le buffer du TTY actuel dans sa structure
 *   2. Copier le buffer du nouveau TTY dans la mémoire vidéo
 *   3. Mettre à jour le curseur matériel
 * ============================================================================= */

#include "types.h"
#include "vga.h"

/* --- Nombre de terminaux virtuels --- */
#define TTY_COUNT   6

/* --- Structure d'un terminal virtuel --- */
typedef struct {
    uint16_t    buffer[VGA_WIDTH * VGA_HEIGHT]; /* Buffer écran 80×25 */
    int         col;                            /* Position curseur (colonne) */
    int         row;                            /* Position curseur (ligne) */
    uint8_t     attr;                           /* Attribut couleur courant */
} tty_t;

/* --- Fonctions --- */
void tty_init(void);                        /* Initialiser tous les TTY */
void tty_switch(int tty_num);               /* Basculer vers un autre TTY */
void tty_putchar(char c);                   /* Écrire sur le TTY actif */
void tty_puts(const char *s);               /* Écrire une chaîne sur le TTY actif */
void tty_set_color(vga_color_t fg, vga_color_t bg); /* Changer la couleur */
int  tty_get_active(void);                  /* Numéro du TTY actif */

/* Accès direct au TTY courant (pour printk) */
void tty_clear(void);                       /* Effacer le TTY actif */

#endif /* TTY_H */
