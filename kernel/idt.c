/* =============================================================================
 * kernel/idt.c - Initialisation de l'IDT et handlers d'interruptions
 * ============================================================================= */

#include "idt.h"
#include "pic.h"
#include "vga.h"
#include "string.h"

/* --- Table IDT statique en mémoire --- */
static idt_entry_t    idt_table[IDT_ENTRIES];
static idt_register_t idt_reg;

/* --- Table des handlers IRQ personnalisés (NULL = pas de handler) --- */
irq_handler_t irq_handlers[16];

/* --- Messages lisibles pour chaque exception CPU --- */
const char *exception_messages[32] = {
    "Division By Zero",             /* 0  #DE */
    "Debug",                        /* 1  #DB */
    "Non Maskable Interrupt",       /* 2       */
    "Breakpoint",                   /* 3  #BP */
    "Overflow",                     /* 4  #OF */
    "Bound Range Exceeded",         /* 5  #BR */
    "Invalid Opcode",               /* 6  #UD */
    "Device Not Available",         /* 7  #NM */
    "Double Fault",                 /* 8  #DF */
    "Coprocessor Segment Overrun",  /* 9       */
    "Invalid TSS",                  /* 10 #TS */
    "Segment Not Present",          /* 11 #NP */
    "Stack-Segment Fault",          /* 12 #SS */
    "General Protection Fault",     /* 13 #GP */
    "Page Fault",                   /* 14 #PF */
    "Reserved",                     /* 15      */
    "x87 Floating-Point Exception", /* 16 #MF */
    "Alignment Check",              /* 17 #AC */
    "Machine Check",                /* 18 #MC */
    "SIMD Floating-Point Exception",/* 19 #XF */
    "Virtualization Exception",     /* 20 #VE */
    "Control Protection Exception", /* 21 #CP */
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved",
    "Reserved",                     /* 29      */
    "Security Exception",           /* 30 #SX */
    "Reserved"                      /* 31      */
};

/* =============================================================================
 * idt_set_entry - Remplir une entrée IDT
 *
 * @vector:   Numéro d'interruption (0-255)
 * @handler:  Adresse de la fonction handler (ISR/IRQ stub en ASM)
 * @selector: Sélecteur de segment code (0x08 = kernel code)
 * @type_attr: Type + attributs (ex: IDT_GATE_INTERRUPT = 0x8E)
 * ============================================================================= */
static void idt_set_entry(uint8_t vector, void (*handler)(void),
                          uint16_t selector, uint8_t type_attr)
{
    uint32_t offset = (uint32_t)handler;
    idt_table[vector].offset_low  = (uint16_t)(offset & 0xFFFF);
    idt_table[vector].selector    = selector;
    idt_table[vector].zero        = 0;
    idt_table[vector].type_attr   = type_attr;
    idt_table[vector].offset_high = (uint16_t)((offset >> 16) & 0xFFFF);
}

/* =============================================================================
 * idt_init - Initialiser l'IDT avec tous les handlers
 * ============================================================================= */
void idt_init(void)
{
    /* Effacer la table et les handlers IRQ */
    memset(idt_table, 0, sizeof(idt_table));
    memset(irq_handlers, 0, sizeof(irq_handlers));

    /* --- Enregistrer les 32 exceptions CPU --- */
    idt_set_entry(0,  isr0,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(1,  isr1,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(2,  isr2,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(3,  isr3,  0x08, IDT_GATE_TRAP);      /* Breakpoint: Trap Gate */
    idt_set_entry(4,  isr4,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(5,  isr5,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(6,  isr6,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(7,  isr7,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(8,  isr8,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(9,  isr9,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(10, isr10, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(11, isr11, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(12, isr12, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(13, isr13, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(14, isr14, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(15, isr15, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(16, isr16, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(17, isr17, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(18, isr18, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(19, isr19, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(20, isr20, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(21, isr21, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(22, isr22, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(23, isr23, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(24, isr24, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(25, isr25, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(26, isr26, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(27, isr27, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(28, isr28, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(29, isr29, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(30, isr30, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(31, isr31, 0x08, IDT_GATE_INTERRUPT);

    /* --- Enregistrer les 16 IRQ matérielles (vecteurs 32-47 après remapping) --- */
    idt_set_entry(32, irq0,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(33, irq1,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(34, irq2,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(35, irq3,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(36, irq4,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(37, irq5,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(38, irq6,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(39, irq7,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(40, irq8,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(41, irq9,  0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(42, irq10, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(43, irq11, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(44, irq12, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(45, irq13, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(46, irq14, 0x08, IDT_GATE_INTERRUPT);
    idt_set_entry(47, irq15, 0x08, IDT_GATE_INTERRUPT);

    /* --- Charger l'IDT via LIDT --- */
    idt_reg.limit = (uint16_t)(sizeof(idt_table) - 1);
    idt_reg.base  = (uint32_t)&idt_table;
    __asm__ volatile ("lidt (%0)" : : "r"(&idt_reg) : "memory");
}

/* =============================================================================
 * idt_register_irq - Enregistrer un handler C pour une IRQ spécifique
 * ============================================================================= */
void idt_register_irq(uint8_t irq, irq_handler_t handler)
{
    if (irq < 16)
        irq_handlers[irq] = handler;
}

/* =============================================================================
 * isr_handler - Handler C appelé par tous les stubs d'exceptions
 *
 * Appelé depuis isr_common_stub dans isr.asm avec la structure registers_t.
 * Pour l'instant: affiche un message de kernel panic si exception critique.
 * ============================================================================= */
void isr_handler(registers_t *regs)
{
    if (regs->int_no < 32) {
        /* Exception CPU: afficher un message d'erreur */
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_RED);
        vga_puts("\n[KERNEL PANIC] Exception: ");
        vga_puts(exception_messages[regs->int_no]);
        vga_puts("\n  INT=");

        /* Afficher le numéro d'interruption en hexadécimal */
        char buf[16];
        utoa(regs->int_no, buf, 16);
        vga_puts("0x"); vga_puts(buf);

        vga_puts("  ERR=0x");
        utoa(regs->err_code, buf, 16);
        vga_puts(buf);

        vga_puts("  EIP=0x");
        utoa(regs->eip, buf, 16);
        vga_puts(buf);

        vga_puts("\n  System halted.\n");

        /* Halte définitive */
        __asm__ volatile ("cli; hlt");
        while (1) {}
    }
}

/* =============================================================================
 * irq_handler - Handler C appelé par tous les stubs d'IRQ
 *
 * Appelé depuis irq_common_stub dans isr.asm.
 * Distribue vers le handler spécifique enregistré via idt_register_irq().
 * Envoie toujours EOI au PIC en fin de traitement.
 * ============================================================================= */
void irq_handler(registers_t *regs)
{
    /* Numéro d'IRQ = numéro d'interruption - offset de remapping */
    uint8_t irq = (uint8_t)(regs->int_no - PIC_MASTER_OFFSET);

    /* Appeler le handler personnalisé si enregistré */
    if (irq < 16 && irq_handlers[irq] != NULL)
        irq_handlers[irq](regs);

    /* OBLIGATOIRE: signaler la fin de l'interruption au PIC */
    pic_send_eoi(irq);
}
