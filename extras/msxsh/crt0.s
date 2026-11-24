.module crt0
.globl _main
.globl s__DATA
.globl l__DATA
.area _HEADER (ABS)
.org 0x100
 jp start
 .ascii "MSXSH001"
entry_sp: .dw 0
start:
 ld (entry_sp),sp
 ld hl,(6)
 ld de,#0xd000
 or a
 sbc hl,de
 jr c,small
 ld sp,#0xcff0
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
 ld sp,(entry_sp)
 ret
small:
 ld de,#message
 ld c,#9
 call 5
 ld sp,(entry_sp)
 ret
message:
 .ascii "MSXSH: TPA must extend to D000h\r\n$"
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
