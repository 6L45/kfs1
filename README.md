# KFS-1 — Grub, Boot & Screen

> **Kernel From Scratch — Projet 1/10 — 42**

---

## Sommaire

1. [Objectif](#objectif)
2. [Architecture du projet](#architecture-du-projet)
3. [Concepts fondamentaux](#concepts-fondamentaux)
4. [Flux de démarrage](#flux-de-démarrage)
5. [Compilation et exécution](#compilation-et-exécution)
6. [Fichiers détaillés](#fichiers-détaillés)
7. [Bonus implémentés](#bonus-implémentés)
8. [FAQ / Défense](#faq--défense)

---

## Objectif

Créer un noyau x86 32 bits bootable via GRUB qui :
- Démarre via le protocole **Multiboot**
- Affiche **"42"** à l'écran en mode texte VGA
- Fournit une bibliothèque de base (types, string, printk)
- **(Bonus)** Couleurs, scroll, curseur, clavier, 6 terminaux virtuels

---

## Architecture du projet

```
kfs1/
├── boot/
│   ├── boot.asm        → Point d'entrée ASM (en-tête Multiboot + stack)
│   └── isr.asm         → Stubs ASM pour les 32 exceptions + 16 IRQ
├── kernel/
│   ├── types.h         → Types de base (uint8_t, bool, PACKED, etc.)
│   ├── string.h/c      → strlen, strcmp, memset, memcpy, itoa...
│   ├── gdt.h/c         → Global Descriptor Table (segmentation mémoire)
│   ├── idt.h/c         → Interrupt Descriptor Table (gestionnaire d'interruptions)
│   ├── pic.h/c         → PIC 8259A (remapping IRQ)
│   ├── vga.h/c         → Driver VGA text mode (80×25, couleurs, scroll, curseur)
│   ├── tty.h/c         → 6 terminaux virtuels (Alt+F1..F6)
│   ├── keyboard.h/c    → Driver clavier PS/2 (IRQ1, scancodes, AZERTY)
│   ├── printk.h/c      → printf-like pour le noyau (%d %s %x %p...)
│   └── kernel.c        → kernel_main() — point d'entrée C
├── grub/
│   └── grub.cfg        → Configuration GRUB (menu de boot)
├── linker.ld           → Script de liaison (adresses, sections)
├── Makefile            → Compilation, ISO, QEMU
└── README.md           → Ce fichier
```

---

## Concepts fondamentaux

### 1. BIOS → GRUB → Noyau

```
Allumage PC
    │
    ▼
BIOS / UEFI
    │  Lit le MBR (Master Boot Record) du disque
    ▼
GRUB Stage 1  (512 octets dans le MBR)
    │
    ▼
GRUB Stage 2  (chargé depuis la partition)
    │  Lit grub.cfg
    │  Trouve kernel.bin
    │  Cherche l'en-tête Multiboot dans les 8 premiers KB
    │  Charge kernel.bin à 0x100000 (1 MB)
    │  Passe en mode protégé 32 bits
    │  Remplit multiboot_info_t (RAM, modules...)
    │  Appelle _start avec: eax=0x2BADB002, ebx=&multiboot_info
    ▼
boot.asm:_start
    │  Initialise esp (stack)
    │  Appelle kernel_main(magic, mbi)
    ▼
kernel.c:kernel_main()
    │  GDT → IDT → PIC → VGA → TTY → STI → KBD
    ▼
Boucle infinie (hlt)
```

### 2. Protocole Multiboot

L'en-tête Multiboot dans `boot.asm` contient 3 mots de 32 bits :
```
┌─────────────┬─────────────┬─────────────┐
│ 0x1BADB002  │   FLAGS     │  CHECKSUM   │
│  (magic)    │ (0x3 ici)   │ -(mag+flg)  │
└─────────────┴─────────────┴─────────────┘
```
GRUB valide que `magic + flags + checksum == 0 mod 2^32`.

### 3. Mode protégé 32 bits et GDT

En mode protégé, les accès mémoire passent par la **segmentation** :
- `CS:EIP` → le code → segment code (sélecteur `0x08`)
- `DS:addr` → les données → segment data (sélecteur `0x10`)

La **GDT** (Global Descriptor Table) décrit ces segments. On configure un modèle **flat** : tous les segments couvrent toute la RAM (base=0, limite=4 GB), rendant la segmentation transparente.

### 4. IDT et Interruptions

L'**IDT** (Interrupt Descriptor Table) associe chaque vecteur (0-255) à un handler :
- Vecteurs 0-31 : exceptions CPU (division/0, page fault, GPF...)
- Vecteurs 32-47 : IRQ matérielles après remapping PIC
- Vecteur 0x80 : syscalls (futur)

Lors d'une interruption, le CPU :
1. Sauvegarde `EFLAGS`, `CS`, `EIP` sur la pile
2. Consulte l'IDT → saute au stub ASM correspondant
3. Le stub ASM sauvegarde les registres, appelle le handler C
4. `IRET` restaure le contexte

### 5. PIC et remapping des IRQ

Le PIC 8259A gère les interruptions matérielles (IRQ). Par défaut, il mappe IRQ0-7 sur INT 8-15, qui **collisionnent** avec les exceptions CPU ! On le **remappage** :
```
Avant : IRQ0=INT8, IRQ1=INT9, ... IRQ7=INT15
Après : IRQ0=INT32, IRQ1=INT33, ... IRQ15=INT47
```

### 6. VGA Text Mode (0xB8000)

La mémoire vidéo est à `0xB8000`. Chaque cellule = 2 octets :
```
┌───────────────────┬───────────────────┐
│   Octet 0: ASCII  │ Octet 1: Attribut │
└───────────────────┴───────────────────┘

Attribut:  [7: Blink] [6-4: BG Color] [3-0: FG Color]
```
16 couleurs disponibles. Le curseur matériel se contrôle via les ports I/O `0x3D4`/`0x3D5`.

### 7. Clavier PS/2 (IRQ1)

À chaque frappe :
1. IRQ1 → INT 33 → `irq_common_stub` (isr.asm)
2. `irq_handler()` (idt.c) → `keyboard_irq_handler()` (keyboard.c)
3. `inb(0x60)` lit le scancode
4. Conversion scancode → ASCII via la table AZERTY
5. Caractère ajouté au buffer circulaire + echo sur le TTY actif

---

## Flux de démarrage

```
kernel_main()
    │
    ├─ gdt_init()      Flat GDT : NULL / kernel code / kernel data / user code / user data
    │
    ├─ idt_init()      256 entrées IDT : isr0..31 + irq0..15
    │
    ├─ pic_init()      Remapper : IRQ0-7→INT32-39, IRQ8-15→INT40-47
    │   └─ Masquer toutes les IRQ (activation une par une)
    │
    ├─ vga_init()      Effacer l'écran, curseur en (0,0)
    │
    ├─ tty_init()      6 buffers TTY, TTY0 actif
    │
    ├─ sti             Activer les interruptions (CLI→STI)
    │
    ├─ keyboard_init() Enregistrer IRQ1, démasquer IRQ1
    │
    ├─ Affichage : bannière, RAM info, "42" en ASCII art, prompt
    │
    └─ while(1) hlt    Attendre les interruptions (clavier, timer...)
```

---

## Compilation et exécution

### Prérequis

```bash
# Debian/Ubuntu
sudo apt-get install build-essential nasm qemu-system-x86 grub-pc-bin xorriso

# Cross-compilateur (recommandé)
# Voir: https://wiki.osdev.org/GCC_Cross-Compiler
sudo apt-get install gcc-i686-linux-gnu
```

### Compilation

```bash
cd kfs1/

# Compiler et créer l'ISO
make

# Ou juste le kernel.bin
make kernel.bin
```

### Exécution

```bash
# Lancer dans QEMU via ISO (recommandé)
make run

# Lancer le kernel directement (sans GRUB, plus rapide)
make run-kernel

# Débogage avec GDB
make debug
```

### Vérification

```bash
# Vérifier que c'est un noyau Multiboot valide
make check

# Tailles des sections
make size

# Désassembler
make disasm
```

---

## Fichiers détaillés

| Fichier | Rôle | Concepts clés |
|---------|------|---------------|
| `boot/boot.asm` | Point d'entrée, en-tête Multiboot, init stack | Multiboot spec, segments x86 |
| `boot/isr.asm` | Stubs ASM pour 48 interruptions | `pusha`, `iret`, pile d'interruption |
| `kernel/types.h` | Types à taille fixe | Portabilité, `PACKED`, `NORETURN` |
| `kernel/string.c` | strlen, memset, memcpy, itoa... | Implémentation sans libc |
| `kernel/gdt.c` | Configure la segmentation mémoire | Descripteurs GDT, `lgdt`, far jump |
| `kernel/idt.c` | Table des interruptions + handlers | Descripteurs IDT, `lidt`, `isr/irq_handler` |
| `kernel/pic.c` | Remapping PIC + EOI | Ports I/O, ICW1-4, masques IRQ |
| `kernel/vga.c` | Driver affichage 80×25 couleurs | Framebuffer 0xB8000, ports CRT |
| `kernel/tty.c` | 6 terminaux virtuels | Buffer swap, switch Alt+Fn |
| `kernel/keyboard.c` | Driver clavier PS/2 AZERTY | Scancodes Set1, buffer circulaire |
| `kernel/printk.c` | printf-like sans libc | `va_list`, parsing de format |
| `linker.ld` | Organisation mémoire du binaire | Sections ELF, adresse 1MB |

---

## Bonus implémentés

| Bonus | Implémentation |
|-------|---------------|
| **Couleurs** | `vga_set_color(fg, bg)` — 16 couleurs foreground + 8 background |
| **Scroll** | `vga_scroll()` — `memmove()` du framebuffer + effacement dernière ligne |
| **Curseur** | `vga_update_hw_cursor()` — ports CRT `0x3D4/0x3D5` |
| **printf/printk** | `printk("%d %s %x %p...")` — implémentation complète avec `va_list` |
| **Clavier** | IRQ1 + table scancodes AZERTY + buffer circulaire + echo |
| **6 terminaux** | `tty_switch(n)` — Alt+F1..F6 — buffer indépendant par TTY |

---

## FAQ / Défense

**Q: Pourquoi `-nostdlib` et `-ffreestanding` ?**
> Sans ces flags, GCC inclurait automatiquement la libc (printf, malloc...) du système hôte. Dans un noyau, il n'y a pas de libc — on tourne "bare metal". Ces flags indiquent à GCC qu'on n't pas de OS sous-jacent.

**Q: Pourquoi le kernel est chargé à 0x100000 (1MB) ?**
> La mémoire sous 1MB est occupée par le BIOS, les zones VGA (0xA0000-0xBFFFF), et les tables BIOS. À partir de 1MB commence l'"extended memory" libre pour le noyau.

**Q: Pourquoi remap le PIC ?**
> Par défaut, IRQ0-7 → INT 8-15, mais ces vecteurs sont réservés aux exceptions CPU (Double Fault=8, Invalid TSS=10, Page Fault=14...). Sans remapping, une IRQ clavier déclencherait le handler "Segment Not Present". On remappage à INT 32+ pour éviter ce conflit.

**Q: Pourquoi `cli` avant d'initialiser l'IDT ?**
> GRUB nous passe avec les interruptions désactivées (IF=0). On garde ça jusqu'à ce que GDT+IDT+PIC soient prêts, puis `sti`. Si une interruption survenait avec une IDT vide → Triple Fault → reboot immédiat.

**Q: Différence entre Interrupt Gate et Trap Gate ?**
> L'Interrupt Gate met `IF=0` (désactive les interruptions) pendant l'exécution du handler — utile pour les IRQ matérielles (pas d'interruption dans une interruption). La Trap Gate laisse `IF=1` — utile pour les syscalls et le debug.

**Q: Comment fonctionne la pile lors d'une interruption ?**
> Le CPU pousse automatiquement `EFLAGS`, `CS`, `EIP` (et `SS:ESP` si changement de ring). Notre stub ASM ajoute le numéro d'INT + code d'erreur, puis `pusha` sauvegarde les 8 registres généraux. Le tout forme la structure `registers_t` qu'on passe au handler C.

**Q: Pourquoi `hlt` dans la boucle principale ?**
> `hlt` suspend le CPU jusqu'à la prochaine interruption. Sans ça, le CPU tourne en boucle vide à 100% consommant inutilement de l'énergie (busy-wait). Avec `hlt`, le CPU "dort" entre les frappes clavier.

**Q: Comment fonctionne le scroll ?**
> `memmove()` copie les lignes 1..24 vers 0..23 (80 cellules × 24 lignes × 2 octets = 3840 octets déplacés). Puis on efface la ligne 24 avec des espaces. `memmove` (pas `memcpy`) car source et destination se chevauchent.

**Q: Format de l'attribut VGA ?**
> 1 octet : `[Blink:1][BG:3][FG:4]`. Bit 7=clignotement (souvent désactivé), bits 6-4=couleur fond (3 bits = 8 couleurs), bits 3-0=couleur texte (4 bits = 16 couleurs). Ex: `0x0F` = noir/blanc, `0x1E` = bleu/jaune.
