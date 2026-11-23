/* DOS2-owned user segments; page 2 is borrowed only during a cache transfer.
   The program, cache and stack must remain outside 8000h..BFFFh. */
static u16 mapperTable;
#ifdef MAPPER_ONLY
#define MAPPER_SEGMENTS 193
#else
#define MAPPER_SEGMENTS 1
#endif
static u8 mapperSegments[MAPPER_SEGMENTS],mapperSlots[MAPPER_SEGMENTS],mapperPrimary,mapperSlot,mapperCount,mapperError;
static u16 mapperVariables;
static u8 mapperDiscover(void) __naked {
 __asm
 push ix
 push iy
 xor a
 ld hl,#0
 ld de,#0x0402
 call #0xffca
 ld (_mapperTable),hl
 ld c,a
 ld a,b
 ld (_mapperPrimary),a
 push bc
 xor a
 ld hl,#0
 ld de,#0x0401
 call #0xffca
 ld (_mapperVariables),hl
 pop bc
 ld a,c
 pop iy
 pop ix
 ret
 __endasm;
}
static void mapperJump(void) __naked {
 __asm
 jp (hl)
 __endasm;
}
static u8 mapperAllocate(void) __naked {
 __asm
 ld hl,(_mapperTable)
 ld a,(_mapperPrimary)
 or #0x20
 ld b,a
 xor a
 call _mapperJump
 ld c,a
 ld a,b
 ld (_mapperSlot),a
 ld a,#0
 adc a,#0
 ld (_mapperError),a
 ld a,c
 ret
 __endasm;
}
static u8 mapperGetPage(void) __naked {
 __asm
 ld hl,(_mapperTable)
 ld de,#0x27
 add hl,de
 jp (hl)
 __endasm;
}
static void mapperPutPage(u8 segment) __naked {
 segment;
 __asm
 ld hl,(_mapperTable)
 ld de,#0x24
 add hl,de
 jp (hl)
 __endasm;
}
static void mapperPutSlot(u8 slot) __naked {
 slot;
 __asm
 push ix
 push iy
 ld h,#0x80
 call #0x0024
 pop iy
 pop ix
 ret
 __endasm;
}
static u8 mapperInit(void){
 u8 segment,*entry;u16 total=0;
 mapperDiscover();entry=(u8*)mapperVariables;
 mapperCount=0;
 if(!mapperTable || !mapperVariables)return 0;
 do{total+=entry[1]?entry[1]:256;entry+=8;}while(*entry);
 if(total<16)return 0;
 while(mapperCount<MAPPER_SEGMENTS){
  segment=mapperAllocate();if(mapperError)break;
  mapperSegments[mapperCount]=segment;mapperSlots[mapperCount++]=mapperSlot;
 }
#ifdef MAPPER_ONLY
 /* Three equal regions plus a spare segment for linewise yank's final LF. */
 if(mapperCount<4)return 0;
 storageLimit=(Pos)((mapperCount-1)/3)<<14;
#endif
 return mapperCount!=0;
}
static void mapperTransfer(u16 page,u8 *p,u8 write) __critical {
 u8 old=mapperGetPage();
 u8 slot=mapperSlots[page>>6];
 u8 *mapped=(u8*)(0x8000u+((page&63)<<8));
 if(slot!=mapperPrimary)mapperPutSlot(slot);
 mapperPutPage(mapperSegments[page>>6]);
 if(write)memcpy(mapped,p,256);else memcpy(p,mapped,256);
 mapperPutPage(old);
 if(slot!=mapperPrimary)mapperPutSlot(mapperPrimary);
}
/* User segments are released by DOS2 on normal and abnormal termination. */
