/* =============================================================================
 * kernel/kernel.c - Point d'entrée C du noyau
 * =============================================================================
 *
 * kernel_main() est appelée depuis boot.asm après initialisation du stack.
 * C'est le "main()" de notre noyau.
 *
 * ORDRE D'INITIALISATION (ordre critique):
 *   1. GDT  - Segmentation mémoire (base pour tout le reste)
 *   2. IDT  - Table des interruptions (sans elle, tout crash = freeze total)
 *   3. PIC  - Remapper les IRQ (éviter conflits avec les exceptions CPU)
 *   4. VGA  - Affichage (pour pouvoir montrer quelque chose)
 *   5. TTY  - Terminaux virtuels (couche au-dessus du VGA)
 *   6. STI  - Activer les interruptions (seulement après PIC + IDT prêts!)
 *   7. KBD  - Clavier (nécessite les interruptions actives)
 * ============================================================================= */

#include "types.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "vga.h"
#include "tty.h"
#include "keyboard.h"
#include "printk.h"
#include "string.h"

/* =============================================================================
 * Structure multiboot_info - Informations passées par GRUB
 *
 * GRUB remplit cette structure et passe son adresse dans ebx.
 * Elle contient: taille de la RAM, liste des modules chargés, ligne de commande, etc.
 * On n'utilise que les champs mémoire pour l'instant.
 * ============================================================================= */
typedef struct PACKED {
    uint32_t flags;         /* Bits indiquant quels champs sont valides */
    uint32_t mem_lower;     /* Mémoire basse en KB (typiquement 640 KB) */
    uint32_t mem_upper;     /* Mémoire haute en KB (RAM au-delà de 1MB) */
    uint32_t boot_device;
    uint32_t cmdline;       /* Adresse de la ligne de commande du noyau */
    uint32_t mods_count;
    uint32_t mods_addr;
    /* ... autres champs non utilisés ... */
} multiboot_info_t;

/* Numéro magique Multiboot envoyé par le bootloader (différent du header!) */
#define MULTIBOOT_BOOTLOADER_MAGIC  0x2BADB002

/* =============================================================================
 * print_banner - Afficher la bannière de démarrage
 * ============================================================================= */
static void print_banner(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    // Utilisation des codes hexadécimaux CP437 pour les bordures
    printk("\xC9\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBB\n");
    printk("\xBA                        KFS-1 - Kernel From Scratch                         \xBA\n");
    printk("\xBA                    Grub + Boot + Screen  |  42 Project                     \xBA\n");
    printk("\xC8\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBC\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
/*
static void print_banner(void)
{
    vga_set_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
    printk("╔════════════════════════════════════════════════════════════════════════════╗\n");
    printk("║                        KFS-1 - Kernel From Scratch                         ║\n");
    printk("║                    Grub + Boot + Screen  |  42 Project                     ║\n");
    printk("╚════════════════════════════════════════════════════════════════════════════╝\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}
*/
/* =============================================================================
 * print_memory_info - Afficher les informations mémoire fournies par GRUB
 * ============================================================================= */
static void print_memory_info(multiboot_info_t *mbi)
{
    /* Vérifier que GRUB a bien fourni les infos mémoire (bit 0 des flags) */
    if (mbi && (mbi->flags & 0x1)) {
        vga_set_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        printk("[MEM] Lower: %u KB  |  Upper: %u KB  |  Total: ~%u MB\n",
               mbi->mem_lower,
               mbi->mem_upper,
               (mbi->mem_lower + mbi->mem_upper) / 1024);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }
}

/* =============================================================================
 * print_tty_hint - Afficher les raccourcis clavier pour les terminaux
 * ============================================================================= */
static void print_tty_hint(void)
{
    vga_set_color(VGA_COLOR_YELLOW, VGA_COLOR_BLACK);
    printk("[TTY] Use Alt+F1..F6 to switch between 6 virtual terminals\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
}

/* =============================================================================
 * kernel_main - Fonction principale du noyau
 *
 * @magic: Doit valoir MULTIBOOT_BOOTLOADER_MAGIC (0x2BADB002)
 * @mbi:   Pointeur vers la structure multiboot_info remplie par GRUB
 * ============================================================================= */
void kernel_main(uint32_t magic, multiboot_info_t *mbi)
{
    /* =========================================================================
     * 1. Initialiser la GDT
     *    Configure la segmentation mémoire plate (tout l'espace adressable)
     *    DOIT être fait en premier: l'IDT en dépend (sélecteur 0x08)
     * ========================================================================= */
    gdt_init();

    /* =========================================================================
     * 2. Initialiser l'IDT (sans activer les interruptions encore!)
     *    Enregistre les handlers pour les 32 exceptions + 16 IRQ
     * ========================================================================= */
    idt_init();

    /* =========================================================================
     * 3. Initialiser et remapper le PIC
     *    AVANT d'activer les interruptions! Sans remapping, IRQ0-7 collisionneraient
     *    avec les exceptions CPU 8-15.
     * ========================================================================= */
    pic_init();
    /* Masquer toutes les IRQ par défaut (on les active individuellement) */
    for (int i = 0; i < 16; i++)
        pic_mask_irq((uint8_t)i);
    /* Démasquer seulement l'IRQ2 (cascade, nécessaire pour IRQ 8-15) */
    pic_unmask_irq(IRQ_CASCADE);

    /* =========================================================================
     * 4. Initialiser le driver VGA
     *    Efface l'écran, configure le curseur, prêt à afficher
     * ========================================================================= */
    vga_init();

    /* =========================================================================
     * 5. Initialiser les terminaux virtuels (APRÈS VGA)
     *    Configure les 6 TTY, TTY0 est actif
     * ========================================================================= */
    tty_init();

    /* =========================================================================
     * 6. Activer les interruptions (STI)
     *    MAINTENANT que GDT + IDT + PIC sont configurés, c'est sûr
     * ========================================================================= */
    __asm__ volatile ("sti");

    /* =========================================================================
     * 7. Initialiser le clavier (APRÈS STI car utilise les interruptions)
     * ========================================================================= */
    keyboard_init();

    /* =========================================================================
     * Affichage initial
     * ========================================================================= */

    /* Vérifier que GRUB nous a bien chargé (sanity check) */
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
        printk("[ERROR] Bad multiboot magic: 0x%x (expected 0x2BADB002)\n", magic);
        vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    }

    /* Bannière */
    print_banner();

    /* Infos mémoire */
    print_memory_info(mbi);

    /* =========================================================================
     * AFFICHAGE REQUIS PAR LE SUJET: "42"
     * ========================================================================= */
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    printk("\n");
    printk("                                 \xDB\xDB   \xDB\xDB\xDB\xDB\xDB \n");
    printk("                                \xDB\xDB        \xDB\xDB\n");
    printk("                               \xDB\xDB\xDB\xDB\xDB\xDB \xDB\xDB\xDB\xDB\n");
    printk("                                  \xDB\xDB  \xDB\xDB\n");
    printk("                                  \xDB\xDB  \xDB\xDB\xDB\xDB\xDB\xDB\n");
    printk("\n");
    /*
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    printk("\n");
    printk("    ██╗  ██╗██████╗ \n");
    printk("    ██║  ██║╚════██╗\n");
    printk("    ███████║ █████╔╝\n");
    printk("    ╚════██║██╔═══╝ \n");
    printk("         ██║███████╗\n");
    printk("         ╚═╝╚══════╝\n");
    printk("\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    */
    /* Raccourcis TTY */
    print_tty_hint();

    /* Séparateur */
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    printk("______________________________________________________________________________\n");
    vga_set_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

    /* Prompt */
    vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    printk("kfs1> ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);

    /* =========================================================================
     * Boucle principale du noyau
     *
     * Le noyau ne doit JAMAIS se terminer. On boucle indéfiniment en
     * attendant des interruptions (HLT = économie d'énergie).
     *
     * Chaque frappe clavier déclenche IRQ1 → keyboard_irq_handler()
     * → tty_putchar() → vga_putchar() → affichage à l'écran.
     * ========================================================================= */
    /* Boucle infinie: HLT met le CPU en veille jusqu'à la prochaine interruption.
     * Plus efficace qu'un busy-wait (économie d'énergie, pas de chauffe inutile). */
    while (1)
        __asm__ volatile ("hlt");
}
