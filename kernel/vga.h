#ifndef VGA_H
#define VGA_H

/* =============================================================================
 * kernel/vga.h - Interface du driver VGA Text Mode
 * =============================================================================
 *
 * CONCEPT: VGA TEXT MODE
 *
 * Le mode texte VGA (mode 3) est activé par défaut au boot (par le BIOS/UEFI).
 * Il offre une grille de 80 colonnes × 25 lignes de caractères.
 *
 * MÉMOIRE VIDÉO:
 * La mémoire vidéo est mappée à l'adresse physique 0xB8000.
 * Chaque caractère occupe 2 octets consécutifs en mémoire:
 *
 *   Offset 0: Octet du caractère (code ASCII)
 *   Offset 1: Octet d'attribut (couleur)
 *
 * Structure de l'octet d'attribut (8 bits):
 *   Bits 7:   Clignotement (ou bit de poids fort de la couleur fond)
 *   Bits 6-4: Couleur de FOND (background) - 3 bits = 8 couleurs
 *   Bits 3-0: Couleur du TEXTE (foreground) - 4 bits = 16 couleurs
 *
 *   Exemple: 0x0F = fond noir (0x0) + texte blanc brillant (0xF)
 *            0x1E = fond bleu (0x1) + texte jaune (0xE)
 *
 * CURSEUR MATÉRIEL:
 * Le contrôleur CRT VGA (MC6845) gère un curseur clignotant.
 * On le contrôle via deux ports I/O:
 *   Port 0x3D4: Registre d'index (sélectionne quel paramètre modifier)
 *   Port 0x3D5: Registre de données
 * Registres utilisés:
 *   0x0E: Octet haut de la position du curseur (cursor location high)
 *   0x0F: Octet bas de la position du curseur (cursor location low)
 * La position est un offset linéaire: row * 80 + col
 * ============================================================================= */

#include "types.h"

/* --- Dimensions de l'écran --- */
#define VGA_WIDTH   80
#define VGA_HEIGHT  25

/* --- Adresse physique du framebuffer VGA --- */
#define VGA_MEMORY  ((uint16_t *)0xB8000)

/* --- Ports I/O du contrôleur CRT VGA --- */
#define VGA_CTRL_PORT   0x3D4   /* Port d'index: sélectionner un registre CRT */
#define VGA_DATA_PORT   0x3D5   /* Port de données: lire/écrire le registre sélectionné */
#define VGA_CURSOR_HIGH 0x0E    /* Index du registre "cursor position high" */
#define VGA_CURSOR_LOW  0x0F    /* Index du registre "cursor position low" */

/* =============================================================================
 * Couleurs VGA (16 couleurs standard)
 * Les valeurs correspondent aux 4 bits de l'attribut de couleur
 * ============================================================================= */
typedef enum {
    VGA_COLOR_BLACK         = 0,    /* Noir */
    VGA_COLOR_BLUE          = 1,    /* Bleu foncé */
    VGA_COLOR_GREEN         = 2,    /* Vert foncé */
    VGA_COLOR_CYAN          = 3,    /* Cyan foncé */
    VGA_COLOR_RED           = 4,    /* Rouge foncé */
    VGA_COLOR_MAGENTA       = 5,    /* Magenta foncé */
    VGA_COLOR_BROWN         = 6,    /* Marron */
    VGA_COLOR_LIGHT_GREY    = 7,    /* Gris clair */
    VGA_COLOR_DARK_GREY     = 8,    /* Gris foncé */
    VGA_COLOR_LIGHT_BLUE    = 9,    /* Bleu clair */
    VGA_COLOR_LIGHT_GREEN   = 10,   /* Vert clair */
    VGA_COLOR_LIGHT_CYAN    = 11,   /* Cyan clair */
    VGA_COLOR_LIGHT_RED     = 12,   /* Rouge clair */
    VGA_COLOR_LIGHT_MAGENTA = 13,   /* Magenta clair */
    VGA_COLOR_YELLOW        = 14,   /* Jaune */
    VGA_COLOR_WHITE         = 15,   /* Blanc pur */
} vga_color_t;

/* --- Construire un octet d'attribut depuis deux couleurs --- */
static inline uint8_t vga_make_attr(vga_color_t fg, vga_color_t bg)
{
    /* fg occupe les 4 bits bas, bg les 4 bits hauts (bits 4-6, le bit 7 = blink) */
    return (uint8_t)((bg << 4) | (fg & 0x0F));
}

/* --- Construire une entrée VGA (char + attribut) --- */
static inline uint16_t vga_make_entry(char c, uint8_t attr)
{
    /* Octet bas = ASCII, octet haut = attribut couleur */
    return (uint16_t)((uint16_t)(uint8_t)c | ((uint16_t)attr << 8));
}

/* =============================================================================
 * API publique du driver VGA
 * ============================================================================= */

/* Initialisation: efface l'écran, positionne le curseur en (0,0) */
void vga_init(void);

/* Écrire un seul caractère à la position courante du curseur */
void vga_putchar(char c);

/* Écrire une chaîne de caractères */
void vga_puts(const char *s);

/* Écrire un caractère à une position absolue avec une couleur donnée */
void vga_putchar_at(char c, uint8_t attr, int col, int row);

/* Effacer tout l'écran (remplir d'espaces avec la couleur par défaut) */
void vga_clear(void);

/* Changer la couleur courante (fg = texte, bg = fond) */
void vga_set_color(vga_color_t fg, vga_color_t bg);

/* Récupérer la couleur courante */
uint8_t vga_get_color(void);

/* Déplacer le curseur à une position absolue */
void vga_move_cursor(int col, int row);

/* Faire défiler l'écran d'une ligne vers le haut (scroll up) */
void vga_scroll(void);

/* Mettre à jour le curseur matériel CRT selon la position courante */
void vga_update_hw_cursor(void);

/* Masquer/afficher le curseur matériel */
void vga_cursor_enable(bool enable);

/* Sauvegarder le contenu de l'écran VGA dans un buffer */
void vga_save_buffer(uint16_t *buf);

/* Restaurer un buffer dans l'écran VGA */
void vga_restore_buffer(const uint16_t *buf);

/* Obtenir/régler la position du curseur */
int  vga_get_col(void);
int  vga_get_row(void);
void vga_set_cursor_pos(int col, int row);

#endif /* VGA_H */
