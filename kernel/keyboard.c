/* =============================================================================
 * kernel/keyboard.c - Driver clavier PS/2 avec support étendu
 * ============================================================================= */

#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "tty.h"

/* --- Accès aux ports I/O --- */
static inline void outb(uint16_t port, uint8_t val)
{
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}
static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* =============================================================================
 * TABLE DE CORRESPONDANCE SCANCODE → ASCII
 *
 * Index = scancode Set 1 (touche enfoncée)
 * Valeur = caractère ASCII correspondant (0 = pas de caractère imprimable)
 *
 * LAYOUT: AZERTY Français
 * (Pour QWERTY, il faudrait une table différente)
 * ============================================================================= */

/* Table normale (sans Shift) */
// static const char scancode_map_normal[128] = {
// /*00*/  0,    0,    '&',  '\xE9','\"', '\'', '(',  '-',
// /*08*/  '\xE8',  '_',  '\xE7', '\xE0', ')',  '=',  '\b', '\t',
// /*10*/  'a',  'z',  'e',  'r',  't',  'y',  'u',  'i',
// /*18*/  'o',  'p',  '^',  '$',  '\n', 0,    'q',  's',
// /*20*/  'd',  'f',  'g',  'h',  'j',  'k',  'l',  'm',
// /*28*/  '\xF9', '`',  0,    '*',  'w',  'x',  'c',  'v',
// /*30*/  'b',  'n',  ',',  ';',  ':',  '!',  0,    '*',
// /*38*/  0,    ' ',  0,    0,    0,    0,    0,    0,
// /*40*/  0,    0,    0,    0,    0,    0,    0,    '7',
// /*48*/  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
// /*50*/  '2',  '3',  '0',  '.',  0,    0,    0,    0,
// /*58*/  0,    0,    0,    0,    0,    0,    0,    0,
// /*60*/  0,    0,    0,    0,    0,    0,    0,    0,
// /*68*/  0,    0,    0,    0,    0,    0,    0,    0,
// /*70*/  0,    0,    0,    0,    0,    0,    0,    0,
// /*78*/  0,    0,    0,    0,    0,    0,    0,    0,
// };

// /* Table avec Shift enfoncé */
// static const char scancode_map_shift[128] = {
// /*00*/  0,    0,    '1',  '2',  '3',  '4',  '5',  '6',
// /*08*/  '7',  '8',  '9',  '0',  '\xB0', '+', '\b', '\t',
// /*10*/  'A',  'Z',  'E',  'R',  'T',  'Y',  'U',  'I',
// /*18*/  'O',  'P',  '\xA8', '\xA3','\n', 0,   'Q',  'S',
// /*20*/  'D',  'F',  'G',  'H',  'J',  'K',  'L',  'M',
// /*28*/  '%',  '~',  0,    '\xB5','W',  'X',  'C',  'V',
// /*30*/  'B',  'N',  '?',  '.',  '/',  '\xA7', 0,   '*',
// /*38*/  0,    ' ',  0,    0,    0,    0,    0,    0,
// /*40*/  0,    0,    0,    0,    0,    0,    0,    '7',
// /*48*/  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
// /*50*/  '2',  '3',  '0',  '.',  0,    0,    '<',  0,
// /*58*/  0,    0,    0,    0,    0,    0,    0,    0,
// };
/* Table normale (sans Shift) - QWERTY US (Set 1) */
static const char scancode_map_normal[128] = {
/*00*/  0,    0,    '1',  '2',  '3',  '4',  '5',  '6',
/*08*/  '7',  '8',  '9',  '0',  '-',  '=',  '\b', '\t',
/*10*/  'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',
/*18*/  'o',  'p',  '[',  ']',  '\n', 0,    'a',  's',
/*20*/  'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',
/*28*/  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v',
/*30*/  'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',
/*38*/  0,    ' ',  0,    0,    0,    0,    0,    0,
/*40*/  0,    0,    0,    0,    0,    0,    0,    '7',
/*48*/  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
/*50*/  '2',  '3',  '0',  '.',  0,    0,    0,    0,
/*58*/  0,    0,    0,    0,    0,    0,    0,    0,
/*60*/  0,    0,    0,    0,    0,    0,    0,    0,
/*68*/  0,    0,    0,    0,    0,    0,    0,    0,
/*70*/  0,    0,    0,    0,    0,    0,    0,    0,
/*78*/  0,    0,    0,    0,    0,    0,    0,    0,
};

/* Table avec Shift enfoncé - QWERTY US (Set 1) */
static const char scancode_map_shift[128] = {
/*00*/  0,    0,    '!',  '@',  '#',  '$',  '%',  '^',
/*08*/  '&',  '*',  '(',  ')',  '_',  '+',  '\b', '\t',
/*10*/  'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',
/*18*/  'O',  'P',  '{',  '}',  '\n', 0,    'A',  'S',
/*20*/  'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',
/*28*/  '\"', '~',  0,    '|',  'Z',  'X',  'C',  'V',
/*30*/  'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',
/*38*/  0,    ' ',  0,    0,    0,    0,    0,    0,
/*40*/  0,    0,    0,    0,    0,    0,    0,    '7',
/*48*/  '8',  '9',  '-',  '4',  '5',  '6',  '+',  '1',
/*50*/  '2',  '3',  '0',  '.',  0,    0,    '<',  0,
/*58*/  0,    0,    0,    0,    0,    0,    0,    0,
};

/* =============================================================================
 * État interne du clavier
 * ============================================================================= */
static struct {
    bool    shift;      /* Shift gauche ou droit enfoncé */
    bool    ctrl;       /* Ctrl enfoncé */
    bool    alt;        /* Alt enfoncé */
    bool    capslock;   /* CapsLock actif (toggle) */

    /* Buffer circulaire pour stocker les caractères en attente */
    char    buf[256];
    uint8_t buf_head;   /* Index de lecture */
    uint8_t buf_tail;   /* Index d'écriture */
} kbd;

/* =============================================================================
 * keyboard_irq_handler - Handler IRQ1 appelé à chaque frappe de touche
 *
 * Cette fonction est appelée depuis irq_handler() dans idt.c.
 * Elle lit le scancode, le décode, et place le caractère dans le buffer.
 * ============================================================================= */
static void keyboard_irq_handler(registers_t *regs)
{
    (void)regs;     /* Paramètre non utilisé, éviter le warning */

    /* Lire le scancode depuis le port de données */
    uint8_t scancode = inb(KBD_DATA_PORT);

    /* --- Gérer les touches étendues (préfixe 0xE0) --- */
    if (scancode == 0xE0) {
        /* Prochain octet = scancode étendu (flèches, del, etc.) */
        /* Pour l'instant on lit le prochain byte et on l'ignore */
        return;
    }

    /* --- Touche RELÂCHÉE (break code = scancode | 0x80) --- */
    if (scancode & KEY_RELEASE_MASK) {
        uint8_t key = scancode & ~KEY_RELEASE_MASK;
        if (key == KEY_LSHIFT || key == KEY_RSHIFT) kbd.shift = false;
        if (key == KEY_LCTRL)                       kbd.ctrl  = false;
        if (key == KEY_LALT)                        kbd.alt   = false;
        return;
    }

    /* --- Touche ENFONCÉE (make code) --- */
    switch (scancode) {
        case KEY_LSHIFT:
        case KEY_RSHIFT:
            kbd.shift = true;
            return;

        case KEY_LCTRL:
            kbd.ctrl = true;
            return;

        case KEY_LALT:
            kbd.alt = true;
            return;

        case KEY_CAPSLOCK:
            /* CapsLock = toggle (inverse l'état à chaque pression) */
            kbd.capslock = !kbd.capslock;
            return;

        /* --- BONUS: Alt+F1..F6 = changer de terminal virtuel --- */
        case KEY_F1: if (kbd.alt) { tty_switch(0); return; } break;
        case KEY_F2: if (kbd.alt) { tty_switch(1); return; } break;
        case KEY_F3: if (kbd.alt) { tty_switch(2); return; } break;
        case KEY_F4: if (kbd.alt) { tty_switch(3); return; } break;
        case KEY_F5: if (kbd.alt) { tty_switch(4); return; } break;
        case KEY_F6: if (kbd.alt) { tty_switch(5); return; } break;

        default:
            break;
    }

    /* --- Traduire le scancode en caractère ASCII --- */
    if (scancode >= 128)
        return;     /* Scancode hors table, ignorer */

    char c;
    if (kbd.shift) {
        c = scancode_map_shift[scancode];
    } else {
        c = scancode_map_normal[scancode];
    }

    /* CapsLock: inverser la casse des lettres */
    if (kbd.capslock && c >= 'a' && c <= 'z')
        c = (char)(c - 32);     /* minuscule → majuscule */
    else if (kbd.capslock && c >= 'A' && c <= 'Z')
        c = (char)(c + 32);     /* majuscule → minuscule */

    if (c == 0)
        return;     /* Pas de caractère ASCII pour ce scancode */

    /* --- Ajouter le caractère au buffer circulaire --- */
    uint8_t next_tail = (uint8_t)(kbd.buf_tail + 1);
    if (next_tail != kbd.buf_head) {    /* Buffer pas plein */
        kbd.buf[kbd.buf_tail] = c;
        kbd.buf_tail = next_tail;
    }

    /* Afficher le caractère sur le terminal actif (echo) */
    tty_putchar(c);
}

/* =============================================================================
 * keyboard_init - Initialiser le driver clavier
 * ============================================================================= */
void keyboard_init(void)
{
    /* Effacer l'état interne */
    kbd.shift    = false;
    kbd.ctrl     = false;
    kbd.alt      = false;
    kbd.capslock = false;
    kbd.buf_head = 0;
    kbd.buf_tail = 0;

    /* Vider le buffer du contrôleur PS/2 au cas où il y a des données résiduelles */
    while (inb(KBD_STATUS_PORT) & KBD_STATUS_OBF)
        inb(KBD_DATA_PORT);

    /* Enregistrer notre handler pour l'IRQ1 (clavier) */
    idt_register_irq(IRQ_KEYBOARD, keyboard_irq_handler);

    /* Démasquer l'IRQ1 dans le PIC pour commencer à recevoir les interruptions */
    pic_unmask_irq(IRQ_KEYBOARD);
}

/* =============================================================================
 * keyboard_getchar - Lire le prochain caractère (bloquant)
 * Attend qu'un caractère soit disponible dans le buffer
 * ============================================================================= */
char keyboard_getchar(void)
{
    /* Attendre qu'un caractère soit disponible (busy-wait avec HLT) */
    while (kbd.buf_head == kbd.buf_tail) {
        /* HLT: mettre le CPU en veille jusqu'à la prochaine interruption
         * C'est plus efficace qu'une boucle vide (économise l'énergie) */
        __asm__ volatile ("hlt");
    }

    char c = kbd.buf[kbd.buf_head];
    kbd.buf_head = (uint8_t)(kbd.buf_head + 1);
    return c;
}

/* =============================================================================
 * keyboard_has_input - Vrai si un caractère est disponible sans attendre
 * ============================================================================= */
bool keyboard_has_input(void)
{
    return (kbd.buf_head != kbd.buf_tail);
}

/* --- Accesseurs pour l'état des modificateurs --- */
bool keyboard_is_shift(void)    { return kbd.shift; }
bool keyboard_is_ctrl(void)     { return kbd.ctrl; }
bool keyboard_is_alt(void)      { return kbd.alt; }
bool keyboard_is_capslock(void) { return kbd.capslock; }
