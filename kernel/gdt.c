/* =============================================================================
 * kernel/gdt.c - Initialisation de la GDT (Global Descriptor Table)
 * ============================================================================= */

#include "gdt.h"
#include "string.h"

/* --- Tableau des descripteurs GDT (en mémoire statique) --- */
static gdt_entry_t    gdt_table[GDT_ENTRIES];

/* --- Registre GDTR --- */
static gdt_register_t gdt_reg;

/* =============================================================================
 * gdt_set_entry - Remplir une entrée GDT
 *
 * @idx:   Index dans le tableau gdt_table
 * @base:  Adresse de base du segment (32 bits)
 * @limit: Taille du segment (20 bits utiles en mode page, 32 bits en octets)
 * @access: Octet d'accès (DPL, type, flags)
 * @flags: Nibble supérieur (granularité, taille)
 *
 * ENCODAGE BIZARRE de la GDT:
 * Intel a conçu ce format dans les années 80 pour assurer la compatibilité
 * avec le 286. La base et la limite sont fragmentées dans l'entrée!
 * ============================================================================= */
static void gdt_set_entry(int idx, uint32_t base, uint32_t limit,
                          uint8_t access, uint8_t flags)
{
    gdt_entry_t *e = &gdt_table[idx];

    /* Encoder la LIMITE: bits 0-15 dans limit_low, bits 16-19 dans flags */
    e->limit_low        = (uint16_t)(limit & 0xFFFF);
    e->flags_limit_high = (uint8_t)((flags & 0xF0) | ((limit >> 16) & 0x0F));

    /* Encoder la BASE: fragmentée en 3 morceaux */
    e->base_low  = (uint16_t)(base & 0xFFFF);
    e->base_mid  = (uint8_t)((base >> 16) & 0xFF);
    e->base_high = (uint8_t)((base >> 24) & 0xFF);

    e->access = access;
}

/* =============================================================================
 * gdt_load - Charger la GDT via LGDT et recharger les registres de segments
 *
 * POURQUOI L'ASSEMBLEUR INLINE?
 * L'instruction LGDT ne peut être exécutée qu'en assembleur.
 * Après LGDT, on doit faire un "far jump" pour recharger CS avec le nouveau
 * sélecteur de code (un simple MOV ne peut pas modifier CS directement).
 * Le far jump utilise la syntaxe: ljmp selector:offset
 * ============================================================================= */
static void gdt_load(void)
{
    gdt_reg.limit = (uint16_t)(sizeof(gdt_table) - 1);
    gdt_reg.base  = (uint32_t)&gdt_table;

    __asm__ volatile (
        "lgdt (%0)\n"           /* Charger le registre GDTR */
        /* Far jump: recharge CS avec le sélecteur du segment code noyau */
        /* La syntaxe AT&T pour un ljmp: ljmp $selector, $offset */
        "ljmp $0x08, $1f\n"
        "1:\n"
        /* Recharger tous les registres de segment data avec 0x10 */
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        : : "r"(&gdt_reg) : "memory", "eax"
    );
}

/* =============================================================================
 * gdt_init - Configurer et charger la GDT
 * ============================================================================= */
void gdt_init(void)
{
    /* Effacer le tableau */
    memset(gdt_table, 0, sizeof(gdt_table));

    /* -----------------------------------------------------------------------
     * Entrée 0: NULL descriptor (obligatoire, toujours à zéro)
     * Le CPU génère une exception si on tente d'utiliser le sélecteur 0x00
     * ----------------------------------------------------------------------- */
    gdt_set_entry(GDT_NULL, 0, 0, 0, 0);

    /* -----------------------------------------------------------------------
     * Entrée 1: Code Segment noyau (ring 0)
     * Base  = 0x00000000 (début de la mémoire)
     * Limit = 0xFFFFF avec G=1 (granularité page) → couvre 4 GB
     * Access = 0x9A:
     *   P=1 (présent), DPL=00 (ring 0), Type=1 (code/data),
     *   E=1 (exécutable), DC=0 (non-conforming), R=1 (readable), A=0
     * Flags = 0xC:
     *   G=1 (granularité 4KB), DB=1 (32 bits), L=0, AVL=0
     * ----------------------------------------------------------------------- */
    gdt_set_entry(GDT_KERNEL_CODE,
                  0x00000000, 0xFFFFFFFF,
                  0x9A,   /* P=1, DPL=0, S=1, E=1, DC=0, RW=1, A=0 */
                  0xCF);  /* G=1, DB=1, L=0, AVL=0, limit_high=0xF */

    /* -----------------------------------------------------------------------
     * Entrée 2: Data Segment noyau (ring 0)
     * Identique au code mais non-exécutable, écriture autorisée
     * Access = 0x92:
     *   P=1, DPL=00, S=1, E=0 (data), DC=0, W=1 (writable), A=0
     * ----------------------------------------------------------------------- */
    gdt_set_entry(GDT_KERNEL_DATA,
                  0x00000000, 0xFFFFFFFF,
                  0x92,   /* P=1, DPL=0, S=1, E=0, DC=0, RW=1, A=0 */
                  0xCF);

    /* -----------------------------------------------------------------------
     * Entrée 3: Code Segment userspace (ring 3) — pour les futures tâches
     * Access = 0xFA: DPL=11 (ring 3)
     * ----------------------------------------------------------------------- */
    gdt_set_entry(GDT_USER_CODE,
                  0x00000000, 0xFFFFFFFF,
                  0xFA,   /* P=1, DPL=3, S=1, E=1, DC=0, RW=1, A=0 */
                  0xCF);

    /* -----------------------------------------------------------------------
     * Entrée 4: Data Segment userspace (ring 3)
     * Access = 0xF2: DPL=11 (ring 3)
     * ----------------------------------------------------------------------- */
    gdt_set_entry(GDT_USER_DATA,
                  0x00000000, 0xFFFFFFFF,
                  0xF2,   /* P=1, DPL=3, S=1, E=0, DC=0, RW=1, A=0 */
                  0xCF);

    /* Charger la nouvelle GDT */
    gdt_load();
}
