; MSXSH supervisor. Copies are private DOS2 user mapper segments.
; Never changes mapper page 3. Stack and all transfer code stay in page 3.
.module resident
.area RESIDENT (ABS)
.org 0xc000
 jp warm
 jp warm
dosgate:
 jp dispatch          ; original DOS entry is saved at C612
 jp launch            ; C009
 jp init              ; C00C
 jp release           ; C00F

; Mapper jump veneers patched by init. Each is a JP in the DOS jump table.
alloc: jp 0
free: jp 0
put1: jp 0
get1: jp 0
put2: jp 0
get2: jp 0
readseg: jp 0
writeseg: jp 0

realdos:
 push hl
 ld hl,(0xc612)
 ex (sp),hl
 ret

; Standard-handle memory streams. No disk files are used for pipes.
dispatch:
 push af
 ld a,(0xc608)
 or a
 jr z,forward
 ld a,c
 cp #1
 jr c,handles
 cp #12
 jr c,unsupported
handles:
 ld a,b
 or a
 jr z,input_dispatch
 cp #1
 jr nz,forward
 ld a,(0xc608)
 and #2
 jr z,forward
 ld a,c
 cp #0x49
 jr z,write_dispatch
 cp #0x4b
 jr z,ioctl_dispatch
 cp #0x47
 jr z,unsupported
 cp #0x45
 jr z,unsupported
 jr forward
input_dispatch:
 ld a,(0xc608)
 and #1
 jr z,forward
 ld a,c
 cp #0x48
 jp z,read_dispatch
 cp #0x4b
 jr z,ioctl_dispatch
 cp #0x47
 jr z,unsupported
 cp #0x45
 jr z,unsupported
 cp #0x4a
 jr z,unsupported
forward:
 pop af
 jp realdos
unsupported:
 pop af
 ld a,#1
 ld (0xc625),a
 ld a,#0xc5
 ret
ioctl_dispatch:
 pop af
 or a
 jr nz,unsupported_ioctl
 ld de,#0
 xor a
 ret
unsupported_ioctl:
 ld de,#0
 xor a
 ret
write_dispatch:
 pop af
 ld (0xc614),sp
 ld sp,#0xcbf0
 push bc
 push de
 ld (request),hl
 ld (pointer),de
 ld a,(0xc60a)
 call bank_length
 ld (lengthptr),hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (offset),de
 ld hl,(request)
 add hl,de
 jr c,pipe_full
 ld de,#4097
 or a
 sbc hl,de
 jr nc,pipe_full
write_loop:
 ld hl,(request)
 ld a,h
 or l
 jr z,write_done
 ld hl,(pointer)
 ld e,(hl)
 inc hl
 ld (pointer),hl
 ld hl,(offset)
 ld a,(0xc60a)
 or a
 jr z,write_bank
 set 4,h
write_bank:
 ld a,(0xc60b)
 call writeseg
 ld hl,(offset)
 inc hl
 ld (offset),hl
 ld hl,(request)
 dec hl
 ld (request),hl
 jr write_loop
write_done:
 ld hl,(lengthptr)
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld bc,(offset)
 ld (hl),b
 dec hl
 ld (hl),c
 ld h,b
 ld l,c
 or a
 sbc hl,de
 xor a
 jp pipe_return
pipe_full:
 ld a,#1
 ld (0xc624),a
 ld hl,#0
 ld a,#0xd0
 jp pipe_return
read_dispatch:
 pop af
 ld a,h
 or l
 ret z
 ld (0xc614),sp
 ld sp,#0xcbf0
 push bc
 push de
 ld (request),hl
 ld (pointer),de
 ld hl,#0
 ld (copied),hl
 ld a,(0xc609)
 call bank_length
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (available),de
read_loop:
 ld hl,(request)
 ld a,h
 or l
 jr z,read_done
 ld hl,(0xc610)
 ld de,(available)
 or a
 sbc hl,de
 jr nc,read_done
 ld hl,(0xc610)
 ld a,(0xc609)
 or a
 jr z,read_bank
 set 4,h
read_bank:
 ld a,(0xc60b)
 call readseg
 ld hl,(pointer)
 ld (hl),a
 inc hl
 ld (pointer),hl
 ld hl,(0xc610)
 inc hl
 ld (0xc610),hl
 ld hl,(copied)
 inc hl
 ld (copied),hl
 ld hl,(request)
 dec hl
 ld (request),hl
 jr read_loop
read_done:
 ld hl,(copied)
 ld a,h
 or l
 jr z,pipe_eof
 xor a
 jp pipe_return
pipe_eof:
 ld a,#0xc7
pipe_return:
 pop de
 pop bc
 ld sp,(0xc614)
 ei
 ret
bank_length:
 ld hl,#0xc60c
 or a
 ret z
 inc hl
 inc hl
 ret
request: .dw 0
pointer: .dw 0
offset: .dw 0
lengthptr: .dw 0
available: .dw 0
copied: .dw 0

init:
 push ix
 push iy
 ; Forward the original CP/M BIOS console vectors, but intercept warm boot.
 ld hl,(1)
 dec hl
 dec hl
 dec hl
 ld de,#0xc680
 ld bc,#51
 ldir
 ld hl,#0xc000
 ld de,#0xc680
 ld bc,#6
 ldir
 xor a
 ld hl,#0
 ld de,#0x0402
 call 0xffca
 or a
 jr z,init_fail
 ; Copy addresses from mapper table entries into our veneers.
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (alloc+1),de
 inc hl
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (free+1),de
 inc hl
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (readseg+1),de
 inc hl
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (writeseg+1),de
 ld de,#18
 add hl,de          ; table +29; need +31 for PUT_P1 address
 inc hl
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (put1+1),de
 inc hl
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (get1+1),de
 inc hl
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (put2+1),de
 inc hl
 inc hl
 ld e,(hl)
 inc hl
 ld d,(hl)
 ld (get2+1),de
 xor a
 ld (nseg),a
 ld hl,#segs
alloc_loop:
 xor a
 ld b,#0
 call alloc
 jr c,alloc_failed
 ld (hl),a
 inc hl
 ld a,(nseg)
 inc a
 ld (nseg),a
 cp #4
 jr nz,alloc_loop
 ld a,(segs+3)
 ld (0xc60b),a
 xor a
 jr init_done
alloc_failed:
 call release
init_fail:
 ld a,#1
init_done:
 ld (0xc600),a      ; setup error
 pop iy
 pop ix
 ret

release:
 ld a,(nseg)
 or a
 ret z
 dec a
 ld (nseg),a
 ld e,a
 ld d,#0
 ld hl,#segs
 add hl,de
 ld a,(hl)
 ld b,#0
 call free
 jr release

launch:
 push ix
 push iy
 ld (savedsp),sp
 ld sp,#0xcbf0
 call get1
 ld (old1),a
 call get2
 ld (old2),a
 ; Save pages 0 and 1 through page 2.
 di
 ld a,(segs)
 call put2
 ld hl,#0
 ld de,#0x8000
 ld bc,#0x4000
 ldir
 ld a,(segs+1)
 call put2
 ld hl,#0x4000
 ld de,#0x8000
 ld bc,#0x4000
 ldir
 ld a,(old2)
 call put2
 ; Save page 2 through page 1.
 ld a,(segs+2)
 call put1
 ld hl,#0x8000
 ld de,#0x4000
 ld bc,#0x4000
 ldir
 ld a,(old1)
 call put1
 ei
 ; Constrain child TPA; preserve CP/M xx03 and xx06 entry conventions.
 ld hl,#0xc683
 ld (1),hl
 ld hl,#0xc006
 ld (6),hl
 ; Child command line (also provided via PARAMETERS by C frontend).
 ld hl,#0xc700
 ld de,#0x80
 ld bc,#128
 ldir
 ld a,#1
 ld (0x37),a
 ld de,#0x80
 ld c,#0x1a
 call dosgate
 ; Load pre-opened COM, whose size was validated before snapshot.
 ld a,(0xc601)
 ld b,a
 ld de,#0x100
 ld hl,(0xc602)
 ld c,#0x48
 call dosgate
 or a
 jr nz,load_fail
 ld de,(0xc602)
 or a
 sbc hl,de
 jr nz,load_short
 ld a,(0xc601)
 ld b,a
 ld c,#0x45
 call dosgate
 or a
 jr nz,load_fail
 ; Initialize FCB area, rather than inheriting stale shell FCBs.
 ld hl,#0x5c
 ld de,#0x5d
 ld bc,#35
 ld (hl),#0
 ldir
 ld sp,#0xbff0
 ld hl,#0
 push hl
 jp 0x100
load_short:
 ld a,#0xc7
load_fail:
 ld b,a
 ld c,#0x62
 call dosgate

warm:
 ld sp,#0xcbf0
 ; Clear handlers installed by the child before restoring shell memory.
 ld de,#0
 ld c,#0x63
 call dosgate
 ld de,#0
 ld c,#0x64
 call dosgate
 ld a,(0xc604)
 ld b,a
 ld c,#0x61
 call dosgate
 or a
 jr nz,join_fail
 ld a,b
join_fail:
 ld (0xc605),a
 ; Restore all low 48KB, including the original vectors and shell data.
 di
 ld a,(segs)
 call put2
 ld hl,#0x8000
 ld de,#0
 ld bc,#0x4000
 ldir
 ld a,(segs+1)
 call put2
 ld hl,#0x8000
 ld de,#0x4000
 ld bc,#0x4000
 ldir
 ld a,(old2)
 call put2
 ld a,(segs+2)
 call put1
 ld hl,#0x4000
 ld de,#0x8000
 ld bc,#0x4000
 ldir
 ld a,(old1)
 call put1
 ei
 ld sp,(savedsp)
 pop iy
 pop ix
 ret

savedsp: .dw 0
old1: .db 0
old2: .db 0
nseg: .db 0
segs: .db 0,0,0,0


