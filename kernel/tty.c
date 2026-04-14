/* =============================================================================
 * kernel/tty.c - Gestion des terminaux virtuels
 * ============================================================================= */

#include "tty.h"
#include "vga.h"
#include "string.h"

/* --- Tableau des TTY --- */
static tty_t tty_list[TTY_COUNT];

/* --- Index du TTY actuellement affiché --- */
static int active_tty = 0;

/* =============================================================================
 * tty_init - Initialiser tous les terminaux virtuels
 *
 * Tous les TTY commencent avec un écran noir et le curseur en (0,0).
 * Le TTY 0 est actif par défaut (déjà initialisé par vga_init).
 * ============================================================================= */
void tty_init(void)
{
    for (int i = 0; i < TTY_COUNT; i++) {
        /* Remplir le buffer avec des espaces (noir sur noir) */
        uint16_t blank = vga_make_entry(' ',
                            vga_make_attr(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        for (int j = 0; j < VGA_WIDTH * VGA_HEIGHT; j++)
            tty_list[i].buffer[j] = blank;

        tty_list[i].col  = 0;
        tty_list[i].row  = 0;
        tty_list[i].attr = vga_make_attr(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }

    /* Le TTY 0 récupère le contenu actuel de la mémoire vidéo (initialisé par vga_init) */
    vga_save_buffer(tty_list[0].buffer);
    tty_list[0].col  = vga_get_col();
    tty_list[0].row  = vga_get_row();
    tty_list[0].attr = vga_get_color();

    active_tty = 0;
}

/* =============================================================================
 * tty_switch - Basculer vers le terminal virtuel numéro tty_num
 *
 * ALGORITHME:
 *   1. Sauvegarder l'état du TTY actif (buffer + position curseur + couleur)
 *   2. Afficher le buffer du nouveau TTY dans la mémoire vidéo
 *   3. Restaurer la position du curseur et la couleur du nouveau TTY
 * ============================================================================= */
void tty_switch(int tty_num)
{
    if (tty_num < 0 || tty_num >= TTY_COUNT || tty_num == active_tty)
        return;

    /* Sauvegarder l'état du TTY actuellement visible */
    vga_save_buffer(tty_list[active_tty].buffer);
    tty_list[active_tty].col  = vga_get_col();
    tty_list[active_tty].row  = vga_get_row();
    tty_list[active_tty].attr = vga_get_color();

    /* Activer le nouveau TTY */
    active_tty = tty_num;

    /* Restaurer son contenu dans la mémoire vidéo */
    vga_restore_buffer(tty_list[active_tty].buffer);

    /* Restaurer la position du curseur et la couleur */
    vga_set_cursor_pos(tty_list[active_tty].col, tty_list[active_tty].row);
    /* (vga_set_cursor_pos appelle vga_update_hw_cursor automatiquement) */

    /* Restaurer la couleur dans le driver VGA */
    uint8_t attr = tty_list[active_tty].attr;
    /* Extraire fg et bg depuis l'attribut (bg = bits 4-6, fg = bits 0-3) */
    vga_color_t fg = (vga_color_t)(attr & 0x0F);
    vga_color_t bg = (vga_color_t)((attr >> 4) & 0x07);
    vga_set_color(fg, bg);
}

/* =============================================================================
 * tty_putchar - Écrire un caractère sur le TTY actif
 *
 * Délègue directement à vga_putchar() puisque le TTY actif EST
 * la mémoire vidéo visible. La sauvegarde du buffer se fait lors
 * d'un tty_switch().
 * ============================================================================= */
void tty_putchar(char c)
{
    vga_putchar(c);
}

/* =============================================================================
 * tty_puts - Écrire une chaîne sur le TTY actif
 * ============================================================================= */
void tty_puts(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

/* =============================================================================
 * tty_set_color - Changer la couleur sur le TTY actif
 * ============================================================================= */
void tty_set_color(vga_color_t fg, vga_color_t bg)
{
    vga_set_color(fg, bg);
    tty_list[active_tty].attr = vga_get_color();
}

/* =============================================================================
 * tty_clear - Effacer le TTY actif
 * ============================================================================= */
void tty_clear(void)
{
    vga_clear();
}

/* =============================================================================
 * tty_get_active - Retourner le numéro du TTY actif
 * ============================================================================= */
int tty_get_active(void)
{
    return active_tty;
}
