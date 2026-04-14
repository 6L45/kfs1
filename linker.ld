/* =============================================================================
 * linker.ld - Script de liaison (Linker Script) pour le noyau KFS-1
 * =============================================================================
 *
 * CONCEPT: LE LINKER SCRIPT
 *
 * Le compilateur produit des fichiers objets (.o) contenant des sections:
 *   .text   : code machine exécutable
 *   .rodata : données constantes (chaînes, tables...)
 *   .data   : données initialisées (variables globales)
 *   .bss    : données non initialisées (mises à 0 au boot)
 *
 * Le linker (ld) fusionne ces sections de tous les .o en un binaire final.
 * Le linker script contrôle:
 *   1. Le POINT D'ENTRÉE: symbole _start (défini dans boot.asm)
 *   2. L'ADRESSE DE CHARGEMENT: où en mémoire placer chaque section
 *   3. L'ORDRE des sections dans le binaire
 *
 * ADRESSE 1MB (0x100000):
 * GRUB charge notre noyau à 1MB en mémoire physique. Pourquoi 1MB?
 *   - Les 640 premiers KB sont la "conventional memory" (RAM BIOS)
 *   - De 640KB à 1MB: zone réservée BIOS/vidéo (VGA à 0xB8000, etc.)
 *   - À partir de 1MB: "extended memory" libre pour le noyau
 *
 * ALIGNEMENT SUR PAGES (4KB = 0x1000):
 * Chaque section est alignée sur 4KB (taille d'une page mémoire).
 * Utile pour la pagination future (chaque section peut avoir ses propres
 * permissions de page: RX pour .text, RO pour .rodata, RW pour .data/.bss)
 * =============================================================================*/

/* Symbole d'entrée: doit correspondre au _start défini dans boot.asm */
ENTRY(_start)

SECTIONS
{
    /* Adresse de début = 1MB (0x100000) */
    /* GRUB charge le noyau ici en mémoire physique */
    . = 1M;

    /* -------------------------------------------------------------------------
     * Section .text - Code exécutable
     * La section .multiboot DOIT être en tête (GRUB cherche le magic dans les
     * 8 premiers KB du fichier). On la force en première.
     * ------------------------------------------------------------------------- */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.multiboot)   /* En-tête Multiboot en tout premier! */
        *(.text)        /* Code de tous les fichiers objets */
        *(.text.*)      /* Variations (inlining, sections nommées) */
    }

    /* -------------------------------------------------------------------------
     * Section .rodata - Données en lecture seule
     * Chaînes de format, tables de constantes, exception_messages[], etc.
     * ------------------------------------------------------------------------- */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* -------------------------------------------------------------------------
     * Section .data - Données initialisées
     * Variables globales avec une valeur initiale non nulle.
     * Ex: static int x = 42;
     * ------------------------------------------------------------------------- */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    /* -------------------------------------------------------------------------
     * Section .bss - Données non initialisées (Block Started by Symbol)
     * Variables globales sans valeur initiale ou initialisées à 0.
     * Ex: static int x; (ou static int x = 0;)
     * Le bootloader/noyau DOIT les mettre à 0 avant usage.
     * GRUB garantit que .bss est initialisé à 0 pour un noyau Multiboot.
     * ------------------------------------------------------------------------- */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)       /* Variables "common" (déclarées extern dans plusieurs fichiers) */
        *(.bss)
        *(.bss.*)
    }

    /* -------------------------------------------------------------------------
     * Symboles exportés pour le noyau
     * Permettent au code C de connaître les adresses de fin de chaque section
     * (utile pour l'allocateur mémoire physique dans KFS-2)
     * ------------------------------------------------------------------------- */
    kernel_end = .;     /* Adresse de fin du noyau en mémoire */

    /* Ignorer tout le reste (sections de debug, etc.) */
    /DISCARD/ : { *(.comment) *(.eh_frame) *(.note*) }
}
