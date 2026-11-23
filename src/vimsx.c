/* vimSX for MSX-DOS2. Document storage is a gap buffer in V9968 VRAM. */
#include <string.h>
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long Pos;
#if defined(MAPPER_ONLY) || defined(NATIVE_V9968)
#define FAST_VIEW
#endif
#define BASE 0x40000UL
#ifdef MAPPER_ONLY
static Pos storageLimit;
static u16 undoMask;
#define LIMIT storageLimit
#define CLIP_BASE (storageLimit*2)
#define FULL_MESSAGE "File exceeds available mapper RAM"
#define READ_MESSAGE "Cannot read file (or mapper full)"
#else
#define undoMask 65535u
#define LIMIT 0x100000UL
#define CLIP_BASE 0x200000UL
#define FULL_MESSAGE "File limit: 1 MiB"
#define READ_MESSAGE "Cannot read file (or over 1 MiB)"
#endif
#ifdef EXTERNAL
#define PORT 0x88
#else
#define PORT 0x98
#endif
__sfr __at(PORT) data;
__sfr __at(PORT+1) control;
__sfr __at(PORT+4) extension;
static u8 fn,h,err,kanji,ins,dirty,originalCPU;
static u16 arg,amount;
static Pos left,right,cursor,top;
static Pos bufferBase,clipLength;
static u8 activeBuffer,clipLines;
typedef struct {
 Pos left,right,cursor,top,lines;
 u8 used,dirty;
 char path[64];
} Document;
static Document documents[2];
static char message[40];
static char path[64],command[80],pattern[80];
static u8 buf[256],width;
static void dos(void) __naked {
 __asm
 push ix
 push iy
 ld a,(_fn)
 ld c,a
 ld a,(_h)
 ld b,a
 ld de,(_arg)
 ld hl,(_amount)
 xor a
 call 5
 ld (_err),a
 ld a,b
 ld (_h),a
 ld (_amount),hl
 pop iy
 pop ix
 ret
 __endasm;
}
/* The native V9968 build renders on the same chip as document storage. */
#ifdef NATIVE_V9968
__sfr __at(PORT) videoData;
__sfr __at(PORT+1) videoCtrl;
__sfr __at(PORT+3) videoCommand;
#else
__sfr __at(0x98) videoData;
__sfr __at(0x99) videoCtrl;
#endif
static u8 graphics,gx,gy,escape,foreground,background,bodyRows,gutter;
#ifdef NATIVE_V9968
static u8 font[8];
static u16 fontAddress;
#else
static u8 font[2048];
#endif
static u8 cursorVisible;
static u8 cursorPixels[128],cursorBytes,cursorRows,cursorBlock;
static u8 at(Pos p);
static u8 lead(u8 c);
#ifdef NATIVE_V9968
#define CELL_W (kanji?8:6)
#define CELL_H (kanji?16:8)
static u8 japaneseKey(void);
static void jeClose(void);
static u8 jeInit(void);
#else
#define CELL_W 6
#define CELL_H 8
#endif
static Pos cursorAddr;
static u8 ascii80,wideScreen,leftMargin,initialColumns;
static u16 videoStride,screenHeight,atlasY;
static Pos rowAddress(u16 y){Pos a=(Pos)y<<7;return wideScreen?a<<1:a;}
static void biosScreen(u8 mode) __naked {
 mode;
 __asm
 push ix
 push iy
 ld iy,(#0xfcc0)
 ld ix,#0x005f
 call #0x001c
 ei
 pop iy
 pop ix
 ret
 __endasm;
}
/* Read CGTABL from the main BIOS slot with the DOS inter-slot entry. */
static void readFont(void) __naked {
 __asm
 push ix
 push iy
 ld hl,#4
 ld a,(#0xfcc1)
 call #0x000c
 ld e,a
 ld hl,#5
 ld a,(#0xfcc1)
 push de
 call #0x000c
 pop de
 ld h,a
 ld l,e
 __endasm;
#ifdef NATIVE_V9968
 __asm
 ld (_fontAddress),hl
 pop iy
 pop ix
 ret
 __endasm;
}
/* Only cache misses need BIOS glyph bytes; rendered glyphs remain in VRAM. */
static void readGlyph(u8 c) __naked {
 c;
 __asm
 push ix
 push iy
 ld l,a
 ld h,#0
 add hl,hl
 add hl,hl
 add hl,hl
 ld de,(_fontAddress)
 add hl,de
 ld de,#_font
 ld bc,#8
 __endasm;
#else
 __asm
 ld de,#_font
 ld bc,#2048
 __endasm;
#endif
 __asm
001$:
 push hl
 push de
 push bc
 ld a,(#0xfcc1)
 call #0x000c
 pop bc
 pop de
 pop hl
 ld (de),a
 inc hl
 inc de
 dec bc
 ld a,b
 or c
 jr nz,001$
 pop iy
 pop ix
 ret
 __endasm;
}
/* SDCC register ABI: address in HLDE, write flag on the stack.
   Set the 18-bit VRAM address without a 32-bit shift in the pixel loop. */
static void videoAddress(Pos a,u8 write) __naked {
 a;write;
 __asm
 ld a,l
 add a,a
 add a,a
 ld b,a
 ld a,d
 rlca
 rlca
 and #3
 or b
 out (_videoCtrl),a
 ld a,#0x8e
 out (_videoCtrl),a
 ld a,e
 out (_videoCtrl),a
 pop hl
 pop bc
 dec sp
 ld a,d
 and #63
 or c
 out (_videoCtrl),a
 jp (hl)
 __endasm;
}
#define FORCLR (*(volatile u8*)0xf3e9)
#define BAKCLR (*(volatile u8*)0xf3ea)
#define BDRCLR (*(volatile u8*)0xf3eb)
static u8 dosInk,dosPaper,dosBorder,ink,paper;
#ifdef NATIVE_V9968
#include "v9968_draw.h"
#endif
static void applyColors(void) __critical {
 FORCLR=ink;BAKCLR=paper;BDRCLR=paper;
 videoCtrl=paper;videoCtrl=0x87;
}
static void clearAsciiPage(void) __critical {
#ifdef NATIVE_V9968
 videoCtrl=paper;videoCtrl=0x87;
 videoCtrl=(*(u8*)0xffe7)|2;videoCtrl=0x88;
 nativeFill(0,0,videoStride*2,screenHeight,paper*17);
 memset(glyphValid,0,sizeof(glyphValid));
#else
 u16 i=212u*videoStride;u8 packed=paper*17;
 videoCtrl=paper;videoCtrl=0x87;
 videoCtrl=(*(u8*)0xffe7)|2;videoCtrl=0x88;
 videoAddress(0,64);
 while(i--)videoData=packed;
 videoCtrl=0;videoCtrl=0x8e;
#endif
}
static Pos cellAddress(void){
 u16 y=(u16)gy<<3,x=(u16)gx*3;
#ifdef NATIVE_V9968
 if(kanji){y<<=1;x=(u16)gx<<2;}
#endif
 return rowAddress(y)+x+(leftMargin>>1);
}
static void caretLineCore(u8 show) {
 u8 x,y,i=0,n,value,mask;Pos address;
 if(show){
  cursorAddr=cellAddress();cursorRows=CELL_H;
  cursorBlock=!ins && gy<bodyRows;
  cursorBytes=cursorBlock?(lead(at(cursor))?CELL_W:CELL_W/2):1;
 }
 mask=ink^paper;mask=cursorBlock?mask*17:mask<<4;
 address=cursorAddr;i=0;
 for(y=0;y<cursorRows;y++,address+=videoStride){
  if(show){
   videoAddress(address,0);n=i;
   for(x=0;x<cursorBytes;x++)cursorPixels[n++]=videoData;
  }
  videoAddress(address,64);
  for(x=0;x<cursorBytes;x++){value=cursorPixels[i++];videoData=show?value^mask:value;}
 }
 videoCtrl=0;videoCtrl=0x8e;cursorVisible=show;
}
static void caretLine(u8 show) __naked {
 show;
 __asm
 di
 call _caretLineCore
 ei
 ret
 __endasm;
}
static const u8 digits[10][8]={
 {112,136,152,168,200,136,112,0},{32,96,32,32,32,32,112,0},
 {112,136,8,16,32,64,248,0},{240,8,8,112,8,8,240,0},
 {16,48,80,144,248,16,16,0},{248,128,128,240,8,8,240,0},
 {48,64,128,240,136,136,112,0},{248,8,16,32,64,64,64,0},
 {112,136,136,112,136,136,112,0},{112,136,136,120,8,16,96,0}
};
#ifdef NATIVE_V9968
#include "ank_font.h"
#endif
static void drawCharCore(u8 c) {
 u8 y,j,bits,l,r;Pos a=cellAddress();
#ifdef NATIVE_V9968
 u8 savedX=gx,savedY=gy,cached=!(gy<bodyRows && gx<gutter && c>='0' && c<='9');
 if(cached){
  if(glyphValid[c] && glyphColors[c]==(foreground<<4|background))goto cachedGlyph;
  gx=c%width;gy=atlasY/CELL_H+c/width;a=cellAddress();
 }
 if(!kanji || c<32 || c>126)readGlyph(c);
#endif
 for(y=0;y<CELL_H;y++,a+=videoStride){
#ifdef NATIVE_V9968
  if(kanji){
   if(c>=32 && c<=126)bits=y>=2 && y<14?ankFont[c-32][y-2]:0;
   else bits=y>=4 && y<12?font[y-4]:0;
  }else
#endif
  bits=font[
#ifdef NATIVE_V9968
   y
#else
   (u16)c*8+y
#endif
  ];
  /* Printable ASCII already fits in five dots plus one spacing dot.
     Preserve its strokes; only condense the wider non-ASCII BIOS glyphs. */
  if(CELL_W==6 && (c<32 || c>126))
   bits=(bits&0xe0)|((bits&0x18)?16:0)|((bits&6)?8:0)|((bits&1)?4:0);
  if(CELL_H==8 && gy<bodyRows && gx<gutter && c>='0' && c<='9')bits=digits[c-'0'][y];
  videoAddress(a,64);
  for(j=0;j<CELL_W/2;j++){
   l=bits&128?foreground:background;r=bits&64?foreground:background;
   videoData=(l<<4)|r;bits<<=2;
  }
 }
 videoCtrl=0;videoCtrl=0x8e;
#ifdef NATIVE_V9968
 if(cached){
  gx=savedX;gy=savedY;glyphValid[c]=1;glyphColors[c]=foreground<<4|background;
cachedGlyph:
  nativeCopy((c%width)*CELL_W+leftMargin,atlasY+(c/width)*CELL_H,gx*CELL_W+leftMargin,gy*CELL_H,CELL_W,CELL_H);
 }
#endif
}
static void drawChar(u8 c) __naked {
 c;
 __asm
 di
 call _drawCharCore
 ei
 ret
 __endasm;
}
__sfr __at(0xd8) kanjiLow;
__sfr __at(0xd9) kanjiHigh;
__sfr __at(0xda) kanji2Low;
__sfr __at(0xdb) kanji2High;
static u8 firstByte,wideTile[32];
static void drawWide(u8 trail){
 u8 row,col,i,y,x,pixel,packed=0;
 u16 index,bits;Pos addr=cellAddress();
 row=(firstByte<(u8)0xa0?firstByte-0x81:firstByte-0xc1)*2+0x21;
 if(trail>=0x9f){row++;col=trail-0x7e;}else col=trail-(trail<0x7f?0x1f:0x20);
 if(row>=0x50){
  index=(u16)(row-0x50)*96+col-0x20;
  kanji2Low=index&63;kanji2High=index>>6;
  for(i=0;i<32;i++)wideTile[i]=kanji2High;
 }else{
  index=(u16)(row-0x20)*96+col-0x20;
  if(row>=0x30)index-=512;
  kanjiLow=index&63;kanjiHigh=index>>6;
  for(i=0;i<32;i++)wideTile[i]=kanjiHigh;
 }
 for(y=0;y<CELL_H;y++,addr+=videoStride){
  if(CELL_H==16){i=(y&7)+(y>=8?16:0);bits=((u16)wideTile[i]<<8)|wideTile[i+8];}
  else{i=(y&3)*2+(y>=4?16:0);bits=((u16)(wideTile[i]|wideTile[i+1])<<8)|(wideTile[i+8]|wideTile[i+9]);}
  __critical {
   videoAddress(addr,64);
   for(x=0;x<CELL_W*2;x++){
    pixel=(bits&0x8000)?foreground:background;bits<<=1;
    if(CELL_W==6 && x%3==2){if(bits&0x8000)pixel=foreground;bits<<=1;}
    if(x&1)videoData=packed|pixel;else packed=pixel<<4;
   }
   videoCtrl=0;videoCtrl=0x8e;
  }
 }
}
/* Clear the remaining character cells one scanline at a time. */
#ifdef NATIVE_V9968
static void clearLineTail(void) __critical {
 if(gx<width)nativeFill(gx*CELL_W+leftMargin,gy*CELL_H,(width-gx)*CELL_W,CELL_H,background*17);
 gx=width;
}
#else
/* No register argument crosses SDCC's __critical entry sequence. */
static u8 clearEnd;
static void clearLineCore(void) __critical {
 u8 y,n,count=(clearEnd-gx)*3,packed=background*17;
 Pos addr=cellAddress();
 for(y=0;y<8;y++,addr+=videoStride){
  videoAddress(addr,64);
  for(n=0;n<count;n++)videoData=packed;
 }
 videoCtrl=0;videoCtrl=0x8e;gx=clearEnd;
}
static void clearLineTo(u8 end){clearEnd=end;clearLineCore();}
static void clearLineTail(void){clearLineTo(width);}
#endif
static u8 lead(u8 c);
static void put(u8 c){
 if(!graphics){arg=c;fn=2;dos();return;}
 if(cursorVisible)caretLine(0);
 if(escape){escape=0;if(c=='K' && gx<width)clearLineTail();return;}
 if(c==27){firstByte=0;escape=1;return;}
 if(firstByte){if(gx+2<=width){drawWide(c);gx+=2;}firstByte=0;return;}
 if(lead(c)){firstByte=c;return;}
 if(c==8){if(gx)gx--;return;}
 if(c>=32&&gx<width){drawChar(c);gx++;}
}
static void str(const char *s){while(*s)put(*s++);}
static u8 commandKeys,pending;
static u8 rawKana(u8 c){
 static const u8 hiragana[]={130,240,130,159,130,161,130,163,130,165,130,167,130,225,130,227,130,229,130,193,129,91,130,160,130,162,130,164,130,166,130,168,130,169,130,171,130,173,130,175,130,177,130,179,130,181,130,183,130,185,130,187,130,189,130,191,130,194,130,196,130,198,130,200,130,201,130,202,130,203,130,204,130,205,130,208,130,211,130,214,130,217,130,220,130,221,130,222,130,223,130,224,130,226,130,228,130,230,130,231,130,232,130,233,130,234,130,235,130,237,130,241};
 u8 index;
 if((c>=0x86 && c<=0x9f)||(c>=0xe0 && c<=0xfd)){
  index=c<0xa0?c-0x86:c-0xe0+26;
  pending=hiragana[index*2+1];return hiragana[index*2];
 }
 return c;
}
static u8 key(void){

 u8 c;
 if(pending){c=pending;pending=0;return c;}
 if(commandKeys){do{*(volatile u8*)0xfcac=0;fn=6;arg=255;dos();}while(!err);return err;}
#ifdef NATIVE_V9968
 if(kanji)return japaneseKey();
#endif
 if(!graphics){fn=7;dos();return err;}
 do{fn=6;arg=255;dos();}while(!err);
 return rawKana(err);
}
static void locate(u8 x,u8 y){
 if(graphics){if(cursorVisible)caretLine(0);firstByte=0;gx=x;gy=y;return;}
 put(27);put('Y');put(32+y);put(32+x);
}
static void reg(u8 r,u8 v){control=v;control=128|r;}
/* Restore bank zero before returning control to DOS or the BIOS ISR. */
static u8 pages[4][256],changed[4];
static u16 tags[4];
#ifdef MAPPER_ONLY
#include "mapper.h"
#define transfer mapperTransfer
#else
/* Use SCREEN 7's physical odd/even layout in every display mode.
   SCREEN 0-6 transfer the two 128-byte halves separately. No display-mode
   register is changed while accessing document or clipboard storage. */
static void vramTransfer(u16 page,u8 *p,u8 write) __critical {
 u16 a=page+0x400;u8 step=1,part=0,i,r14=(a>>6)&15,lo=0,hi=a;
#ifdef EXTERNAL
#ifdef NATIVE_V9968
 if(!graphics || !wideScreen)
#endif
#else
 /* DOS Kanji can leave SCRMOD at zero while R#0 selects SCREEN 7. */
 if((*(u8*)0xf3df&14)<10)
#endif
 {r14=(r14&8)|((r14>>1)&3);lo=a<<7;hi=a>>1;step=2;}
 reg(28,a>>10);
 do {
  reg(14,r14|(part<<2));control=lo;control=(hi&63)|write;i=part;
  do {if(write)data=p[i];else p[i]=data;i+=step;}while(i!=part);
 }while(++part<step);
 reg(28,0);reg(14,0);
}

#ifdef NATIVE_V9968
#include "mapper.h"
#include "mapper_cache.h"
#else
#define transfer vramTransfer
#endif
#endif
static u8 *pageFor(Pos a){
 u16 page=a>>8;u8 slot=(page^(page>>8))&3;
 if(tags[slot]!=page){
  if(changed[slot])transfer(tags[slot],pages[slot],64);
  transfer(page,pages[slot],0);changed[slot]=0;tags[slot]=page;
 }
 return pages[slot];
}
static u8 rawGet(Pos a){return pageFor(a)[(u8)a];}
static void rawSet(Pos a,u8 v){
 u16 page=a>>8;pageFor(a)[(u8)a]=v;changed[(page^(page>>8))&3]=1;
}
static u8 get(Pos a){return rawGet(bufferBase+a);}
static void set(Pos a,u8 v){rawSet(bufferBase+a,v);}
static Pos length(void){return LIMIT-right+left;}
static u8 at(Pos p){return p<length()?get(p<left?p:p+right-left):0;}
static void gap(void){
 while(left>cursor){--left;--right;set(right,get(left));}
 while(left<cursor){set(left,get(right));++left;++right;}
}
static u8 lead(u8 c){return (c>=129&&c<=159)||(c>=224&&c<=252);}
static Pos step(Pos p){if(p>=length())return p;return p+1+(lead(at(p))&&p+1<length());}
static Pos bol(Pos p){while(p&&at(p-1)!=10)--p;return p;}
static Pos eol(Pos p){while(p<length()&&at(p)!=10)p=step(p);return p;}
static Pos prev(Pos p);
static Pos numberedFrom,firstNumber,lineCount;
static u8 textChanged,changedLine;
static Pos changedFrom,changedTo;
static void changeRange(Pos end){
 if(!textChanged){changedFrom=cursor;changedTo=end;changedLine=0;}
 else{if(cursor<changedFrom)changedFrom=cursor;if(end>changedTo)changedTo=end;}
 textChanged=1;
}
static void historyLog(u8 kind,u8 c);
static void add(u8 c){if(cursor<numberedFrom)numberedFrom=0;gap();if(left<right){historyLog(1,c);changeRange(cursor+1);set(left,c);if(c==10){lineCount++;changedLine=1;}left++;cursor++;dirty=1;}}
static void del(Pos end){Pos p;u8 c;changeRange(end);for(p=cursor;p<end;p++){c=at(p);historyLog(2,c);if(c==10){lineCount--;changedLine=1;}}if(cursor<numberedFrom)numberedFrom=0;gap();right+=end-cursor;dirty=1;}
#define BODY_ROWS 23
static u8 painted[BODY_ROWS][81],rowText[81],repaint;
#ifdef NATIVE_V9968
static Pos paintedTop;
#endif
#ifdef FAST_VIEW
static Pos viewStart[BODY_ROWS],viewEnd[BODY_ROWS];
static u8 viewEOF[BODY_ROWS],viewRows;
static Pos viewLength;
#endif
static u8 numbers;
static Pos rowStarts[BODY_ROWS];
static void note(const char *s){strcpy(message,s);repaint=1;}
#include "undo.h"
static void remember(void){
 Document *d=&documents[activeBuffer];
 d->used=1;d->left=left;d->right=right;d->cursor=cursor;d->top=top;
 d->lines=lineCount;d->dirty=dirty;strcpy(d->path,path);
}
static void switchBuffer(u8 which){
 Document *d;
 if(which==activeBuffer)return;
 if(!documents[which].used){note("No other file open");return;}
 remember();activeBuffer=which;bufferBase=which?LIMIT:0;d=&documents[which];
 left=d->left;right=d->right;cursor=d->cursor;top=d->top;
 lineCount=d->lines;dirty=d->dirty;strcpy(path,d->path);
 numberedFrom=0;firstNumber=1;repaint=1;
}
static u8 unsaved(void){remember();return documents[0].dirty||documents[1].dirty;}
/* The shared register lives after both 1 MiB document regions in VRAM.
   A line register always includes its final newline, including at EOF. */
static void yank(Pos begin,Pos end,u8 lines){
 Pos p;clipLength=0;clipLines=lines;
 for(p=begin;p<end;p++)rawSet(CLIP_BASE+clipLength++,at(p));
 if(lines&&(end==begin||at(end-1)!=10))rawSet(CLIP_BASE+clipLength++,10);
}
static void lineRegister(u8 cut,u16 count){
 Pos begin=bol(cursor),end=begin;
 do{end=eol(end);if(end<length())end++;else break;}while(--count);
 yank(begin,end,1);
 if(cut){
  cursor=begin;
  if(end>begin)del(end);
  else if(cursor){cursor--;del(end);}
  if(cursor>=length()&&cursor)cursor=prev(cursor);
  cursor=bol(cursor);
 }
}
static void cutCharacters(u16 count){
 Pos end=cursor,stop=eol(cursor);
 while(count--&&end<stop)end=step(end);
 if(end==cursor)return;
 yank(cursor,end,0);del(end);
 if(cursor==eol(cursor)&&cursor>bol(cursor))cursor=prev(cursor);
}
static void paste(u8 before,u16 count){
 Pos p,start,available=right-left;u16 n;u8 newline=0;
 if(!clipLength){note("Register is empty");return;}
 if(clipLines){
  start=before?bol(cursor):eol(cursor);
  if(!before&&length()){
   if(start<length())start++;else newline=1;
  }
 }else start=before?cursor:(cursor<eol(cursor)?step(cursor):cursor);
 /* Check every repetition before writing, without overflow or long division. */
 if(available<newline){note(FULL_MESSAGE);return;}
 available-=newline;
 for(n=0;n<count;n++){
  if(available<clipLength){note(FULL_MESSAGE);return;}
  available-=clipLength;
 }
 cursor=start;if(newline){add(10);start++;}
 do{for(p=0;p<clipLength;p++)add(rawGet(CLIP_BASE+p));}while(--count);
 cursor=clipLines?start:prev(cursor);
}
/* Consume one visual row. EOF has its own cursor cell after a newline
   or a full-width row. A wide glyph belongs wholly to the next row. */
static u8 rowAtEnd;
/* Walk the current cache page directly, stopping at a page or gap boundary.
   Rendering does not mutate the document while this pointer is in use. */
static u8 *renderPage(Pos p,u16 *available){
 Pos a=bufferBase+(p<left?p:p+right-left);
 *available=256-(u8)a;
 if(p<left && left-p<*available)*available=(u16)(left-p);
 return pageFor(a)+(u8)a;
}
static Pos prev(Pos p){
 Pos a,b;u8 *src=0,n;u16 available=0;
 if(!p)return 0;if(at(p-1)==10)return p-1;
 a=p;
#ifdef FAST_VIEW
 /* Cached visual-row starts are known character boundaries, even on a
    long wrapped logical line. Never use them after a text mutation. */
 if(graphics && !repaint && !textChanged){
  u8 y;for(y=0;y<viewRows;y++)if(p>viewStart[y] && p<=viewEnd[y]){a=viewStart[y];break;}
 }
#endif
 if(a==p)a=bol(p);
 b=a;
 while(a<p){
  b=a;if(!available)src=renderPage(a,&available);
  n=lead(*src)&&a+1<p?2:1;a+=n;
  if(available>=n){src+=n;available-=n;}else available=0;
 }
 return b;
}
static Pos visualRow(Pos p,u8 *caret){
 u8 x=0,c,n,columns=width-gutter,*src=0;u16 available=0;Pos size=length();
 *caret=255;rowAtEnd=0;
 while(1){
  /* An insertion caret at the right edge still needs its next screen row. */
  if(x==columns && p==cursor)break;
  if(p==size){rowAtEnd=1;if(p==cursor)*caret=x;break;}
  if(!available)src=renderPage(p,&available);
  c=*src;n=lead(c)&&p+1<size?2:1;
  if(c!=10 && (u8)(x+n)>columns)break;
  if(p==cursor)*caret=x;
  if(c==10){p++;break;}
  rowText[x++]=c<32?' ':c;p++;src++;available--;
  if(n==2){
   if(!available)src=renderPage(p,&available);
   rowText[x++]=*src++;available--;p++;
  }
 }
 rowText[x]=0;return p;
}
#ifdef NATIVE_V9968
/* One packed scanline in the reserved row, centred on the text area. */
static void separator(void) __critical {
 u16 stride=graphics?videoStride:((*(u8*)0xfcaf==7)?256:128);
 u8 height=graphics?CELL_H:16;
 u16 y=(u16)bodyRows*height+height/2;
 u16 n=stride-(graphics?leftMargin:8);
 videoAddress(((Pos)y<<(stride==256?8:7))+(graphics?leftMargin/2:4),64);
 while(n--)videoData=ink*17;
 videoCtrl=0;videoCtrl=0x8e;
}
/* The first eight pixels are outside both console text areas. */
static void clearTopMargin(void) __critical {
 u8 y,i;u16 stride=graphics?videoStride:((*(u8*)0xfcaf==7)?256:128);
 for(y=0;y<8;y++){
  videoAddress(((Pos)y<<(stride==256?8:7)),64);
  for(i=0;i<(graphics?leftMargin/2:4);i++)videoData=paper*17;
 }
 videoCtrl=0;videoCtrl=0x8e;
}
#else
/* DOS Kanji may leave SCRMOD=0; use the actual R#0/R#9 shadows. */
static u16 consoleStride(void){return (*(u8*)0xf3df&14)==10?256:128;}
static void separator(void) __critical {
 u16 stride=graphics?videoStride:consoleStride();
 u16 y=(u16)bodyRows*(graphics?8:16)+(graphics?4:8),n;
 u8 fields=!graphics && (*(u8*)0xffe8&8)?2:1;
 Pos a;
 if(fields==2)y>>=1;
 a=((Pos)y<<(stride==256?8:7))+(graphics?leftMargin/2:4);
 do {
  videoAddress(a,64);n=stride-(graphics?leftMargin:8);
  while(n--)videoData=ink*17;
  a+=0x10000UL;
 }while(--fields);
 videoCtrl=0;videoCtrl=0x8e;
}
static void clearTopMargin(void) __critical {
 u8 y,i,fields=!graphics && (*(u8*)0xffe8&8)?2:1;
 u16 stride=graphics?videoStride:consoleStride();Pos page=0;
 /* DOS Kanji starts at x=0: its first glyph is not a margin. */
 if(!graphics)return;
 do {
  for(y=0;y<8;y++){
   videoAddress(page+((Pos)y<<(stride==256?8:7)),64);
   for(i=0;i<(graphics?leftMargin/2:4);i++)videoData=paper*17;
  }
  page+=0x10000UL;
 }while(--fields);
 videoCtrl=0;videoCtrl=0x8e;
}
#endif
static u8 statusIns,statusDirty;
static void paintTextRow(u8 y){
 u8 i=0;
#ifndef NATIVE_V9968
 u8 oldEnd;
#endif
 if(!repaint && !strcmp((char*)rowText,(char*)painted[y]))return;
 if(graphics && !repaint){
  while(rowText[i] && rowText[i]==painted[y][i]){
   if(lead(rowText[i])){if(!rowText[i+1] || rowText[i+1]!=painted[y][i+1])break;i+=2;}
   else i++;
  }
 }
 locate(i,y);for(;rowText[i];i++)put(rowText[i]);
#ifndef NATIVE_V9968
 /* The cached blank tail remains valid unless this row became shorter. */
 if(repaint)str("\033K");
 else{
  oldEnd=strlen((char*)painted[y]);
  if(i<oldEnd){
   if(graphics)clearLineTo(oldEnd);
   else str("\033K");
  }
 }
#else
 str("\033K");
#endif
 strcpy((char*)painted[y],(char*)rowText);
}
#ifdef NATIVE_V9968
static void jePaintIfActive(void);
#endif
static void display(void){
 Pos p,next,lineNo,value;u8 y,x,cx=0,cy=0,slot=0,used=0,i;
 foreground=ink;background=paper;
#ifdef FAST_VIEW
 /* Cursor-only movement never changes text, line numbers or the status bar. */
 if(graphics && !repaint && !*message && statusIns==ins && statusDirty==dirty){
  if(textChanged && !changedLine){
   Pos delta=length()-viewLength,newEnd;
   for(y=0;y<viewRows;y++)if(changedFrom>=viewStart[y] &&
      (changedFrom<viewEnd[y] || (viewEOF[y] && changedFrom==viewEnd[y]))){
    newEnd=viewEnd[y]+delta;
    if(changedTo>viewEnd[y] && changedTo>newEnd)break;
    next=visualRow(viewStart[y],&x);
    if(next!=newEnd || rowAtEnd!=viewEOF[y] || x==255)break;
    if(gutter){memmove(rowText+gutter,rowText,strlen((char*)rowText)+1);memcpy(rowText,painted[y],gutter);}
    paintTextRow(y);viewEnd[y]=next;
    for(i=y+1;i<viewRows;i++){viewStart[i]+=delta;viewEnd[i]+=delta;}
    viewLength=length();textChanged=0;break;
   }
  }
  if(!textChanged){
   for(y=0;y<viewRows;y++)if(cursor>=viewStart[y] &&
      (cursor<viewEnd[y] || (viewEOF[y] && cursor==viewEnd[y]))){
    locate((u8)(cursor-viewStart[y])+gutter,y);caretLine(1);return;
   }
  }
 }
#endif
 {
  u8 wanted=0;Pos threshold=1000;
  if(numbers){wanted=4;while(lineCount>=threshold){wanted++;threshold=(threshold<<3)+(threshold<<1);}}
  if(wanted!=gutter){gutter=wanted;top=bol(top);repaint=1;}
 }
 if(cursor<top)top=bol(cursor);
 p=top;
 for(;;){
  rowStarts[slot]=p;slot=(slot+1)%bodyRows;if(used<bodyRows)used++;
  next=visualRow(p,&x);
  if(x!=255){top=used==bodyRows?rowStarts[slot]:top;break;}
  if(next<=p)break;
  p=next;
 }
#ifdef NATIVE_V9968
 if(graphics && !kanji && !repaint && top!=paintedTop){
  if(cursorVisible)caretLine(0);
  if(top==viewStart[1]){
   __critical {nativeCopy(leftMargin,8,leftMargin,0,width*6,(bodyRows-1)*8);}
   memmove(painted,painted+1,(bodyRows-1)*81);painted[bodyRows-1][0]=255;
  }else if(visualRow(top,&x)==paintedTop){
   __critical {nativeScrollDown(bodyRows);}
   memmove(painted+1,painted,(bodyRows-1)*81);painted[0][0]=255;
  }
 }
 paintedTop=top;
#endif
 if(!numberedFrom || numberedFrom>top){numberedFrom=0;firstNumber=1;}
 if(numbers)while(numberedFrom<top){if(at(numberedFrom)==10)firstNumber++;numberedFrom++;}
 lineNo=firstNumber;p=top;
#ifdef FAST_VIEW
 viewRows=0;
#endif
 for(y=0;y<bodyRows;y++){
  next=visualRow(p,&x);
#ifdef FAST_VIEW
  viewStart[y]=p;viewEnd[y]=next;viewEOF[y]=rowAtEnd;viewRows=y+1;
#endif
  if(x!=255){cx=x+gutter;cy=y;}
  if(numbers){
   memmove(rowText+gutter,rowText,strlen((char*)rowText)+1);
   memset(rowText,' ',gutter);
   if(!p||at(p-1)==10){
    static const Pos powers[]={1000000UL,100000UL,10000UL,1000UL,100UL,10UL,1UL};
    u8 digit,started=0;value=lineNo;for(i=0;i<7;i++){digit=0;while(value>=powers[i]){value-=powers[i];digit++;}if(digit||started||i==6){rowText[i+gutter-8]='0'+digit;started=1;}}
   }
   if(next>p && at(next-1)==10)lineNo++;
  }
  paintTextRow(y);
  if(rowAtEnd){
   for(y++;y<bodyRows;y++)if(repaint||strcmp((char*)painted[y],"~")){
    locate(0,y);str("~\033K");strcpy((char*)painted[y],"~");
   }
   break;
  }
  p=next;
 }
 foreground=ink;
 if(repaint)separator();
 if(*message){
 foreground=ink;background=paper;locate(0,bodyRows+1);str(message);str("\033K");
 }else if(repaint || statusIns!=ins || statusDirty!=dirty){
 foreground=paper;background=ink;locate(0,bodyRows+1);str(ins?"[INSERT] ":"[NORMAL] ");str(kanji?"KANJI ":"ASCII ");
 for(i=0;path[i] && i+17<width;i++){
  if(lead(path[i])&&i+18>=width)break;
  put(path[i]);if(lead(path[i])&&path[i+1])put(path[++i]);
 }
 str(dirty?" *\033K":"\033K");statusIns=ins;statusDirty=dirty;
 }
#ifdef FAST_VIEW
 viewLength=length();
#endif
 textChanged=0;
 repaint=0;clearTopMargin();
#ifdef NATIVE_V9968
 jePaintIfActive();
#endif
 locate(cx,cy);if(graphics)caretLine(1);
}
/* DOS maps RAM into page 0: read the main-ROM generation through RDSLT. */
static u8 machineVersion(void) __naked{
 __asm
 push ix
 push iy
 ld a,(#0xfcc1)
 ld hl,#0x002d
 call #0x000c
 pop iy
 pop ix
 ei
 ret
 __endasm;
}
static u8 cpu(u8 mode) __naked{
 mode;
 __asm
 push ix
 push iy
 ld iy,(#0xfcc0)
 cp #3
 jr nz,001$
 ld ix,#0x0183
 jr 002$
001$: or #0x80
 ld ix,#0x0180
002$: call #0x001c
 pop iy
 pop ix
 ret
 __endasm;
}
static u8 fileError;
static u8 file(u8 save){
 Pos p=0;u16 i,n;u8 bad=0,handle;
 fileError=0;arg=(u16)path;fn=save?0x44:0x43;h=0;dos();if(err){fileError=err;return 0;}handle=h;
 for(;;){
  if(save){n=0;while(p<length()&&n<254){u8 c=at(p);p++;if(c==10)buf[n++]=13;buf[n++]=c;}if(!n)break;}
  else n=256;
  h=handle;arg=(u16)buf;amount=n;fn=save?0x49:0x48;dos();
  if(!save&&err==0xc7)break;
  if(err||(save&&amount!=n)){fileError=err?err:1;bad=1;break;}
  if(!amount)break;
  if(!save)for(i=0;i<amount;i++){if(buf[i]==13)continue;if(left==right){fileError=1;bad=1;break;}set(left,buf[i]);if(buf[i]==10)lineCount++;left++;}
  if(bad)break;
 }
 h=handle;fn=0x45;dos();if(!bad&&!err){dirty=0;if(!save)historyReset();historySaved();return 1;}if(err)fileError=err;return 0;
}
static u8 samePath(const char *a,const char *b){
 u8 x,y;
 while(*a&&*b){
  x=*a++;y=*b++;
  if(lead(x)){if(x!=y||!*a||*a++!=*b++)return 0;continue;}
  if(x>='a'&&x<='z')x-=32;if(y>='a'&&y<='z')y-=32;
  if(x!=y)return 0;
 }
 return !*a&&!*b;
}
static void openOther(const char *name){
 u8 which,old=activeBuffer;
 while(*name==' ')name++;
 if(!*name||strlen(name)>63){note("Invalid filename");return;}
 remember();
 for(which=0;which<2;which++)if(documents[which].used&&samePath(name,documents[which].path)){switchBuffer(which);return;}
 which=activeBuffer^1;
 if(documents[which].used){note("Two files already open");return;}
 activeBuffer=which;bufferBase=which?LIMIT:0;
 left=cursor=top=0;right=LIMIT;dirty=0;lineCount=1;strcpy(path,name);
 if(!file(0)&&fileError!=0xd7){
  switchBuffer(old);memset(&documents[which],0,sizeof(Document));
  note(READ_MESSAGE);return;
 }
 remember();numberedFrom=0;firstNumber=1;repaint=1;
}
static u8 saveAll(void){
 u8 old=activeBuffer,i,ok=1;
 remember();
 for(i=0;i<2;i++)if(documents[i].used){switchBuffer(i);if(!file(1)){ok=0;break;}}
 switchBuffer(old);remember();if(!ok)note("Write failed");return ok;
}
/* Kanji driver EXTBIO device 11h: query (E=0), select (E=1). */
static u8 consoleMode(void) __naked {
 __asm
 push ix
 push iy
 ld de,#0x1100
 xor a
 call #0xffca
 pop iy
 pop ix
 ret
 __endasm;
}
static void selectMode(u8 mode) __naked {
 mode;
 __asm
 push ix
 push iy
 ld de,#0x1101
 call #0xffca
 pop iy
 pop ix
 ret
 __endasm;
}
static u8 initialMode;
static u8 dosScreen;
#ifdef NATIVE_V9968
#include "msxje.h"
#endif
static void changeMode(u8 mode){
#ifdef NATIVE_V9968
 jeClose();
#endif
 if(cursorVisible)caretLine(0);
 wideScreen=ascii80;
#ifdef NATIVE_V9968
 if(mode)wideScreen=1;
#endif
 videoStride=wideScreen?256:128;leftMargin=wideScreen?16:8;screenHeight=212;atlasY=256;
#ifdef NATIVE_V9968
 selectMode(0);kanji=mode;biosScreen(wideScreen?7:5);
#ifdef EXTERNAL
 {
  u8 r,v;
  __critical {
   for(r=0;r<24;r++){
    v=*(u8*)(r<8?0xf3df+r:0xffe7+r-8);
    if(r==1)v&=~32; /* external VDP must not drive the BIOS interrupt */
    reg(r,v);
   }
  }
 }
#endif
 videoCtrl=(*(u8*)0xffe8)|128;videoCtrl=0x89;
 width=kanji?60:(ascii80?80:40);bodyRows=23;
 /* FIL needs the wider R#2 address mask, or rows repeat after y=255. */
 __critical {reg(20,kanji?0x21:1);reg(2,kanji?63:31);}
 if(kanji){screenHeight=424;atlasY=512;}
 applyColors();clearAsciiPage();graphics=1;escape=firstByte=0;
 if(kanji && !jeInit())note("MSX-JE not available");
#else
 if(graphics){if(cursorVisible)caretLine(0);graphics=0;biosScreen(dosScreen);}
 /* The driver derives its bitmap width from LINL40, not SCRMOD. */
 *(u8*)0xf3ae=ascii80?80:40;
 /* Wide Kanji uses full 16-dot glyphs across both interlace fields. */
 selectMode(mode?(ascii80?3:1):0);kanji=consoleMode();
 if(!kanji){
  biosScreen(wideScreen?7:5);
#ifndef MAPPER_ONLY
  reg(20,1);
#endif
  videoCtrl=(*(u8*)0xffe8)|128;videoCtrl=0x89;
  applyColors();clearAsciiPage();graphics=1;escape=0;width=ascii80?80:40;bodyRows=23;
 }else{
  width=*(u8*)0xf3b0;if(width<16||width>80)width=40;width--;
  bodyRows=*(u8*)0xf3b1;bodyRows=bodyRows>=25?22:bodyRows>=12?bodyRows-3:9;
#ifndef MAPPER_ONLY
  reg(20,1);
#endif
  applyColors();put(12);
 }
#endif
#ifndef MAPPER_ONLY
 extension=0;
#ifndef NATIVE_V9968
 reg(20,1);
#endif
 reg(21,0);reg(28,0);
#endif
 top=bol(top);repaint=1;
}
static u8 option(const char *name){
 const char *p=command;
 while(*p && *name){u8 c=*p++;if(c>='A' && c<='Z')c+=32;if(c!=*name++)return 0;}
 return !*p && !*name;
}
static void commandLine(char prefix,u8 n){
 u8 first=0,i;
 /* Reserve a cell for the insertion cursor. Scroll by whole characters. */
 while(n-first>width-2)first+=lead(command[first])&&first+1<n?2:1;
 foreground=ink;background=paper;
 locate(0,bodyRows+1);
 put(prefix);
 for(i=first;i<n;i++)put(command[i]);
 str("\033K");
 locate(1+n-first,bodyRows+1);
 if(graphics)caretLine(1);
}
static void input(char prefix){
 u8 n=0,c,last;
 repaint=1;
 commandLine(prefix,n);
 while((c=key())!=13){
  if(c==27){n=0;break;}
  if(c==8&&n){
   last=0;
   while(last+1+(lead(command[last])!=0)<n)last+=lead(command[last])?2:1;
   n=last;
  }else if(c>=32){
   if(lead(c)){
    u8 trail=key();
    if(n+2<=78){command[n++]=c;command[n++]=trail;}
   }else if(n<78)command[n++]=c;
  }
  commandLine(prefix,n);
 }
 command[n]=0;
}
static void search(void){Pos p=cursor,i;u16 j,n=strlen(pattern);if(!n)return;for(i=0;i<length();i++){p=step(p);if(p==length())p=0;for(j=0;j<n&&p+j<length()&&at(p+j)==(u8)pattern[j];j++){}if(j==n){cursor=p;return;}}}
/* Literal substitution. Count growing matches before changing VRAM. */
static void substitute(u8 whole){
 char *old=command+(whole?3:2),*replacement=strchr(old,'/'),*flags;
 Pos begin=whole?0:bol(cursor),p,last=cursor,matches=0,freeBytes=right-left;
 u8 pass,skip,global=0,i,n,r;
 if(!replacement||replacement==old)return;
 *replacement++=0;
 flags=strchr(replacement,'/');
 if(flags){*flags++=0;if(*flags&&strcmp(flags,"g"))return;global=*flags=='g';}
 n=strlen(old);r=strlen(replacement);
 for(i=0;i<n;i++)if(lead(old[i])&&++i==n)return;
 for(i=0;i<r;i++)if(lead(replacement[i])&&++i==r)return;
 for(pass=(r<=n);pass<2;pass++){
  matches=0;skip=0;p=begin;
  while(p<length()){
   if(at(p)==10){if(!whole)break;skip=0;p++;continue;}
   if(!skip&&length()-p>=n){
    for(i=0;i<n&&at(p+i)==(u8)old[i];i++){}
    if(i==n){
     matches++;skip=!global;
     if(!pass){if(freeBytes<(u8)(r-n))return;freeBytes-=r-n;p+=n;}
     else{last=p;cursor=p;del(p+n);for(i=0;i<r;i++)add(replacement[i]);p=cursor;}
     continue;
    }
   }
   p=step(p);
  }
 }
 if(matches){cursor=bol(last);if(top>cursor)top=cursor;}
}
static u8 savedFunctionKeys[160];
void main(void){
 u8 k,n,i,fast,loadFailed=0;u16 count=0,repeat;Pos a,b,col;
 fast=machineVersion()==3;
 initialColumns=*(u8*)0xf3b0;ascii80=initialColumns>40;
 textChanged=changedLine=0;
#ifdef NATIVE_V9968
 jeReady=0;jePos=jeLength=0;
 mapperInit();memset(mapperCacheTags,255,sizeof(mapperCacheTags));memset(mapperCacheDirty,0,sizeof(mapperCacheDirty));
#endif
#ifdef MAPPER_ONLY
 graphics=0;
 if(!mapperInit()){str("vimSX requires 256KB mapper RAM and free segments.\r\n");return;}
#endif
 memset(documents,0,sizeof(documents));activeBuffer=0;bufferBase=clipLength=0;clipLines=0;message[0]=0;
 memset(histories,0,sizeof(histories));histories[0].valid=histories[1].valid=1;historyMute=historyRecording=historyOverflow=0;
 memset(tags,255,sizeof(tags));memset(changed,0,sizeof(changed));left=cursor=top=0;right=LIMIT;dirty=ins=kanji=commandKeys=pending=0;pattern[0]=0;
#ifndef MAPPER_ONLY
 graphics=cursorVisible=0;
#endif
 dosInk=ink=FORCLR&15;dosPaper=paper=BAKCLR&15;dosBorder=BDRCLR;initialMode=kanji=*(u8*)0xfcaf==0?0:consoleMode();numbers=gutter=0;numberedFrom=0;lineCount=1;repaint=1;
#ifndef MAPPER_ONLY
 extension=0;reg(20,1);reg(21,0);
#ifdef EXTERNAL
 /* Start storage access in a known non-interleaved mode. */
 reg(0,0);reg(1,0);
#endif
#endif
 memcpy(savedFunctionKeys,(void*)0xf87f,160);memset((void*)0xf87f,0,160);
 if(fast){originalCPU=cpu(3);cpu(2);}
 n=*(u8*)0x80;i=0;while(i<n&&((char*)0x81)[i]==' ')i++;
 for(k=0;i<n&&k<63&&((char*)0x81)[i]>' ';i++)path[k++]=((char*)0x81)[i];
 path[k]=0;if(k){if(!file(0)&&fileError!=0xd7)loadFailed=1;}else strcpy(path,"TEXT.TXT");
 graphics=cursorVisible=0;selectMode(0);dosScreen=*(u8*)0xfcaf;readFont();changeMode(initialMode);
 if(loadFailed)goto done;
 remember();
 for(;;){display();commandKeys=!ins;k=key();commandKeys=0;if(*message){message[0]=0;repaint=1;}
  if(!ins)historyBegin();
  if(k>=28&&k<=31)k="lhkj"[k-28];
  else if(ins){if(k==27){ins=0;if(cursor>bol(cursor))cursor=prev(cursor);historyCommit();}else if(k==8&&cursor){a=cursor;cursor=prev(cursor);del(a);}else if(k==13)add(10);else if(k>=32){if(lead(k)){u8 trail=key();if(right-left>=2){add(k);add(trail);}}else add(k);}continue;}
  if(k>='0'&&k<='9'&&(k!='0'||count)){if(count<1000)count=count*10+k-'0';continue;}
  repeat=count?count:1;count=0;
  switch(k){
  case 'u':case 18:do{historyApply(k==18);}while(--repeat);break;
  case 'h':if(cursor>bol(cursor))cursor=prev(cursor);break;
  case 'l':a=step(cursor);if(a<eol(cursor)||(ins&&a==eol(cursor)))cursor=a;break;
  case 'j':case 'k':a=bol(cursor);col=cursor-a;b=eol(cursor);if(k=='j'){if(b>=length())break;a=b+1;}else if(a)a=bol(a-1);else break;
   b=eol(a);cursor=a;while(cursor<b&&step(cursor)-a<=col&&(ins||step(cursor)<b))cursor=step(cursor);break;
  case '0':cursor=bol(cursor);break;
  case '$':a=eol(cursor);cursor=a>bol(cursor)?prev(a):a;break;
  case 'a':if(cursor<eol(cursor))cursor=step(cursor);
  case 'i':ins=1;break;
  case 'A':cursor=eol(cursor);ins=1;break;
  case 'x':cutCharacters(repeat);break;
  case 'd':if(key()=='d')lineRegister(1,repeat);break;
  case 'y':if(key()=='y')lineRegister(0,repeat);break;
  case 'p':paste(0,repeat);break;
  case 'P':paste(1,repeat);break;
  case '/':input('/');if(*command)strcpy(pattern,command);search();break;
  case 'n':search();break;
  case ':':input(':');
   if(option("undo")||option("redo")){historyApply(option("redo"));break;}
   if(!strcmp(command,"q!"))goto done;
   if(!strcmp(command,"q")){if(!unsaved())goto done;note("Unsaved file: use :wa or :q!");}
   else if(!strcmp(command,"w")||!strcmp(command,"wq")){
    if(!file(1))note("Write failed");
    else if(command[1]=='q'){if(!unsaved())goto done;note("Other file has unsaved changes");}
   }
   else if(!strcmp(command,"wa")||!strcmp(command,"wqa")){if(saveAll()&&command[2]=='a')goto done;}
   else if(!strncmp(command,"e ",2))openOther(command+2);
   else if(option("bn")||option("bp")||option("bnext")||option("bprevious"))switchBuffer(activeBuffer^1);
   else if(option("b1")||option("b 1"))switchBuffer(0);
   else if(option("b2")||option("b 2"))switchBuffer(1);
   else if(option("set number")||option("set nu")||option("set nonumber")||option("set nonu")){
    numbers=option("set number")||option("set nu");top=bol(top);repaint=1;
   }
   else if(!strcmp(command,"z80")){if(fast)cpu(0);}
   else if(!strcmp(command,"r800")){if(fast)cpu(2);else note("R800 requires MSX turbo R");}
   else if(!strncmp(command,"s/",2))substitute(0);
   else if(!strncmp(command,"%s/",3))substitute(1);
   else if(option("color 1")||option("color 2")||option("color 3")||option("color dos")){
    ink=dosInk;paper=dosPaper;
    if(command[6]>='1'&&command[6]<='3'){
     ink=command[6]=='3'?12:15;paper=command[6]=='1'?4:1;
    }
    if(cursorVisible&&graphics)caretLine(0);
    applyColors();if(graphics)clearAsciiPage();else put(12);repaint=1;
   }
   else if(option("mode80")||option("mode 80")){ascii80=1;changeMode(0);}
   else if(option("mode40")||option("mode 40")){ascii80=0;changeMode(0);}
   else if(option("ascii"))changeMode(0);
   else if(option("kanji"))changeMode(1);
   break;
  }
  if(!ins)historyCommit();
 }
 done:
#ifdef NATIVE_V9968
 jeClose();
#ifdef EXTERNAL
 /* DOS uses the main VDP. Do not leave a frozen editor on this output. */
 if(cursorVisible)caretLine(0);
 clearAsciiPage();
 foreground=ink;background=paper;
 locate(0,2);str("vimSX has exited.");
 locate(0,4);str("DOS: select the main VDP display.");
#endif
#endif
#if !defined(MAPPER_ONLY) && !defined(EXTERNAL)
 reg(20,1);
#endif
#ifndef NATIVE_V9968
 /* The Kanji driver also uses LINL40 when returning directly to DOS. */
 *(u8*)0xf3ae=initialColumns;
#endif
 memcpy((void*)0xf87f,savedFunctionKeys,160);FORCLR=dosInk;BAKCLR=dosPaper;BDRCLR=dosBorder;if(graphics){graphics=0;*(u8*)0xf3ae=initialColumns;biosScreen(dosScreen);}selectMode(initialMode);FORCLR=dosInk;BAKCLR=dosPaper;BDRCLR=dosBorder;if(fast)cpu(originalCPU&3);put(12);
 if(loadFailed)str(READ_MESSAGE "\r\n");
}
