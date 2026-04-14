#ifndef PIC_H
#define PIC_H

/* =============================================================================
 * kernel/pic.h - Programmable Interrupt Controller (PIC Intel 8259A)
 * =============================================================================
 *
 * CONCEPT: LE PIC (Programmable Interrupt Controller)
 *
 * Le PIC 8259A est un chip qui gère les IRQ (Interrupt ReQuests) matérielles.
 * Il sert d'intermédiaire entre les périphériques et le CPU:
 *   Clavier → IRQ1 → PIC → INT (numéro) → CPU → IDT → Handler
 *
 * CONFIGURATION PC STANDARD:
 * Un PC a deux PIC en cascade (maître et esclave):
 *   PIC Maître: gère IRQ 0-7  (ports 0x20/0x21)
 *   PIC Esclave: gère IRQ 8-15 (ports 0xA0/0xA1)
 *   L'esclave est connecté sur IRQ2 du maître (cascade)
 *
 * PROBLÈME DE REMAPPING:
 * Par défaut, le PIC maître mappe IRQ0-7 sur INT 0x08-0x0F.
 * OR, INT 0x08-0x1F sont réservés pour les EXCEPTIONS CPU!
 * Il y a donc collision: un Double Fault (exception 8) et le Timer (IRQ0)
 * auraient le même vecteur 0x08!
 *
 * SOLUTION: Remap the PIC!
 * On reconfigure le PIC pour que:
 *   IRQ 0-7  → INT 0x20-0x27 (vecteurs 32-39)
 *   IRQ 8-15 → INT 0x28-0x2F (vecteurs 40-47)
 *
 * PROTOCOLE D'INITIALISATION (ICW = Initialization Command Words):
 * Le PIC s'initialise avec une séquence précise de 4 octets (ICW1-4).
 * ============================================================================= */

#include "types.h"

/* --- Ports du PIC Maître --- */
#define PIC_MASTER_CMD  0x20    /* Port commande (write) / statut (read) */
#define PIC_MASTER_DATA 0x21    /* Port données (masques d'interruptions) */

/* --- Ports du PIC Esclave --- */
#define PIC_SLAVE_CMD   0xA0
#define PIC_SLAVE_DATA  0xA1

/* --- Vecteurs d'interruption après remapping --- */
#define PIC_MASTER_OFFSET 0x20  /* IRQ0-7  → INT 32-39 */
#define PIC_SLAVE_OFFSET  0x28  /* IRQ8-15 → INT 40-47 */

/* --- Commandes PIC --- */
#define PIC_CMD_EOI     0x20    /* End Of Interrupt: signaler fin de traitement IRQ */
#define PIC_CMD_INIT    0x11    /* ICW1: initialisation (bit0=ICW4 needed, bit4=init) */

/* --- ICW4 (Initialization Command Word 4) --- */
#define PIC_ICW4_8086   0x01    /* Mode 8086/88 (vs mode MCS-80/85) */

/* --- Numéros IRQ (avant remapping) --- */
#define IRQ_TIMER       0       /* Timer PIT */
#define IRQ_KEYBOARD    1       /* Clavier PS/2 */
#define IRQ_CASCADE     2       /* Cascade PIC esclave */
#define IRQ_RTC         8       /* Real Time Clock */
#define IRQ_MOUSE       12      /* Souris PS/2 */

/* --- Fonctions --- */
void    pic_init(void);                         /* Remap et initialiser les PIC */
void    pic_send_eoi(uint8_t irq);              /* Envoyer EOI après traitement */
void    pic_mask_irq(uint8_t irq);              /* Masquer (désactiver) une IRQ */
void    pic_unmask_irq(uint8_t irq);            /* Démasquer (activer) une IRQ */
uint16_t pic_get_irr(void);                     /* Interrupt Request Register */
uint16_t pic_get_isr(void);                     /* In-Service Register */

#endif /* PIC_H */
