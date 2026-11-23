/* Standard V99x8 command interface, on the selected V9968 ports.
   Glyph atlas: y=256 in ASCII, y=512 in FIL Kanji.
   All display/atlas addresses stay below text storage at 256 KiB.
   Wait for completion before CPU VRAM access or returning to the BIOS. */
static u8 glyphColors[256],glyphValid[256];
static void nativeWait(void){
 videoCtrl=2;videoCtrl=0x8f;
 while(videoCtrl&1){}
 videoCtrl=0;videoCtrl=0x8f;
}
static void nativeWord(u16 value){videoCommand=value;videoCommand=value>>8;}
static void nativeFill(u16 x,u16 y,u16 w,u16 h,u8 color){
 nativeWait();videoCtrl=36;videoCtrl=0x91;
 nativeWord(x);nativeWord(y);nativeWord(w);nativeWord(h);
 videoCommand=color;videoCommand=0;videoCommand=0xc0;nativeWait();
}
static void nativeCopy(u16 sx,u16 sy,u16 dx,u16 dy,u16 w,u16 h){
 nativeWait();videoCtrl=32;videoCtrl=0x91;
 nativeWord(sx);nativeWord(sy);nativeWord(dx);nativeWord(dy);
 nativeWord(w);nativeWord(h);
 videoCommand=0;videoCommand=0;videoCommand=0xd0;nativeWait();
}
static void nativeScrollDown(u8 rows){
 nativeWait();videoCtrl=32;videoCtrl=0x91;
 nativeWord(leftMargin);nativeWord((rows-1)*8-1);nativeWord(leftMargin);nativeWord(rows*8-1);
 nativeWord(width*6);nativeWord((rows-1)*8);
 videoCommand=0;videoCommand=8;videoCommand=0xd0;nativeWait();
}
