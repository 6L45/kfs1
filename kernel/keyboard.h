#ifndef KEYBOARD_H
#define KEYBOARD_H

/* =============================================================================
 * kernel/keyboard.h - Driver clavier PS/2
 * =============================================================================
 *
 * CONCEPT: CLAVIER PS/2
 *
 * Le clavier PS/2 communique via deux ports I/O:
 *   Port 0x60 (DATA):   Lire les scancodes / envoyer des commandes au clavier
 *   Port 0x64 (STATUS): Lire le statut du contrôleur / envoyer des commandes
 *
 * SCANCODES:
 * Quand une touche est pressée/relâchée, le clavier envoie un "scancode".
 * Il existe 3 jeux de scancodes (Set 1, 2, 3). Le PS/2 utilise Set 1 par défaut.
 *
 * Set 1:
 *   - Touche enfoncée (make code):  scancode de 1 octet (ex: 'A' = 0x1E)
 *   - Touche relâchée (break code): scancode | 0x80 (ex: 'A' relâché = 0x9E)
 *   - Touches étendues: préfixe 0xE0 suivi d'un scancode
 *
 * DÉCLENCHEMENT VIA IRQ1:
 * Chaque frappe déclenche l'IRQ1 (INT 33 après remapping).
 * Le handler lit le scancode sur le port 0x60 et le traduit en caractère ASCII.
 *
 * TOUCHES MODIFICATRICES (MODIFIER KEYS):
 * Shift, Ctrl, Alt, CapsLock ne produisent pas de caractère directement.
 * Le driver garde leur état dans des flags et modifie la traduction en conséquence.
 *
 * BONUS: ALT+F1..F6 pour changer de terminal virtuel
 * ============================================================================= */

#include "types.h"

/* --- Ports du contrôleur PS/2 --- */
#define KBD_DATA_PORT   0x60    /* Lire scancode / envoyer données au clavier */
#define KBD_STATUS_PORT 0x64    /* Lire statut du contrôleur 8042 */

/* --- Masque du bit "Output Buffer Full" dans le registre status --- */
#define KBD_STATUS_OBF  0x01    /* Bit 0: output buffer full (donnée disponible) */

/* --- Scancodes spéciaux (Set 1) --- */
#define KEY_ESCAPE      0x01
#define KEY_BACKSPACE   0x0E
#define KEY_TAB         0x0F
#define KEY_ENTER       0x1C
#define KEY_LCTRL       0x1D
#define KEY_LSHIFT      0x2A
#define KEY_RSHIFT      0x36
#define KEY_LALT        0x38
#define KEY_CAPSLOCK    0x3A
#define KEY_F1          0x3B
#define KEY_F2          0x3C
#define KEY_F3          0x3D
#define KEY_F4          0x3E
#define KEY_F5          0x3F
#define KEY_F6          0x40
#define KEY_F7          0x41
#define KEY_F8          0x42
#define KEY_F9          0x43
#define KEY_F10         0x44
#define KEY_F11         0x57
#define KEY_F12         0x58
#define KEY_DELETE      0x53
#define KEY_UP          0x48
#define KEY_DOWN        0x50
#define KEY_LEFT        0x4B
#define KEY_RIGHT       0x4D

/* Un scancode >= 0x80 = touche relâchée (break) */
#define KEY_RELEASE_MASK    0x80

/* --- Fonctions --- */
void    keyboard_init(void);            /* Initialiser le driver (enregistrer IRQ1) */
char    keyboard_getchar(void);         /* Lire le prochain caractère (bloquant) */
bool    keyboard_has_input(void);       /* Vrai si un caractère est disponible */

/* État des touches modificatrices (accessible pour le gestionnaire TTY) */
bool    keyboard_is_shift(void);
bool    keyboard_is_ctrl(void);
bool    keyboard_is_alt(void);
bool    keyboard_is_capslock(void);

#endif /* KEYBOARD_H */
