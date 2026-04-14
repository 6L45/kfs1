/* =============================================================================
 * kernel/vga.c - Driver VGA Text Mode
 * =============================================================================
 *
 * Implémentation du driver d'affichage en mode texte VGA.
 * Supporte: couleurs, scroll automatique, curseur matériel.
 *
 * PORT I/O EN ASSEMBLEUR INLINE:
 * En C pour noyau, on ne peut pas utiliser les fonctions libc pour accéder
 * aux ports I/O. On utilise des instructions ASM inline:
 *   outb(port, val): écrire val sur le port I/O (instruction OUT)
 *   inb(port): lire une valeur depuis le port I/O (instruction IN)
 *
 * Les ports I/O x86 sont un espace d'adressage séparé de la mémoire (0-65535).
 * L'instruction OUT envoie une donnée à un périphérique via son port.
 * ============================================================================= */

#include "vga.h"
#include "string.h"

/* =============================================================================
 * Fonctions d'accès aux ports I/O (inline ASM)
 * ============================================================================= */

/* Écrire un octet sur un port I/O */
static inline void outb(uint16_t port, uint8_t val)
{
    /* "a" = registre al/ax/eax (valeur), "Nd" = constante ou registre dx (port) */
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

/* Lire un octet depuis un port I/O */
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* =============================================================================
 * État interne du driver VGA
 * ============================================================================= */
static struct {
    int       col;              /* Colonne courante du curseur (0-79) */
    int       row;              /* Ligne courante du curseur (0-24) */
    uint8_t   attr;             /* Attribut couleur courant */
    uint16_t *fb;               /* Pointeur vers le framebuffer VGA (0xB8000) */
} vga_state;

/* =============================================================================
 * Fonctions internes
 * ============================================================================= */

/* Calculer l'offset linéaire dans le framebuffer: row * 80 + col */
static inline int vga_offset(int col, int row)
{
    return row * VGA_WIDTH + col;
}

/* =============================================================================
 * vga_init - Initialiser le driver VGA
 * Efface l'écran, configure la couleur par défaut, place le curseur en (0,0)
 * ============================================================================= */
void vga_init(void)
{
    vga_state.fb   = VGA_MEMORY;
    vga_state.col  = 0;
    vga_state.row  = 0;
    vga_state.attr = vga_make_attr(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    vga_clear();
    vga_cursor_enable(true);
    vga_update_hw_cursor();
}

/* =============================================================================
 * vga_clear - Effacer tout l'écran
 * Remplit le framebuffer avec des espaces et l'attribut courant
 * ============================================================================= */
void vga_clear(void)
{
    uint16_t blank = vga_make_entry(' ', vga_state.attr);
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        vga_state.fb[i] = blank;
    vga_state.col = 0;
    vga_state.row = 0;
    vga_update_hw_cursor();
}

/* =============================================================================
 * vga_scroll - Faire défiler l'écran d'une ligne vers le haut
 *
 * ALGORITHME:
 *   1. Copier les lignes 1..24 vers les lignes 0..23 (memmove)
 *   2. Effacer la dernière ligne (remplir d'espaces)
 *   3. Décrémenter la ligne du curseur
 * ============================================================================= */
void vga_scroll(void)
{
    uint16_t blank = vga_make_entry(' ', vga_state.attr);

    /* Déplacer toutes les lignes d'une position vers le haut */
    /* Source: ligne 1 (offset 80), Destination: ligne 0 (offset 0) */
    /* Taille: (25-1) * 80 = 24 lignes × 80 cellules × 2 octets */
    memmove(vga_state.fb,
            vga_state.fb + VGA_WIDTH,
            (VGA_HEIGHT - 1) * VGA_WIDTH * sizeof(uint16_t));

    /* Effacer la dernière ligne */
    for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++)
        vga_state.fb[i] = blank;

    /* Le curseur reste sur la dernière ligne */
    vga_state.row = VGA_HEIGHT - 1;
}

/* =============================================================================
 * vga_putchar_at - Écrire un caractère à une position absolue
 * ============================================================================= */
void vga_putchar_at(char c, uint8_t attr, int col, int row)
{
    vga_state.fb[vga_offset(col, row)] = vga_make_entry(c, attr);
}

/* =============================================================================
 * vga_putchar - Écrire un caractère à la position courante du curseur
 *
 * Gère les caractères spéciaux:
 *   '\n' (newline): aller au début de la ligne suivante
 *   '\r' (carriage return): aller au début de la ligne courante
 *   '\t' (tab): avancer au prochain multiple de 8
 *   '\b' (backspace): reculer d'un caractère (et effacer)
 * ============================================================================= */
void vga_putchar(char c)
{
    switch (c) {
        case '\n':
            /* Newline: aller à la colonne 0 de la ligne suivante */
            vga_state.col = 0;
            vga_state.row++;
            break;

        case '\r':
            /* Carriage return: retour en début de ligne */
            vga_state.col = 0;
            break;

        case '\t':
            /* Tabulation: avancer au prochain multiple de 8 */
            vga_state.col = (vga_state.col + 8) & ~7;
            break;

        case '\b':
            /* Backspace: reculer et effacer le caractère précédent */
            if (vga_state.col > 0) {
                vga_state.col--;
            } else if (vga_state.row > 0) {
                /* Remonter à la fin de la ligne précédente */
                vga_state.row--;
                vga_state.col = VGA_WIDTH - 1;
            }
            vga_state.fb[vga_offset(vga_state.col, vga_state.row)]
                = vga_make_entry(' ', vga_state.attr);
            break;

        default:
            /* Caractère imprimable: l'écrire dans le framebuffer */
            vga_state.fb[vga_offset(vga_state.col, vga_state.row)]
                = vga_make_entry(c, vga_state.attr);
            vga_state.col++;
            break;
    }

    /* Wrap: si on dépasse la fin de ligne, aller à la ligne suivante */
    if (vga_state.col >= VGA_WIDTH) {
        vga_state.col = 0;
        vga_state.row++;
    }

    /* Scroll: si on dépasse le bas de l'écran */
    if (vga_state.row >= VGA_HEIGHT)
        vga_scroll();

    /* Mettre à jour le curseur matériel */
    vga_update_hw_cursor();
}

/* =============================================================================
 * vga_puts - Écrire une chaîne de caractères
 * ============================================================================= */
void vga_puts(const char *s)
{
    while (*s)
        vga_putchar(*s++);
}

/* =============================================================================
 * vga_set_color - Changer la couleur d'écriture courante
 * ============================================================================= */
void vga_set_color(vga_color_t fg, vga_color_t bg)
{
    vga_state.attr = vga_make_attr(fg, bg);
}

uint8_t vga_get_color(void)
{
    return vga_state.attr;
}

/* =============================================================================
 * vga_move_cursor - Déplacer le curseur logique et matériel
 * ============================================================================= */
void vga_move_cursor(int col, int row)
{
    if (col >= 0 && col < VGA_WIDTH && row >= 0 && row < VGA_HEIGHT) {
        vga_state.col = col;
        vga_state.row = row;
        vga_update_hw_cursor();
    }
}

/* =============================================================================
 * vga_update_hw_cursor - Mettre à jour la position du curseur matériel CRT
 *
 * PROTOCOLE CRT MC6845:
 *   1. Écrire l'index du registre souhaité dans le port 0x3D4
 *   2. Lire/Écrire la valeur dans le port 0x3D5
 *
 * La position est stockée sur 16 bits (max 80×25=2000 < 65536).
 * On envoie d'abord l'octet haut (bits 15-8), puis l'octet bas (bits 7-0).
 * ============================================================================= */
void vga_update_hw_cursor(void)
{
    uint16_t pos = (uint16_t)(vga_state.row * VGA_WIDTH + vga_state.col);

    /* Envoyer l'octet haut de la position */
    outb(VGA_CTRL_PORT, VGA_CURSOR_HIGH);
    outb(VGA_DATA_PORT, (uint8_t)((pos >> 8) & 0xFF));

    /* Envoyer l'octet bas de la position */
    outb(VGA_CTRL_PORT, VGA_CURSOR_LOW);
    outb(VGA_DATA_PORT, (uint8_t)(pos & 0xFF));
}

/* =============================================================================
 * vga_cursor_enable - Activer ou désactiver le curseur clignotant
 *
 * Registres utilisés:
 *   0x0A (Cursor Start Register): bits 4-0 = ligne de début du curseur
 *         bit 5 = Cursor Disable (1 = désactivé)
 *   0x0B (Cursor End Register):   bits 4-0 = ligne de fin du curseur
 * ============================================================================= */
void vga_cursor_enable(bool enable)
{
    if (enable) {
        /* Activer le curseur: lignes 14 à 15 (bas de la cellule de caractère) */
        outb(VGA_CTRL_PORT, 0x0A);
        outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xC0) | 14);
        outb(VGA_CTRL_PORT, 0x0B);
        outb(VGA_DATA_PORT, (inb(VGA_DATA_PORT) & 0xE0) | 15);
    } else {
        /* Désactiver le curseur: mettre le bit 5 du registre 0x0A */
        outb(VGA_CTRL_PORT, 0x0A);
        outb(VGA_DATA_PORT, 0x20);  /* Bit 5 = Cursor Disable */
    }
}

/* =============================================================================
 * vga_save_buffer / vga_restore_buffer - Sauvegarde/restauration de l'écran
 * Utilisé par le gestionnaire de terminaux virtuels (tty.c)
 * ============================================================================= */
void vga_save_buffer(uint16_t *buf)
{
    memcpy(buf, vga_state.fb, VGA_WIDTH * VGA_HEIGHT * sizeof(uint16_t));
}

void vga_restore_buffer(const uint16_t *buf)
{
    memcpy(vga_state.fb, buf, VGA_WIDTH * VGA_HEIGHT * sizeof(uint16_t));
}

/* --- Accesseurs pour la position du curseur --- */
int vga_get_col(void)   { return vga_state.col; }
int vga_get_row(void)   { return vga_state.row; }

void vga_set_cursor_pos(int col, int row)
{
    vga_state.col = col;
    vga_state.row = row;
    vga_update_hw_cursor();
}
