#ifndef GDT_H
#define GDT_H

/* =============================================================================
 * kernel/gdt.h - Global Descriptor Table (GDT)
 * =============================================================================
 *
 * CONCEPT: SEGMENTATION x86 (mode protégé 32 bits)
 *
 * En mode protégé, toutes les adresses mémoire passent par un mécanisme de
 * SEGMENTATION avant d'arriver au matériel. Le CPU ne travaille plus directement
 * avec des adresses physiques mais avec des adresses VIRTUELLES (segment:offset).
 *
 * LA GDT (Global Descriptor Table):
 * C'est un tableau en mémoire qui décrit les SEGMENTS de mémoire disponibles.
 * Chaque entrée (descriptor) de 8 octets décrit un segment:
 *   - Son adresse de base (base address)
 *   - Sa taille (limit)
 *   - Ses permissions (lecture, écriture, exécution)
 *   - Son niveau de privilège (ring 0 = noyau, ring 3 = userspace)
 *
 * SÉLECTEURS DE SEGMENTS:
 * Les registres CS (code), DS (data), SS (stack), ES, FS, GS contiennent
 * des "sélecteurs" qui sont des INDEX dans la GDT × 8 (taille d'une entrée).
 *   - 0x00 = NULL descriptor (obligatoire, jamais utilisé)
 *   - 0x08 = Segment code ring 0 (noyau)
 *   - 0x10 = Segment data ring 0 (noyau)
 *   - 0x18 = Segment code ring 3 (userspace, pour plus tard)
 *   - 0x20 = Segment data ring 3 (userspace, pour plus tard)
 *
 * FLAT MODEL:
 * On configure une GDT "plate" où tous les segments couvrent toute la mémoire
 * (base=0, limit=4GB). La segmentation est alors transparente et on travaille
 * en pratique avec des adresses linéaires = adresses physiques (sans pagination).
 *
 * POURQUOI RECONFIGURER LA GDT?
 * GRUB en installe une temporaire. Il est recommandé d'en créer une propre
 * pour garantir les paramètres exacts qu'on attend (notamment pour les IDT handlers).
 * ============================================================================= */

#include "types.h"

/* --- Structure d'un descripteur GDT (8 octets) --- */
/* Attribut PACKED: pas de padding, les champs sont exactement consécutifs */
typedef struct PACKED {
    uint16_t    limit_low;      /* Bits 0-15 de la limite du segment */
    uint16_t    base_low;       /* Bits 0-15 de l'adresse de base */
    uint8_t     base_mid;       /* Bits 16-23 de l'adresse de base */

    /* Octet d'accès (Access byte):
     *   Bit 7: Present (P) - Le segment est présent en mémoire (1 = oui)
     *   Bits 6-5: DPL - Descriptor Privilege Level (0=ring0, 3=ring3)
     *   Bit 4: Descriptor Type (1=code/data, 0=système)
     *   Bit 3: Executable (1=code, 0=data)
     *   Bit 2: Direction/Conforming
     *   Bit 1: Readable/Writable
     *   Bit 0: Accessed (mis à 1 par le CPU quand le segment est accédé)
     */
    uint8_t     access;

    /* Nibble haut: flags + bits 16-19 de la limite
     *   Bit 7 (G): Granularity (0=limite en octets, 1=limite en pages 4KB)
     *   Bit 6 (DB): Default operation size (0=16bits, 1=32bits)
     *   Bit 5 (L): Long mode (1=64bits, doit être 0 en 32bits)
     *   Bit 4: Disponible (inutilisé)
     *   Bits 3-0: Bits 16-19 de la limite
     */
    uint8_t     flags_limit_high;

    uint8_t     base_high;      /* Bits 24-31 de l'adresse de base */
} gdt_entry_t;

/* --- Structure du registre GDTR (passé à l'instruction LGDT) --- */
typedef struct PACKED {
    uint16_t    limit;          /* Taille de la GDT en octets - 1 */
    uint32_t    base;           /* Adresse linéaire de la GDT */
} gdt_register_t;

/* --- Indices dans notre GDT --- */
#define GDT_NULL        0   /* Entrée nulle obligatoire */
#define GDT_KERNEL_CODE 1   /* Segment code noyau (ring 0) */
#define GDT_KERNEL_DATA 2   /* Segment data noyau (ring 0) */
#define GDT_USER_CODE   3   /* Segment code userspace (ring 3) - futur */
#define GDT_USER_DATA   4   /* Segment data userspace (ring 3) - futur */
#define GDT_ENTRIES     5   /* Nombre total d'entrées */

/* --- Sélecteurs (index × 8) --- */
#define SEG_KERNEL_CODE     0x08    /* GDT_KERNEL_CODE * 8 */
#define SEG_KERNEL_DATA     0x10    /* GDT_KERNEL_DATA * 8 */
#define SEG_USER_CODE       0x1B    /* GDT_USER_CODE * 8 + RPL(3) */
#define SEG_USER_DATA       0x23    /* GDT_USER_DATA * 8 + RPL(3) */

/* --- Initialiser et charger la GDT --- */
void gdt_init(void);

#endif /* GDT_H */
