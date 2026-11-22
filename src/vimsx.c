/* vimSX for MSX-DOS2. Document storage is a gap buffer in V9968 VRAM. */
#include <string.h>
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long Pos;
#define BASE 0x40000UL
#define LIMIT 0x100000UL
#define CLIP_BASE 0x200000UL
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
static u8 line[130],buf[256],width;
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
/* Internal display, independent of the document VRAM port. */
__sfr __at(0x98) videoData;
__sfr __at(0x99) videoCtrl;
static u8 graphics,gx,gy,escape,foreground,background,bodyRows,gutter;
static u8 font[2048],cursorPixels[3],cursorVisible;
static u16 cursorAddr;
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
 ld de,#_font
 ld bc,#2048
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
static void videoAddress(u16 a,u8 write){
 videoCtrl=a>>14;videoCtrl=0x8e;videoCtrl=a;videoCtrl=((a>>8)&63)|write;
}
#define FORCLR (*(volatile u8*)0xf3e9)
#define BAKCLR (*(volatile u8*)0xf3ea)
#define BDRCLR (*(volatile u8*)0xf3eb)
static u8 dosInk,dosPaper,dosBorder,ink,paper;
static void applyColors(void) __critical {
 FORCLR=ink;BAKCLR=paper;BDRCLR=paper;
 videoCtrl=paper;videoCtrl=0x87;
}
static void clearAsciiPage(void) __critical {
 u16 i;
 videoCtrl=paper;videoCtrl=0x87;
 videoCtrl=(*(u8*)0xffe7)|2;videoCtrl=0x88;
 videoAddress(0,64);
 for(i=0;i<212u*128u;i++)videoData=paper*17;
 videoCtrl=0;videoCtrl=0x8e;
}
static u16 cellAddress(void){return (u16)gy*1024+gx*3+4;}
static void caretLineCore(u8 show) {
 u8 i;
 if(show){
  cursorAddr=cellAddress()+896;
  videoAddress(cursorAddr,0);for(i=0;i<3;i++)cursorPixels[i]=videoData;
 }
 videoAddress(cursorAddr,64);
 for(i=0;i<3;i++)videoData=show?(cursorPixels[i]^255):cursorPixels[i];
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
static void drawCharCore(u8 c) {
 u8 y,j,bits,l,r;u16 a=cellAddress();
 for(y=0;y<8;y++,a+=128){
  bits=font[(u16)c*8+y];
  /* Printable ASCII already fits in five dots plus one spacing dot.
     Preserve its strokes; only condense the wider non-ASCII BIOS glyphs. */
  if(c<32 || c>126)
   bits=(bits&0xe0)|((bits&0x18)?16:0)|((bits&6)?8:0)|((bits&1)?4:0);
  if(gy<bodyRows && gx<gutter && c>='0' && c<='9')bits=digits[c-'0'][y];
  videoAddress(a,64);
  for(j=0;j<3;j++){
   l=bits&128?foreground:background;r=bits&64?foreground:background;
   videoData=(l<<4)|r;bits<<=2;
  }
 }
 videoCtrl=0;videoCtrl=0x8e;
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
 u16 index,bits,addr=cellAddress();
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
 for(y=0;y<8;y++,addr+=128){
  i=(y&3)*2+(y>=4?16:0);
  bits=((u16)(wideTile[i]|wideTile[i+1])<<8)|(wideTile[i+8]|wideTile[i+9]);
  __critical {
   videoAddress(addr,64);
   for(x=0;x<12;x++){
    pixel=(bits&0x8000)?foreground:background;bits<<=1;
    if(x%3==2){if(bits&0x8000)pixel=foreground;bits<<=1;}
    if(x&1)videoData=packed|pixel;else packed=pixel<<4;
   }
   videoCtrl=0;videoCtrl=0x8e;
  }
 }
}
/* Clear the remaining character cells one scanline at a time. */
static void clearLineTail(void) __critical {
 u8 y,n,count=(40-gx)*3,packed=background*17;
 u16 addr=cellAddress();
 for(y=0;y<8;y++,addr+=128){
  videoAddress(addr,64);
  for(n=0;n<count;n++)videoData=packed;
 }
 videoCtrl=0;videoCtrl=0x8e;gx=40;
}
static u8 lead(u8 c);
static void put(u8 c){
 if(!graphics){arg=c;fn=2;dos();return;}
 if(cursorVisible)caretLine(0);
 if(escape){escape=0;if(c=='K' && gx<40)clearLineTail();return;}
 if(c==27){firstByte=0;escape=1;return;}
 if(firstByte){if(gx+2<=40){drawWide(c);gx+=2;}firstByte=0;return;}
 if(lead(c)){firstByte=c;return;}
 if(c==8){if(gx)gx--;return;}
 if(c>=32&&gx<40){drawChar(c);gx++;}
}
static void str(const char *s){while(*s)put(*s++);}
static u8 commandKeys,pending;
static u8 key(void){

 static const u8 hiragana[]={130,240,130,159,130,161,130,163,130,165,130,167,130,225,130,227,130,229,130,193,129,91,130,160,130,162,130,164,130,166,130,168,130,169,130,171,130,173,130,175,130,177,130,179,130,181,130,183,130,185,130,187,130,189,130,191,130,194,130,196,130,198,130,200,130,201,130,202,130,203,130,204,130,205,130,208,130,211,130,214,130,217,130,220,130,221,130,222,130,223,130,224,130,226,130,228,130,230,130,231,130,232,130,233,130,234,130,235,130,237,130,241};
 u8 c,index;
 if(pending){c=pending;pending=0;return c;}
 if(commandKeys){do{*(volatile u8*)0xfcac=0;fn=6;arg=255;dos();}while(!err);return err;}
 if(!graphics){fn=7;dos();return err;}
 do{fn=6;arg=255;dos();}while(!err);
 c=err;
 if((c>=0x86 && c<=0x9f)||(c>=0xe0 && c<=0xfd)){
  index=c<0xa0?c-0x86:c-0xe0+26;
  pending=hiragana[index*2+1];return hiragana[index*2];
 }
 return c;
}
static void locate(u8 x,u8 y){
 if(graphics){if(cursorVisible)caretLine(0);firstByte=0;gx=x;gy=y;return;}
 put(27);put('Y');put(32+y);put(32+x);
}
static void reg(u8 r,u8 v){control=v;control=128|r;}
static void address(Pos a,u8 write){reg(28,a>>18);reg(14,(a>>14)&15);control=a;control=((a>>8)&63)|write;}
/* Restore bank zero before returning control to DOS or the BIOS ISR. */
static u8 pages[4][256],changed[4];
static u16 tags[4];
static void transfer(u16 page,u8 *p,u8 write) __critical {
 u16 i;address(BASE+((Pos)page<<8),write);
 if(write){for(i=0;i<256;i++)data=p[i];}
 else{for(i=0;i<256;i++)p[i]=data;}
 reg(28,0);reg(14,0);
}
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
static Pos prev(Pos p){Pos a,b;if(!p)return 0;if(at(p-1)==10)return p-1;a=bol(p);while((b=step(a))<p)a=b;return a;}
static Pos numberedFrom,firstNumber,lineCount;
static void add(u8 c){if(cursor<numberedFrom)numberedFrom=0;gap();if(left<right){set(left,c);if(c==10)lineCount++;left++;cursor++;dirty=1;}}
static void del(Pos end){Pos p;for(p=cursor;p<end;p++)if(at(p)==10)lineCount--;if(cursor<numberedFrom)numberedFrom=0;gap();right+=end-cursor;dirty=1;}
#define BODY_ROWS 23
static u8 painted[BODY_ROWS][81],rowText[81],repaint;
static u8 numbers;
static Pos rowStarts[BODY_ROWS];
static void note(const char *s){strcpy(message,s);repaint=1;}
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
 if(available<newline){note("File limit: 1 MiB");return;}
 available-=newline;
 for(n=0;n<count;n++){
  if(available<clipLength){note("File limit: 1 MiB");return;}
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
static Pos visualRow(Pos p,u8 *caret){
 u8 x=0,c,n,*src;u16 available=0;Pos size=length();
 *caret=255;rowAtEnd=0;
 while(x<width-gutter){
  if(p==size){rowAtEnd=1;if(p==cursor)*caret=x;break;}
  if(!available)src=renderPage(p,&available);
  c=*src;n=lead(c)&&p+1<size?2:1;
  if(x+n>width-gutter)break;
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
/* One packed scanline in the reserved row, centred on the text area. */
static void separator(void) __critical {
 u16 stride=(*(u8*)0xfcaf==7)?256:128;
 u8 height=graphics?8:16;
 u16 y=(u16)bodyRows*height+height/2;
 u16 n=stride-8;
 videoAddress(y*stride+4,64);
 while(n--)videoData=ink*17;
 videoCtrl=0;videoCtrl=0x8e;
}
/* The first eight pixels are outside both console text areas. */
static void clearTopMargin(void) __critical {
 u8 y,i;u16 stride=(*(u8*)0xfcaf==7)?256:128;
 for(y=0;y<8;y++){
  videoAddress((u16)y*stride,64);
  for(i=0;i<4;i++)videoData=paper*17;
 }
 videoCtrl=0;videoCtrl=0x8e;
}
static u8 statusIns,statusDirty;
static void display(void){
 Pos p,next,lineNo,value;u8 y,x,cx=0,cy=0,slot=0,used=0,i;
 foreground=ink;background=paper;
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
 if(!numberedFrom || numberedFrom>top){numberedFrom=0;firstNumber=1;}
 if(numbers)while(numberedFrom<top){if(at(numberedFrom)==10)firstNumber++;numberedFrom++;}
 lineNo=firstNumber;p=top;
 for(y=0;y<bodyRows;y++){
  next=visualRow(p,&x);
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
  if(repaint||strcmp((char*)rowText,(char*)painted[y])){
   i=0;
   if(graphics && !repaint){
    while(rowText[i] && rowText[i]==painted[y][i]){
     if(lead(rowText[i])){if(!rowText[i+1] || rowText[i+1]!=painted[y][i+1])break;i+=2;}
     else i++;
    }
   }
   locate(i,y);for(;rowText[i];i++){foreground=ink;put(rowText[i]);}foreground=ink;str("\033K");strcpy((char*)painted[y],(char*)rowText);
  }
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
 repaint=0;clearTopMargin();locate(cx,cy);if(graphics)caretLine(1);
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
 h=handle;fn=0x45;dos();if(!bad&&!err){dirty=0;return 1;}if(err)fileError=err;return 0;
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
  note("Cannot read file (or over 1 MiB)");return;
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
static void changeMode(u8 mode){
 if(graphics){if(cursorVisible)caretLine(0);graphics=0;biosScreen(dosScreen);}
 selectMode(mode);kanji=consoleMode();
 if(!kanji){
  biosScreen(5);
  videoCtrl=(*(u8*)0xffe8)|128;videoCtrl=0x89;
  applyColors();clearAsciiPage();graphics=1;escape=0;width=40;bodyRows=23;
 }else{
  width=*(u8*)0xf3b0;if(width<16||width>80)width=40;width--;
  bodyRows=*(u8*)0xf3b1;bodyRows=bodyRows>=25?22:bodyRows>=12?bodyRows-3:9;
  applyColors();put(12);
 }
 extension=0;reg(20,1);reg(21,0);reg(28,0);
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
 u8 k,n,i,fast=1,loadFailed=0;u16 count=0,repeat;Pos a,b,col;
 memset(documents,0,sizeof(documents));activeBuffer=0;bufferBase=clipLength=0;clipLines=0;message[0]=0;
 memset(tags,255,sizeof(tags));memset(changed,0,sizeof(changed));left=cursor=top=0;right=LIMIT;dirty=ins=kanji=commandKeys=pending=0;pattern[0]=0;
 dosInk=ink=FORCLR&15;dosPaper=paper=BAKCLR&15;dosBorder=BDRCLR;initialMode=kanji=consoleMode();width=*(u8*)0xf3b0;if(width<16||width>80)width=40;width--;numbers=gutter=0;numberedFrom=0;lineCount=1;repaint=1;
 extension=0;reg(20,1);reg(21,0);
 memcpy(savedFunctionKeys,(void*)0xf87f,160);memset((void*)0xf87f,0,160);
 if(fast){originalCPU=cpu(3);cpu(2);}
 n=*(u8*)0x80;i=0;while(i<n&&((char*)0x81)[i]==' ')i++;
 for(k=0;i<n&&k<63&&((char*)0x81)[i]>' ';i++)path[k++]=((char*)0x81)[i];
 path[k]=0;if(k){if(!file(0)&&fileError!=0xd7)loadFailed=1;}else strcpy(path,"TEXT.TXT");
 graphics=cursorVisible=0;selectMode(0);dosScreen=*(u8*)0xfcaf;readFont();changeMode(0);
 if(loadFailed)goto done;
 remember();
 for(;;){display();commandKeys=!ins;k=key();commandKeys=0;if(*message){message[0]=0;repaint=1;}
  if(k>=28&&k<=31)k="lhkj"[k-28];
  else if(ins){if(k==27){ins=0;if(cursor>bol(cursor))cursor=prev(cursor);}else if(k==8&&cursor){a=cursor;cursor=prev(cursor);del(a);}else if(k==13)add(10);else if(k>=32){if(lead(k)){u8 trail=key();if(right-left>=2){add(k);add(trail);}}else add(k);}continue;}
  if(k>='0'&&k<='9'&&(k!='0'||count)){if(count<1000)count=count*10+k-'0';continue;}
  repeat=count?count:1;count=0;
  switch(k){
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
   else if(!strcmp(command,"z80")&&fast)cpu(0);
   else if(!strcmp(command,"r800")&&fast)cpu(2);
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
   else if(option("ascii"))changeMode(0);
   else if(option("kanji"))changeMode(1);
   break;
  }
 }
 done:memcpy((void*)0xf87f,savedFunctionKeys,160);FORCLR=dosInk;BAKCLR=dosPaper;BDRCLR=dosBorder;if(graphics){graphics=0;biosScreen(dosScreen);}selectMode(initialMode);FORCLR=dosInk;BAKCLR=dosPaper;BDRCLR=dosBorder;if(fast)cpu(originalCPU&3);put(12);
 if(loadFailed)str("Cannot read file (or over 1 MiB)\r\n");
}
