/* MSX-JE virtual terminal. No console/VDP rendering is delegated to DOS.
   Page 2 is unused by this VRAM-backed build and holds the JE desktop. */
static u16 jeEntry,jeSize,jeOut,jeMin,jePos,jeLength;
static u8 jeSlot,jeFn,jeStatus,jeReady,jeActive,jeLatch,jeX,jeY,jeResult[256];
static u8 jeCells[28],jeInk,jePaper;
static void jeResetWindow(void){memset(jeCells,' ',sizeof(jeCells));jeX=jeY=0;jeInk=ink;jePaper=paper;}
static void jeRestore(void){
 u8 i;foreground=jeInk;background=jePaper;locate(0,bodyRows);
 for(i=0;i<28;i++)put(jeCells[i]);str("\033K");
}
static void jePaintIfActive(void){if(jeReady && jeActive)jeRestore();}
static void jeFind(void) __naked {
 __asm
 push ix
 push iy
 ld hl,#0xc000
 ld a,(#0xf344)
 ld b,a
 xor a
 ld de,#0x1000
 call #0xffca
 ld (_jeOut),hl
 pop iy
 pop ix
 ret
 __endasm;
}
static void jeCall(void) __naked {
 __asm
 push ix
 push iy
 ld ix,(_jeEntry)
 ld a,(_jeSlot)
 ld h,a
 ld l,#0
 push hl
 pop iy
 ld hl,#0x8000
 ld de,(_jeSize)
 ld a,(#0xf343)
 ld b,a
 ld a,(_jeFn)
 call #0x001c
 ei
 ld (_jeStatus),a
 ld (_jeOut),hl
 ld (_jeMin),de
 pop iy
 pop ix
 ret
 __endasm;
}
static void jeClose(void){if(jeReady){jeFn=3;jeCall();jeReady=0;}jePos=jeLength=0;jeActive=jeLatch=0;}
static u8 jeInit(void){
 u8 *entry=(u8*)0xc000;
 jeReady=jeActive=jeLatch=0;jePos=jeLength=0;
 if(*(u16*)6<0xc200)return 0;
 memset(entry,255,64);jeFind();
 while((u16)entry<jeOut && (u16)entry<0xc040 && (entry[0]&3))entry+=4;
 if((u16)entry>=jeOut || (u16)entry>=0xc040)return 0;
 jeSlot=entry[1];jeEntry=entry[2]|((u16)entry[3]<<8);
 jeFn=1;jeSize=0;jeCall();if(jeMin>0x4000)return 0;
 jeSize=jeOut>0x4000?0x4000:jeOut;
 jeFn=2;jeCall();jeFn=4;jeCall();jeReady=1;jeResetWindow();return 1;
}
static void jeText(const u8 *s){
 u8 c,v,n,i,previous;u16 guard=0;
 foreground=jeInk;background=jePaper;
 while(guard++<1024 && (c=*s++)){
  if(c>=32){
   n=lead(c)&&*s?2:1;if(jeX+n>28)jeX=0;
   locate(jeX,bodyRows);jeCells[jeX++]=c;put(c);
   if(n==2){jeCells[jeX++]=*s;put(*s++);}
  }else if(c>=16){
   v=*s++;
   if(c==18){
    foreground=jePaper;background=jeInk;locate(jeX,bodyRows);
    for(i=jeX;i<28 && i-jeX<v;i++)put(jeCells[i]);
    foreground=jeInk;background=jePaper;
   }else if(c==24 && v)jeX=v<=28?v-1:27;
   else if(c==26){jeInk=v>>4;jePaper=v&15;if(jeInk==jePaper){jeInk=ink;jePaper=paper;}foreground=jeInk;background=jePaper;}
  }else if(c==5 || c==8 || c==12){
   if(c==12)jeX=0;
   if(c==8 && jeX){
    previous=0;for(i=0;i<jeX;){previous=i;i+=lead(jeCells[i])?2:1;}
    memset(jeCells+previous,' ',jeX-previous);jeX=previous;jeRestore();
   }else{memset(jeCells+jeX,' ',28-jeX);locate(jeX,bodyRows);str("\033K");}
  }
 }
}
static u8 jeToggle(void){
 u8 chord=!( *(volatile u8*)0xfbeb & 2) && !( *(volatile u8*)0xfbed & 1);
 if(!chord){jeLatch=0;return 0;}
 if(!jeLatch){
  jeLatch=1;jeActive^=1;
  if(jeReady){jeFn=4;jeCall();jeResetWindow();repaint=1;if(jeActive)jeRestore();else display();}
 }
 return 1;
}
static u8 japaneseKey(void){
 u8 status;
 if(jePos<jeLength)return jeResult[jePos++];
 for(;;){
  /* BIOS may enqueue SPACE for this chord. It is a mode key, not text. */
  if(jeToggle()){fn=6;arg=255;dos();continue;}
  if(jeReady && jeActive){
   jeFn=6;jeCall();status=jeStatus;
   if(jeToggle())continue;
   if(status&1){jeText((u8*)jeOut);repaint=1;}
   if(status&2){
    jeFn=7;jeCall();jeLength=strlen((char*)jeOut);
    if(jeLength>255)jeLength=255;
    memcpy(jeResult,(void*)jeOut,jeLength);jePos=0;
    if(status&4){jeFn=4;jeCall();jeResetWindow();repaint=1;}
    if(jeLength)return jeResult[jePos++];
   }
   if(status&4){jeFn=4;jeCall();jeActive=0;repaint=1;return 27;}
   if(status&3)continue;
   continue;
  }
  fn=6;arg=255;dos();if(err){if(err==32 && !(*(volatile u8*)0xfbeb&2))continue;return rawKana(err);}
 }
}
