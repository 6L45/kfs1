#ifndef IDT_H
#define IDT_H

/* =============================================================================
 * kernel/idt.h - Interrupt Descriptor Table (IDT)
 * =============================================================================
 *
 * CONCEPT: L'IDT (Interrupt Descriptor Table)
 *
 * En mode protégé, l'IDT est l'équivalent de la table des vecteurs d'interruption
 * du mode réel, mais avec plus de contrôle (niveaux de privilège, types, etc.)
 *
 * L'IDT contient 256 entrées (une par vecteur d'interruption 0-255).
 * Chaque entrée de 8 octets indique:
 *   - L'adresse du handler (ISR = Interrupt Service Routine)
 *   - Le sélecteur de segment de code (pour savoir quel segment CS utiliser)
 *   - Le type de gate (Interrupt Gate ou Trap Gate)
 *   - Le DPL (niveau de privilège minimal pour déclencher via INT n)
 *
 * TYPES DE GATES:
 *   Interrupt Gate (0x8E): Désactive les interruptions pendant le handler (IF=0)
 *                          Utilisé pour les IRQ matérielles et la plupart des exceptions
 *   Trap Gate (0x8F):      Laisse les interruptions actives pendant le handler
 *                          Utilisé pour les syscalls (INT 0x80) et le debug
 *
 * STRUCTURE D'UN DESCRIPTEUR IDT (8 octets):
 *   Bits 0-15:  Offset bas du handler (adresse bits 0-15)
 *   Bits 16-31: Sélecteur de segment (ex: 0x08 = code ring 0)
 *   Bits 32-39: Zéro (réservé)
 *   Bits 40-47: Type + attributs (P, DPL, type gate)
 *   Bits 48-63: Offset haut du handler (adresse bits 16-31)
 *
 * LE REGISTRE IDTR:
 * L'instruction LIDT charge le registre IDTR qui contient:
 *   - La taille de la table en octets - 1
 *   - L'adresse linéaire de la table
 * ============================================================================= */

#include "types.h"

/* --- Structure d'un descripteur IDT (8 octets, PACKED) --- */
typedef struct PACKED {
    uint16_t    offset_low;     /* Bits 0-15 de l'adresse du handler */
    uint16_t    selector;       /* Sélecteur de segment code (0x08 pour noyau) */
    uint8_t     zero;           /* Toujours 0 */

    /* Type/Attributs:
     *   Bit 7: P (Present) - 1 si l'entrée est valide
     *   Bits 6-5: DPL - Privilege level (0=ring0 → pas accessible depuis ring3)
     *   Bit 4: 0 pour les interrupt/trap gates
     *   Bits 3-0: Type: 0xE=Interrupt Gate 32bits, 0xF=Trap Gate 32bits
     *
     *   Valeur typique: 0x8E = P=1, DPL=0, Type=0xE (Interrupt Gate)
     */
    uint8_t     type_attr;

    uint16_t    offset_high;    /* Bits 16-31 de l'adresse du handler */
} idt_entry_t;

/* --- Registre IDTR --- */
typedef struct PACKED {
    uint16_t    limit;          /* sizeof(idt_table) - 1 */
    uint32_t    base;           /* Adresse de idt_table */
} idt_register_t;

/* --- Nombre d'entrées dans l'IDT --- */
#define IDT_ENTRIES     256

/* --- Types d'attributs pour les gates --- */
#define IDT_GATE_INTERRUPT  0x8E    /* P=1, DPL=0, Type=Interrupt Gate (32bits) */
#define IDT_GATE_TRAP       0x8F    /* P=1, DPL=0, Type=Trap Gate (32bits) */
#define IDT_GATE_USER       0xEE    /* P=1, DPL=3, Type=Trap Gate (pour syscalls) */

/* =============================================================================
 * Structure des registres sauvegardés sur la pile lors d'une interruption
 * Cette structure correspond EXACTEMENT à l'état de la pile après:
 *   1. Le CPU pousse EFLAGS, CS, EIP (+ error_code pour certaines exceptions)
 *   2. Notre stub ISR pousse le numéro d'INT + faux error_code
 *   3. Notre stub pousse DS via push eax
 *   4. PUSHA pousse les registres généraux
 * ============================================================================= */
typedef struct PACKED {
    /* --- Sauvegardés par PUSHA (dans l'ordre inverse de push) --- */
    uint32_t edi, esi, ebp;
    uint32_t esp;       /* Valeur ESP au moment du PUSHA (pas celle du handler) */
    uint32_t ebx, edx, ecx, eax;

    /* --- Sauvegardés par notre stub --- */
    uint32_t ds;            /* Registre de segment données */
    uint32_t int_no;        /* Numéro d'interruption (0-255) */
    uint32_t err_code;      /* Code d'erreur (ou 0 si pas de code d'erreur) */

    /* --- Sauvegardés automatiquement par le CPU lors de l'interruption --- */
    uint32_t eip;           /* Instruction pointer au moment de l'interruption */
    uint32_t cs;            /* Code Segment */
    uint32_t eflags;        /* Registre de flags */
    /* esp et ss ne sont sauvegardés que si changement de niveau de privilège */
} registers_t;

/* --- Table des noms d'exceptions (pour le débogage) --- */
extern const char *exception_messages[32];

/* --- Tableau de handlers IRQ personnalisés --- */
typedef void (*irq_handler_t)(registers_t *regs);
extern irq_handler_t irq_handlers[16];

/* --- Fonctions --- */
void idt_init(void);                                /* Initialiser l'IDT */
void idt_register_irq(uint8_t irq, irq_handler_t handler); /* Enregistrer un handler IRQ */

/* --- Déclarations des ISR (définies dans isr.asm) --- */
extern void isr0(void);  extern void isr1(void);  extern void isr2(void);
extern void isr3(void);  extern void isr4(void);  extern void isr5(void);
extern void isr6(void);  extern void isr7(void);  extern void isr8(void);
extern void isr9(void);  extern void isr10(void); extern void isr11(void);
extern void isr12(void); extern void isr13(void); extern void isr14(void);
extern void isr15(void); extern void isr16(void); extern void isr17(void);
extern void isr18(void); extern void isr19(void); extern void isr20(void);
extern void isr21(void); extern void isr22(void); extern void isr23(void);
extern void isr24(void); extern void isr25(void); extern void isr26(void);
extern void isr27(void); extern void isr28(void); extern void isr29(void);
extern void isr30(void); extern void isr31(void);

/* --- Déclarations des IRQ (définies dans isr.asm) --- */
extern void irq0(void);  extern void irq1(void);  extern void irq2(void);
extern void irq3(void);  extern void irq4(void);  extern void irq5(void);
extern void irq6(void);  extern void irq7(void);  extern void irq8(void);
extern void irq9(void);  extern void irq10(void); extern void irq11(void);
extern void irq12(void); extern void irq13(void); extern void irq14(void);
extern void irq15(void);

#endif /* IDT_H */
