; =============================================================================
; boot/boot.asm - Point d'entrée du noyau (Multiboot + Bootstrap)
; =============================================================================
;
; CONCEPT: GRUB (Grand Unified Bootloader) est un chargeur d'amorçage.
; Quand l'ordinateur démarre, le BIOS cherche un secteur de boot sur le disque.
; GRUB est installé là. Il lit le disque, trouve notre noyau (un fichier ELF),
; le charge en mémoire à 0x100000 (1 MB), puis cherche dans les 8 premiers KB
; du binaire un "en-tête Multiboot" pour confirmer que c'est bien un noyau
; compatible Multiboot.
;
; MULTIBOOT SPECIFICATION:
; L'en-tête doit contenir:
;   1. Un nombre magique: 0x1BADB002
;   2. Des flags indiquant ce qu'on demande à GRUB
;   3. Un checksum tel que: magic + flags + checksum = 0 (mod 2^32)
;
; ÉTAT AU DÉMARRAGE (fourni par GRUB):
;   - CPU en mode protégé 32 bits (pas de mode réel, pas de mode 64 bits)
;   - eax = 0x2BADB002 (confirmation que GRUB nous a chargé)
;   - ebx = adresse physique de la structure multiboot_info_t (infos RAM, etc.)
;   - Interruptions DÉSACTIVÉES (IF = 0 dans EFLAGS)
;   - Pas encore de pile (stack) définie par nous
;   - A20 gate activée (accès à toute la mémoire)
;   - GDT plate (flat) mise en place par GRUB: cs=0x08, ds=0x10
; =============================================================================

; --- Constantes Multiboot ---
MBOOT_MAGIC     equ 0x1BADB002  ; Signature que GRUB recherche (= "I'M A BOOT 2")
MBOOT_ALIGN     equ 1 << 0      ; Bit 0: aligner les modules chargés sur 4KB
MBOOT_MEMINFO   equ 1 << 1      ; Bit 1: fournir la carte mémoire (mem_lower/upper)
MBOOT_FLAGS     equ (MBOOT_ALIGN | MBOOT_MEMINFO)
; Checksum: la somme des 3 champs doit valoir 0 modulo 2^32
MBOOT_CHECKSUM  equ -(MBOOT_MAGIC + MBOOT_FLAGS)

; =============================================================================
; Section .multiboot — L'en-tête doit se trouver dans les 8 premiers KB
; Le linker script place cette section EN PREMIER dans le binaire final
; =============================================================================
section .multiboot
align 4                     ; Aligné sur 4 octets (obligatoire)
    dd MBOOT_MAGIC          ; dd = "define doubleword" = 32 bits
    dd MBOOT_FLAGS
    dd MBOOT_CHECKSUM

; =============================================================================
; Section .bss — Données non initialisées (initialisées à 0 au boot)
; On y réserve la pile (stack) du noyau
; =============================================================================
section .bss
align 16                    ; SSE et ABI x86 demandent l'alignement 16 octets
stack_bottom:
    resb 16384              ; resb = "reserve bytes" — 16 KB pour la pile
stack_top:                  ; Étiquette pointant APRÈS la réserve
                            ; (la pile x86 GRANDIT VERS LE BAS, donc on pointe au sommet)

; =============================================================================
; Section .text — Code exécutable
; =============================================================================
section .text
global _start               ; Rendre _start visible pour le linker (= symbole d'entrée)
extern kernel_main          ; Déclaré dans kernel.c, défini au link time

_start:
    ; -----------------------------------------------------------------
    ; À ce point, GRUB nous a passé:
    ;   eax = 0x2BADB002 (magic multiboot de confirmation)
    ;   ebx = pointeur vers multiboot_info_t
    ; Les interruptions sont désactivées.
    ; -----------------------------------------------------------------

    ; Initialiser le Stack Pointer (esp) vers le haut de notre pile
    ; IMPORTANT: SANS pile valide, aucun appel de fonction (call) ne fonctionne !
    mov esp, stack_top

    ; Convention d'appel C (cdecl): les arguments sont pushés de DROITE à GAUCHE
    ; kernel_main(uint32_t magic, multiboot_info_t* mbi)
    push ebx            ; 2ème arg: pointeur multiboot_info (ebx intact depuis GRUB)
    push eax            ; 1er arg: magic number (eax intact depuis GRUB)

    ; Sauter dans le monde C !
    call kernel_main

    ; --- Si kernel_main() retourne (ne devrait JAMAIS arriver) ---
    ; On met le CPU en état de halte propre
.hang:
    cli                 ; CLear Interrupt flag: bloquer toutes les interruptions masquables
    hlt                 ; HALT: arrêter le CPU (en attente d'une interruption)
    jmp .hang           ; Si une NMI (Non-Maskable Interrupt) réveille le CPU, reboucler
