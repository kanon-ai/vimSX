.module crt0
.globl _main
.globl s__DATA
.globl l__DATA
.area _HEADER (ABS)
.org 0x100
    jp start
start:
    ; Check TPA before touching static storage. Leave 2 KB for stack.
    ld hl,#s__DATA
    ld de,#l__DATA+2048
    add hl,de
    ex de,hl
    ld hl,(6)
    or a
    sbc hl,de
    jr c,no_memory
    ld hl,#s__DATA
    ld bc,#l__DATA
clear:
    ld a,b
    or c
    jr z,run
    ld (hl),#0
    inc hl
    dec bc
    jr clear
run:
    call _main
    jp 0
no_memory:
    ld de,#memory_message
    ld c,#9
    call 5
    ld c,#0x6f
    call 5
    or a
    jr nz,old_dos
    ld a,b
    cp #2
    jr c,old_dos
    ld b,#2
    ld c,#0x62
    call 5
old_dos:
    jp 0
memory_message:
    .ascii "LS: insufficient TPA memory\r\n$"
.area _HOME
.area _CODE
.area _INITIALIZER
.area _GSINIT
.area _GSFINAL
.area _DATA
.area _INITIALIZED
.area _BSEG
.area _BSS
.area _HEAP
