/* Per-document circular edit journal, outside document and yank regions.
   Four-byte slots hold a 21-bit position, byte and two-bit opcode. Complete transactions
   are evicted together. An oversized transaction is never partly undone. */
typedef struct {u16 first,last,now,saved;u8 valid;} History;
static History histories[2];
static u8 historyMute,historyRecording,historyOverflow;
static u16 historyStart;
static Pos historyBefore;
static Pos historyAddress(u16 index){
 return CLIP_BASE+LIMIT+256+((Pos)index<<2)+(activeBuffer?((Pos)undoMask+1)<<2:0);
}
static u8 historyKind(u16 index){return (rawGet(historyAddress(index)+2)>>6)+1;}
static Pos historyPosition(u16 index){
 Pos a=historyAddress(index),p=rawGet(a);
 p|=(u16)rawGet(a+1)<<8;
 return p|((Pos)(rawGet(a+2)&31)<<16);
}
static void historyReset(void){memset(&histories[activeBuffer],0,sizeof(History));}
static void historySaved(void){History *h=&histories[activeBuffer];h->saved=h->now;h->valid=1;}
static void historyBegin(void){historyBefore=cursor;historyRecording=historyOverflow=0;}
static u8 historyAppend(u8 kind,u8 c,Pos p){
 History *h=&histories[activeBuffer];Pos a;u8 k;
 if(((h->last+1)&undoMask)==h->first){
  if(historyRecording && h->first==historyStart){
   historyReset();historyRecording=0;historyOverflow=1;return 0;
  }
  do{
   if(h->saved==h->first)h->valid=0;
   k=historyKind(h->first);h->first=(h->first+1)&undoMask;
  }while(k!=4 && h->first!=h->last);
 }
 a=historyAddress(h->last);
 rawSet(a,(u8)p);rawSet(a+1,(u8)(p>>8));
 rawSet(a+2,(u8)(p>>16)|((kind-1)<<6));rawSet(a+3,c);
 h->last=(h->last+1)&undoMask;
 return 1;
}
static void historyLog(u8 kind,u8 c){
 History *h=&histories[activeBuffer];
 if(historyMute||historyOverflow)return;
 if(!historyRecording){
  if(((h->saved-h->first)&undoMask)>((h->now-h->first)&undoMask))h->valid=0;
  h->last=h->now;historyStart=h->last;
  historyAppend(3,0,historyBefore);historyRecording=1;
 }
 historyAppend(kind,c,cursor);
}
static void historyCommit(void){
 if(historyRecording && historyAppend(4,0,cursor))histories[activeBuffer].now=histories[activeBuffer].last;
 historyRecording=0;
 if(historyOverflow)note("Edit exceeds undo history");
}
static void historyApply(u8 redo){
 History *h=&histories[activeBuffer];u16 i=h->now;u8 kind,c;Pos p;
 if(i==(redo?h->last:h->first)){note(redo?"Nothing to redo":"Nothing to undo");return;}
 historyMute=1;
 for(;;){
  if(!redo)i=(i-1)&undoMask;
  kind=historyKind(i);p=historyPosition(i);
  if(kind==(redo?4:3)){cursor=p;h->now=redo?(i+1)&undoMask:i;break;}
  if(kind<3){
   cursor=p;c=rawGet(historyAddress(i)+3);
   if((kind==1)==redo)add(c);else del(cursor+1);
  }
  if(redo)i=(i+1)&undoMask;
 }
 historyMute=0;dirty=!(h->valid && h->now==h->saved);
 numberedFrom=0;firstNumber=1;repaint=1;
 if(top>cursor)top=bol(cursor);
}
