; =============================================================================
; boot/isr.asm - Stubs assembleur pour les Interrupt Service Routines
; =============================================================================
;
; CONCEPT: INTERRUPTIONS ET EXCEPTIONS
;
; Le CPU x86 peut être interrompu à tout moment par:
;   1. EXCEPTIONS (INT 0-31): erreurs internes du CPU
;      Ex: division par 0 (INT 0), page fault (INT 14), GPF (INT 13)
;   2. IRQ matérielles (INT 32-47 après remapping): signaux des périphériques
;      Ex: clavier (IRQ1=INT33), timer (IRQ0=INT32)
;   3. Interruptions logicielles (INT 0x80 pour les syscalls Linux)
;
; MÉCANIQUE D'UNE INTERRUPTION:
; Quand une interruption survient, le CPU:
;   1. Sauvegarde EFLAGS, CS, EIP sur la pile (+ code d'erreur pour certaines)
;   2. Consulte l'IDT (Interrupt Descriptor Table) à l'entrée correspondante
;   3. Saute au handler enregistré dans l'IDT
;   4. À la fin du handler, IRET restaure EIP, CS, EFLAGS
;
; CODE D'ERREUR: certaines exceptions poussent automatiquement un code d'erreur
; sur la pile (ISR 8, 10-14, 17, 30). Pour les autres, on pousse 0 manuellement
; afin que notre structure "registers_t" en C ait toujours le même format.
;
; STRUCTURE DE LA PILE à l'entrée du handler commun (après pusha + push ds):
;   [esp+0]  = ds (segment de données sauvegardé)
;   [esp+4]  = edi, esi, ebp, esp_orig, ebx, edx, ecx, eax (pusha)
;   [esp+36] = numéro d'interruption
;   [esp+40] = code d'erreur (ou 0)
;   [esp+44] = eip (sauvegardé par le CPU)
;   [esp+48] = cs
;   [esp+52] = eflags
; =============================================================================

; --- Macro: ISR sans code d'erreur automatique ---
; Le CPU ne pousse pas de code d'erreur pour ces interruptions,
; donc on en pousse un faux (0) pour uniformiser la structure registers_t
%macro ISR_NO_ERR 1
global isr%1
isr%1:
    cli                     ; Désactiver les interruptions masquables
    push dword 0            ; Faux code d'erreur
    push dword %1           ; Numéro d'interruption
    jmp isr_common_stub     ; Aller au handler commun
%endmacro

; --- Macro: ISR avec code d'erreur automatique ---
; Le CPU a DÉJÀ poussé le code d'erreur avant de sauter ici
%macro ISR_ERR 1
global isr%1
isr%1:
    cli
    ; Pas de push 0 ici: le code d'erreur est déjà sur la pile
    push dword %1           ; Numéro d'interruption
    jmp isr_common_stub
%endmacro

; --- Macro: IRQ (Interrupt ReQuest) matérielle ---
; %1 = numéro IRQ (0-15), %2 = numéro d'interruption remappé (32-47)
%macro IRQ 2
global irq%1
irq%1:
    cli
    push dword 0            ; Pas de code d'erreur pour les IRQ
    push dword %2           ; Numéro d'interruption (32 + numéro IRQ)
    jmp irq_common_stub
%endmacro

; =============================================================================
; Définition des 32 exceptions CPU (Intel Manual Vol.3, Table 6-1)
; =============================================================================
ISR_NO_ERR 0    ; #DE: Division Error (div/idiv par 0)
ISR_NO_ERR 1    ; #DB: Debug Exception
ISR_NO_ERR 2    ;      NMI (Non-Maskable Interrupt)
ISR_NO_ERR 3    ; #BP: Breakpoint (INT3)
ISR_NO_ERR 4    ; #OF: Overflow (INTO)
ISR_NO_ERR 5    ; #BR: Bound Range Exceeded (BOUND)
ISR_NO_ERR 6    ; #UD: Invalid/Undefined Opcode
ISR_NO_ERR 7    ; #NM: Device Not Available (FPU absent)
ISR_ERR    8    ; #DF: Double Fault (a un code d'erreur = 0)
ISR_NO_ERR 9    ; Coprocessor Segment Overrun (obsolète, jamais déclenché)
ISR_ERR    10   ; #TS: Invalid TSS
ISR_ERR    11   ; #NP: Segment Not Present
ISR_ERR    12   ; #SS: Stack-Segment Fault
ISR_ERR    13   ; #GP: General Protection Fault (accès mémoire illégal, etc.)
ISR_ERR    14   ; #PF: Page Fault
ISR_NO_ERR 15   ;      Réservé Intel
ISR_NO_ERR 16   ; #MF: x87 Floating-Point Exception
ISR_ERR    17   ; #AC: Alignment Check
ISR_NO_ERR 18   ; #MC: Machine Check
ISR_NO_ERR 19   ; #XF: SIMD Floating-Point Exception
ISR_NO_ERR 20   ; #VE: Virtualization Exception
ISR_NO_ERR 21   ; #CP: Control Protection Exception
ISR_NO_ERR 22   ; Réservé
ISR_NO_ERR 23   ; Réservé
ISR_NO_ERR 24   ; Réservé
ISR_NO_ERR 25   ; Réservé
ISR_NO_ERR 26   ; Réservé
ISR_NO_ERR 27   ; Réservé
ISR_NO_ERR 28   ; Réservé
ISR_NO_ERR 29   ; Réservé
ISR_ERR    30   ; #SX: Security Exception
ISR_NO_ERR 31   ; Réservé

; =============================================================================
; Définition des 16 IRQ matérielles (après remapping PIC: IRQn -> INT(n+32))
; =============================================================================
IRQ  0, 32   ; IRQ0:  Timer (PIT - Programmable Interval Timer)
IRQ  1, 33   ; IRQ1:  Clavier PS/2 (port 0x60)
IRQ  2, 34   ; IRQ2:  Cascade PIC esclave (non utilisé directement)
IRQ  3, 35   ; IRQ3:  Port série COM2
IRQ  4, 36   ; IRQ4:  Port série COM1
IRQ  5, 37   ; IRQ5:  LPT2 ou carte son
IRQ  6, 38   ; IRQ6:  Lecteur de disquette (FDC)
IRQ  7, 39   ; IRQ7:  LPT1 / Spurious interrupt PIC maître
IRQ  8, 40   ; IRQ8:  RTC (Real Time Clock)
IRQ  9, 41   ; IRQ9:  Libre / ACPI
IRQ 10, 42   ; IRQ10: Libre
IRQ 11, 43   ; IRQ11: Libre
IRQ 12, 44   ; IRQ12: Souris PS/2 (port 0x60)
IRQ 13, 45   ; IRQ13: Coprocesseur FPU (obsolète)
IRQ 14, 46   ; IRQ14: Contrôleur ATA primaire (disque dur/CD-ROM)
IRQ 15, 47   ; IRQ15: Contrôleur ATA secondaire / Spurious PIC esclave

; =============================================================================
; Handler commun pour les EXCEPTIONS (ISR)
; =============================================================================
extern isr_handler          ; Fonction C dans idt.c

isr_common_stub:
    pusha               ; Push all: eax, ecx, edx, ebx, esp_orig, ebp, esi, edi
                        ; (8 registres × 4 octets = 32 octets sur la pile)

    ; Sauvegarder le registre de segment DS
    mov ax, ds          ; Lire le sélecteur de segment données actuel
    push eax            ; Le sauvegarder sur la pile

    ; Charger les segments du noyau (sélecteur 0x10 = 3ème entrée GDT = data ring0)
    ; Nécessaire car on peut arriver ici depuis du code userspace (futur)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    ; Appeler le handler C en passant un pointeur vers la structure sur la pile
    push esp            ; Argument: pointeur vers la structure registers_t
    call isr_handler
    add esp, 4          ; Nettoyer l'argument pushé (cdecl: l'appelant nettoie)

    ; Restaurer DS
    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa                ; Restaurer eax, ecx, edx, ebx, esp, ebp, esi, edi
    add esp, 8          ; Nettoyer: numéro d'interruption (4) + code d'erreur (4)
    sti                 ; Réactiver les interruptions
    iret                ; Return from Interrupt: restaure EIP, CS, EFLAGS (et SS:ESP si DPL change)

; =============================================================================
; Handler commun pour les IRQ
; =============================================================================
extern irq_handler          ; Fonction C dans idt.c

irq_common_stub:
    pusha

    mov ax, ds
    push eax

    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    push esp
    call irq_handler
    add esp, 4

    pop eax
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    popa
    add esp, 8
    sti
    iret
