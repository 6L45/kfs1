# =============================================================================
# Makefile - Système de compilation pour KFS-1
# =============================================================================
#
# CONCEPT: MAKEFILE POUR UN NOYAU
#
# La compilation d'un noyau nécessite des outils spéciaux:
#
# 1. CROSS-COMPILATEUR:
#    On cherche d'abord i686-elf-gcc (cross-compilateur pour i386 sans OS).
#    Si absent, on utilise gcc avec les flags -m32 (mode 32 bits).
#    Le cross-compilateur est PRÉFÉRABLE car il n'inclut aucune lib hôte.
#
# 2. NASM (Netwide Assembler):
#    Assemble nos fichiers .asm en format ELF32.
#    Format ELF = Executable and Linkable Format (Linux standard).
#
# 3. FLAGS IMPORTANTS:
#    -m32:              Générer du code 32 bits (architecture i386)
#    -ffreestanding:    Mode "sans OS" (pas de main(), libc non assumée)
#    -nostdlib:         Ne pas lier avec la libc
#    -nodefaultlibs:    Ne pas utiliser les libs par défaut
#    -fno-builtin:      Ne pas remplacer malloc/memset/etc par des builtins GCC
#    -fno-stack-protector: Pas de canary stack (on n'a pas encore de libc pour ça)
#    -fno-exceptions:   Pas d'exceptions C++ (N/A en C mais bonne pratique)
#    -fno-rtti:         Pas de RTTI C++ (N/A en C)
#    -O2:               Optimisations niveau 2 (bonnes performances, debug possible)
#    -Wall -Wextra:     Activer tous les warnings (code propre)
#
# 4. LD (GNU Linker):
#    On utilise le binaire ld du système, mais avec NOTRE propre linker.ld
#    (pas celui du système hôte qui est pour Linux, pas pour notre noyau).
#
# 5. STRUCTURE DE L'ISO:
#    GRUB s'attend à une structure précise:
#      iso/
#        boot/
#          kernel.bin    ← notre noyau
#          grub/
#            grub.cfg    ← configuration GRUB
#    La commande grub-mkrescue crée une ISO bootable depuis ce répertoire.
# =============================================================================

# --- Couleurs pour les messages (optionnel mais joli) ---
GREEN  := \033[0;32m
YELLOW := \033[0;33m
CYAN   := \033[0;36m
RESET  := \033[0m

# --- Détecter le cross-compilateur ---
ifneq ($(shell which i686-elf-gcc 2>/dev/null),)
    CC      := i686-elf-gcc
    LD_CC   := i686-elf-gcc
    HAS_CROSS := yes
else ifneq ($(shell which i686-linux-gnu-gcc 2>/dev/null),)
    CC      := i686-linux-gnu-gcc
    LD_CC   := i686-linux-gnu-gcc
    HAS_CROSS := yes
else
    CC      := gcc
    LD_CC   := gcc
    HAS_CROSS := no
    $(warning "No cross-compiler found, using host gcc with -m32")
endif

# NASM pour l'assembleur
AS      := nasm
# LD pour le linker (on utilise gcc comme wrapper pour simplifier)
LD      := ld
# GRUB tools pour créer l'ISO
GRUB_MKRESCUE := grub-mkrescue
# QEMU pour l'émulation
QEMU    := qemu-system-i386

# --- Nom du noyau et de l'ISO ---
KERNEL  := kernel.bin
ISO     := kfs1.iso

# --- Répertoires ---
BOOT_DIR    := boot
KERNEL_DIR  := kernel
ISO_DIR     := iso
GRUB_DIR    := grub
OBJ_DIR     := obj

# --- Flags de compilation C ---
CFLAGS  := -m32                     \
           -std=c99                  \
           -ffreestanding            \
           -fno-builtin              \
           -fno-stack-protector      \
           -nostdlib                 \
           -nodefaultlibs            \
           -Wall                     \
           -Wextra                   \
           -O2                       \
           -I$(KERNEL_DIR)

# --- Flags NASM ---
# -f elf32: format objet ELF 32 bits (pour le linker)
ASFLAGS := -f elf32

# --- Flags du linker ---
# -T: utiliser notre linker script
# -m elf_i386: format de sortie ELF 32 bits
# --no-undefined: erreur si symboles non résolus
LDFLAGS := -T linker.ld             \
           -m elf_i386              \
           --no-undefined

# =============================================================================
# Sources et objets
# =============================================================================

# Fichiers ASM → objets
ASM_SOURCES := $(BOOT_DIR)/boot.asm \
               $(BOOT_DIR)/isr.asm

# Fichiers C → objets
C_SOURCES   := $(KERNEL_DIR)/kernel.c      \
               $(KERNEL_DIR)/gdt.c         \
               $(KERNEL_DIR)/idt.c         \
               $(KERNEL_DIR)/pic.c         \
               $(KERNEL_DIR)/vga.c         \
               $(KERNEL_DIR)/tty.c         \
               $(KERNEL_DIR)/keyboard.c    \
               $(KERNEL_DIR)/printk.c      \
               $(KERNEL_DIR)/string.c

# Générer les noms d'objets dans obj/
ASM_OBJECTS := $(patsubst %.asm, $(OBJ_DIR)/%.o, $(ASM_SOURCES))
C_OBJECTS   := $(patsubst %.c,   $(OBJ_DIR)/%.o, $(C_SOURCES))
ALL_OBJECTS := $(ASM_OBJECTS) $(C_OBJECTS)

# =============================================================================
# Règles
# =============================================================================

# Règle par défaut: compiler tout et créer l'ISO
.PHONY: all
all: $(ISO)
	@echo "$(GREEN)Build successful!$(RESET)"
	@echo "$(CYAN)  Kernel: $(KERNEL)$(RESET)"
	@echo "$(CYAN)  ISO:    $(ISO)$(RESET)"
	@ls -lh $(KERNEL) $(ISO)

# --- Créer l'ISO bootable ---
$(ISO): $(KERNEL)
	@echo "$(YELLOW)[ISO]$(RESET) Creating bootable ISO..."
	@if ! command -v mformat >/dev/null 2>&1; then \
		echo "$(YELLOW)[ISO]$(RESET) Missing dependency: mformat (install package: mtools)"; \
		exit 1; \
	fi
	@if [ ! -d /usr/lib/grub/i386-pc ]; then \
		echo "$(YELLOW)[ISO]$(RESET) Missing GRUB BIOS modules: /usr/lib/grub/i386-pc"; \
		echo "$(YELLOW)[ISO]$(RESET) Install package: grub-pc-bin"; \
		exit 1; \
	fi
	@mkdir -p $(ISO_DIR)/boot/grub
	@cp $(KERNEL) $(ISO_DIR)/boot/$(KERNEL)
	@cp $(GRUB_DIR)/grub.cfg $(ISO_DIR)/boot/grub/grub.cfg
	@$(GRUB_MKRESCUE) -o $(ISO) $(ISO_DIR)
	@echo "$(GREEN)[ISO]$(RESET) $(ISO) created ($$(du -h $(ISO) | cut -f1))"

# --- Linker: lier tous les objets en kernel.bin ---
$(KERNEL): $(ALL_OBJECTS) linker.ld
	@echo "$(YELLOW)[LD]$(RESET)  Linking $@..."
	@$(LD) $(LDFLAGS) -o $@ $(ALL_OBJECTS)
	@echo "$(GREEN)[LD]$(RESET)  $@ ($$(du -h $@ | cut -f1))"

# --- Compiler les fichiers ASM ---
$(OBJ_DIR)/%.o: %.asm
	@mkdir -p $(dir $@)
	@echo "$(CYAN)[ASM]$(RESET) $<"
	@$(AS) $(ASFLAGS) -o $@ $<

# --- Compiler les fichiers C ---
$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "$(CYAN)[CC]$(RESET)  $<"
	@$(CC) $(CFLAGS) -c -o $@ $<

# =============================================================================
# Cibles utilitaires
# =============================================================================

# Lancer dans QEMU (KVM si disponible, sinon TCG software)
.PHONY: run
run: $(ISO)
	@echo "$(GREEN)[QEMU]$(RESET) Starting KFS-1 (ISO boot)..."
	@if [ -c /dev/kvm ] && [ -w /dev/kvm ]; then \
		echo "$(CYAN)[QEMU]$(RESET) KVM enabled"; \
		$(QEMU) -cdrom $(ISO) -enable-kvm -m 128M -boot order=d -net none || \
		$(QEMU) -cdrom $(ISO) -m 128M -boot order=d -net none; \
	else \
		echo "$(YELLOW)[QEMU]$(RESET) KVM unavailable, using software emulation"; \
		$(QEMU) -cdrom $(ISO) -m 128M -boot order=d -net none; \
	fi

.PHONY: run-curses
run-curses: $(ISO)
	@echo "$(GREEN)[QEMU]$(RESET) Starting KFS-1 (terminal curses display)..."
	@$(QEMU) -cdrom $(ISO) -m 128M -boot order=d -net none -display curses

# Lancer en mode kernel direct (sans ISO, plus rapide pour le débogage)
.PHONY: run-kernel
run-kernel: $(KERNEL)
	@echo "$(GREEN)[QEMU]$(RESET) Starting kernel directly (no ISO)..."
	@$(QEMU) -kernel $(KERNEL) -m 128M

# Lancer avec débogage GDB (arrêt au démarrage, port 1234)
.PHONY: debug
debug: $(KERNEL)
	@echo "$(GREEN)[DEBUG]$(RESET) Starting QEMU with GDB server on port 1234..."
	@echo "$(YELLOW)  Connect with: gdb kernel.bin$(RESET)"
	@echo "$(YELLOW)  Then in GDB:  target remote :1234$(RESET)"
	@$(QEMU) -kernel $(KERNEL) -m 128M -s -S &
	@gdb $(KERNEL) -ex "target remote :1234" -ex "break kernel_main" -ex "continue"

# Afficher la taille des sections du noyau
.PHONY: size
size: $(KERNEL)
	@echo "$(CYAN)Section sizes:$(RESET)"
	@size $(KERNEL)

# Désassembler le noyau (utile pour le debug)
.PHONY: disasm
disasm: $(KERNEL)
	@objdump -d -M intel $(KERNEL) | less

# Afficher les symboles
.PHONY: symbols
symbols: $(KERNEL)
	@nm $(KERNEL) | sort

# Vérifier le format du binaire
.PHONY: check
check: $(KERNEL)
	@echo "$(CYAN)Kernel binary info:$(RESET)"
	@file $(KERNEL)
	@echo ""
	@echo "$(CYAN)ELF sections:$(RESET)"
	@readelf -S $(KERNEL) 2>/dev/null || objdump -h $(KERNEL)
	@echo ""
	@echo "$(CYAN)Multiboot header check:$(RESET)"
	@if command -v grub-file >/dev/null 2>&1; then \
		grub-file --is-x86-multiboot $(KERNEL) && \
		echo "$(GREEN)Valid Multiboot kernel!$(RESET)" || \
		echo "ERROR: Not a valid Multiboot kernel!"; \
	else \
		echo "grub-file not available, skipping multiboot check"; \
	fi

# Nettoyer les objets compilés (garder l'ISO)
.PHONY: clean
clean:
	@echo "$(YELLOW)[CLEAN]$(RESET) Removing object files..."
	@rm -rf $(OBJ_DIR)

# Nettoyage complet (objets + binaires + ISO)
.PHONY: fclean
fclean: clean
	@echo "$(YELLOW)[FCLEAN]$(RESET) Removing all generated files..."
	@rm -f $(KERNEL) $(ISO)
	@rm -rf $(ISO_DIR)

# Recompiler depuis zéro
.PHONY: re
re: fclean all

# Afficher l'aide
.PHONY: help
help:
	@echo "$(CYAN)KFS-1 Makefile targets:$(RESET)"
	@echo "  $(GREEN)all$(RESET)         - Compile tout et créer l'ISO (défaut)"
	@echo "  $(GREEN)run$(RESET)         - Lancer dans QEMU via ISO"
	@echo "  $(GREEN)run-kernel$(RESET)  - Lancer le kernel directement dans QEMU"
	@echo "  $(GREEN)debug$(RESET)       - Lancer avec GDB"
	@echo "  $(GREEN)check$(RESET)       - Vérifier le binaire"
	@echo "  $(GREEN)size$(RESET)        - Tailles des sections"
	@echo "  $(GREEN)disasm$(RESET)      - Désassembler le noyau"
	@echo "  $(GREEN)symbols$(RESET)     - Table des symboles"
	@echo "  $(GREEN)clean$(RESET)       - Supprimer les .o"
	@echo "  $(GREEN)fclean$(RESET)      - Tout supprimer"
	@echo "  $(GREEN)re$(RESET)          - Recompiler depuis zéro"
