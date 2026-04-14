/* =============================================================================
 * kernel/pic.c - Initialisation et gestion du PIC Intel 8259A
 * ============================================================================= */

#include "pic.h"

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

/* Petit délai I/O: écrire sur le port 0x80 (diagnostic) est une façon
 * classique de faire une pause entre commandes PIC (le PIC est lent!) */
static inline void io_wait(void)
{
    outb(0x80, 0);
}

/* =============================================================================
 * pic_init - Remapper et initialiser les deux PIC
 *
 * SÉQUENCE D'INITIALISATION (4 ICW pour chaque PIC):
 *
 *   ICW1 (port commande): Démarrer l'initialisation
 *   ICW2 (port données): Vecteur de base (premier vecteur INT alloué)
 *   ICW3 (port données): Configuration cascade
 *     - Maître: bitmap des IRQ connectées à un esclave (bit2=1 → IRQ2=esclave)
 *     - Esclave: son numéro d'identité (0-7)
 *   ICW4 (port données): Mode de fonctionnement (8086 vs 8085)
 * ============================================================================= */
void pic_init(void)
{
    /* Sauvegarder les masques d'interruptions actuels pour les restaurer après */
    uint8_t mask_master = inb(PIC_MASTER_DATA);
    uint8_t mask_slave  = inb(PIC_SLAVE_DATA);

    /* --- ICW1: Démarrer l'initialisation (cascade mode + ICW4 needed) --- */
    outb(PIC_MASTER_CMD, PIC_CMD_INIT);
    io_wait();
    outb(PIC_SLAVE_CMD, PIC_CMD_INIT);
    io_wait();

    /* --- ICW2: Vecteurs de base --- */
    outb(PIC_MASTER_DATA, PIC_MASTER_OFFSET);   /* IRQ0 → INT 0x20 */
    io_wait();
    outb(PIC_SLAVE_DATA, PIC_SLAVE_OFFSET);     /* IRQ8 → INT 0x28 */
    io_wait();

    /* --- ICW3: Configuration cascade --- */
    outb(PIC_MASTER_DATA, 0x04);    /* Maître: esclave connecté sur IRQ2 (bit 2 = 1) */
    io_wait();
    outb(PIC_SLAVE_DATA,  0x02);    /* Esclave: son ID est 2 (connecté sur IRQ2 du maître) */
    io_wait();

    /* --- ICW4: Mode 8086 --- */
    outb(PIC_MASTER_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC_SLAVE_DATA, PIC_ICW4_8086);
    io_wait();

    /* Restaurer les masques (par défaut: toutes les IRQ masquées sauf ce qu'on active) */
    /* Pour l'instant: masquer toutes les IRQ, on les activera au besoin */
    outb(PIC_MASTER_DATA, mask_master);
    outb(PIC_SLAVE_DATA,  mask_slave);
}

/* =============================================================================
 * pic_send_eoi - Envoyer le signal "End Of Interrupt"
 *
 * OBLIGATOIRE à la fin de chaque handler IRQ!
 * Sans EOI, le PIC considère que l'IRQ est encore "en cours" et ne peut pas
 * déclencher d'autres interruptions du même niveau (ou inférieur).
 *
 * Si l'IRQ vient du PIC esclave (IRQ 8-15), il faut envoyer EOI aux DEUX PIC:
 * d'abord à l'esclave, puis au maître (car le maître voit l'IRQ2 comme occupée).
 * ============================================================================= */
void pic_send_eoi(uint8_t irq)
{
    if (irq >= 8) {
        /* IRQ de l'esclave: envoyer EOI à l'esclave d'abord */
        outb(PIC_SLAVE_CMD, PIC_CMD_EOI);
    }
    /* Toujours envoyer EOI au maître */
    outb(PIC_MASTER_CMD, PIC_CMD_EOI);
}

/* =============================================================================
 * pic_mask_irq - Masquer (désactiver) une IRQ spécifique
 * pic_unmask_irq - Démasquer (activer) une IRQ spécifique
 *
 * Le registre OCW1 (masque) est lu/modifié sur le port DATA du PIC approprié.
 * Un bit à 1 = IRQ masquée (ignorée), un bit à 0 = IRQ active.
 * ============================================================================= */
void pic_mask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t  mask;

    if (irq < 8) {
        port = PIC_MASTER_DATA;
    } else {
        port = PIC_SLAVE_DATA;
        irq -= 8;   /* Convertir en numéro local (0-7) pour l'esclave */
    }
    mask = inb(port);
    outb(port, mask | (1 << irq));  /* Mettre le bit à 1 = masquer */
}

void pic_unmask_irq(uint8_t irq)
{
    uint16_t port;
    uint8_t  mask;

    if (irq < 8) {
        port = PIC_MASTER_DATA;
    } else {
        port = PIC_SLAVE_DATA;
        irq -= 8;
    }
    mask = inb(port);
    outb(port, mask & ~(1 << irq)); /* Mettre le bit à 0 = démasquer */
}

/* =============================================================================
 * pic_get_irr / pic_get_isr - Lire les registres internes du PIC
 *
 * IRR (Interrupt Request Register): bits des IRQ en attente (non encore traitées)
 * ISR (In-Service Register): bits des IRQ en cours de traitement
 * ============================================================================= */
static uint16_t pic_get_reg(uint8_t cmd)
{
    outb(PIC_MASTER_CMD, cmd);
    outb(PIC_SLAVE_CMD, cmd);
    return (uint16_t)(inb(PIC_MASTER_CMD) | ((uint16_t)inb(PIC_SLAVE_CMD) << 8));
}

uint16_t pic_get_irr(void) { return pic_get_reg(0x0A); }
uint16_t pic_get_isr(void) { return pic_get_reg(0x0B); }
