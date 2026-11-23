/* MSX-DOS2 LS 1.0. SDCC Z80, no BIOS calls, no disk modifications. */
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;
#define MAX_ENTRIES 1024
#define NOFIL 0xd7
typedef struct { char name[13]; u8 attr; u16 time,date; u32 size; } Entry;
static Entry entries[MAX_ENTRIES];
static u16 order[MAX_ENTRIES],count;
static u8 fib[64],probe[64];
static char tail[128],path[130],out[256];
static char *args[32],*operands[32];
static u8 argc,npaths,all,almost,reverse,sortby,mark,dirself,human,format;
static u16 width,outlen;
static u8 status,handle,err,attr,fn,sub;
static u16 rp,rq,rr,amount,result;

/* Global register bridge: no dependence on SDCC argument register ABI.
 * Preserve IX/IY, which SDCC may use as frame pointers. */
static void dos(void) __naked {
 __asm
    push ix
    push iy
    ld de,(_rp)
    ld hl,(_rq)
    ld ix,(_rr)
    ld a,(_attr)
    ld b,a
    ld a,(_fn)
    ld c,a
    ld a,(_sub)
    call 5
    ld (_err),a
    ld (_result),de
    ld (_amount),hl
    ld a,b
    ld (_attr),a
    pop iy
    pop ix
    ret
 __endasm;
}
static void finish(u8 code) { attr=code;fn=0x62;dos(); }
static u16 length(const char *s) { u16 n=0;while(s[n])++n;return n; }
static void copy(char *d,const char *s) { while((*d++=*s++)); }
static int cmp(const char *a,const char *b) {
 while(*a && (u8)*a==(u8)*b){++a;++b;}return (int)(u8)*a-(u8)*b;
}
static void flush(void) {
 if(!outlen)return;
 rp=(u16)out;rq=outlen;attr=handle;fn=0x49;dos();
 if(err || amount!=outlen)finish(2);
 outlen=0;
}
static void put(char c) { if(outlen==sizeof(out))flush();out[outlen++]=c; }
static void text(const char *s) { while(*s)put(*s++); }
static void nl(void) { text("\r\n"); }
static void number(u32 n,u8 pad,char fill) {
 char b[11];u8 k=0;
 do {b[k++]=(char)('0'+n%10);n/=10;}while(n);
 while(pad>k){put(fill);--pad;}while(k)put(b[--k]);
}
static void error(const char *s,const char *item) {
 flush();handle=2;text("ls: ");text(s);
 if(item){text(": ");text(item);}nl();flush();handle=1;status=2;
}
static void doserror(const char *item,u8 code) {
 flush();handle=2;text("ls: ");text(item);text(": DOS error 0x");
 put("0123456789ABCDEF"[code>>4]);put("0123456789ABCDEF"[code&15]);
 nl();flush();handle=1;status=2;
}
static u8 find(char *p,u8 *dest,u8 attributes) {
 rp=(u16)p;rr=(u16)dest;attr=attributes;fn=0x40;dos();return err;
}
static u8 next(void) { rr=(u16)fib;fn=0x41;dos();return err; }
static u16 word(u8 *p) { return p[0]|((u16)p[1]<<8); }
static u32 dword(u8 *p) { return (u32)word(p)|((u32)word(p+2)<<16); }
static u8 dots(char *p) { return p[0]=='.' && (!p[1] || (p[1]=='.' && !p[2])); }
static u8 save(void) {
 Entry *e;
 if(fib[14]&8)return 1;
 if(!all && (fib[14]&6))return 1;
 if(dots((char*)fib+1) && (!all || almost))return 1;
 if(count==MAX_ENTRIES){error("too many entries (maximum 1024)",0);return 0;}
 e=&entries[count];copy(e->name,(char*)fib+1);e->attr=fib[14];
 e->time=word(fib+15);e->date=word(fib+17);e->size=dword(fib+21);
 order[count]=count;++count;return 1;
}
static void save_explicit(void) {
 u8 oldall=all,oldalmost=almost;
 all=1;almost=0;save();all=oldall;almost=oldalmost;
}
static int compare(u16 a,u16 b) {
 Entry *x=&entries[a],*y=&entries[b];int c=0;
 if(sortby==1){
  if(x->date!=y->date)c=x->date>y->date?-1:1;
  else if(x->time!=y->time)c=x->time>y->time?-1:1;
 }else if(sortby==2 && x->size!=y->size)c=x->size>y->size?-1:1;
 if(!c)c=cmp(x->name,y->name);
 return reverse?-c:c;
}
static void sort(void) {
 u16 gap,i,j,t;
 if(sortby==3){if(reverse)for(i=0;i<count/2;++i){t=order[i];order[i]=order[count-1-i];order[count-1-i]=t;}return;}
 for(gap=count/2;gap;gap/=2)for(i=gap;i<count;++i){
  t=order[i];j=i;
  while(j>=gap && compare(order[j-gap],t)>0){order[j]=order[j-gap];j-=gap;}
  order[j]=t;
 }
}
static u8 suffix(Entry *e) { return mark && (e->attr&16); }
static void name(Entry *e) { text(e->name);if(suffix(e))put('/'); }
static void longline(Entry *e) {
 u32 n=e->size;u8 unit=0;
 put((e->attr&16)?'d':'-');put((e->attr&1)?'r':'-');
 put((e->attr&2)?'h':'-');put((e->attr&4)?'s':'-');put((e->attr&32)?'a':'-');put(' ');
 if(human){while(n>=1024 && unit<3){n=n/1024+((n%1024)>=512);++unit;}number(n,4,' ');put(" KMG"[unit]);
 }else number(n,10,' ');
 put(' ');
 if(e->date){number(1980+(e->date>>9),4,'0');put('-');number((e->date>>5)&15,2,'0');put('-');number(e->date&31,2,'0');}
 else text("----------");
 put(' ');
 if(e->date){number(e->time>>11,2,'0');put(':');number((e->time>>5)&63,2,'0');}else text("--:--");
 put(' ');name(e);nl();
}
static void display(void) {
 u16 i,r,c,cols,rows,cell=0,n,index;
 sort();
 if(format==1){for(i=0;i<count;++i)longline(&entries[order[i]]);return;}
 if(format==2){for(i=0;i<count;++i){name(&entries[order[i]]);nl();}return;}
 for(i=0;i<count;++i){n=length(entries[i].name)+suffix(&entries[i]);if(n>cell)cell=n;}
 cell+=2;cols=(width+2)/cell;if(!cols)cols=1;if(cols>count)cols=count;if(!cols)return;
 rows=(count+cols-1)/cols;
 for(r=0;r<rows;++r){
  for(c=0;c<cols;++c){
   index=format==3?r*cols+c:c*rows+r;
   if(index>=count)break;
   i=order[index];name(&entries[i]);
   index=format==3?index+1:index+rows;
   if(c+1<cols && index<count){n=length(entries[i].name)+suffix(&entries[i]);while(n++<cell)put(' ');}
  }nl();
 }
}
static void listing(char *p) {
 u8 code,wild=0,isdir=0;u16 i,n;
 count=0;copy(path,p);n=length(path);
 for(i=0;i<n;++i){if(path[i]=='/')path[i]='\\';if(path[i]=='*'||path[i]=='?')wild=1;}
 /* Strip a directory's trailing separator before resolving its existence.
  * Preserve root (\\) and drive root (A:\\). */
 while(n>1 && path[n-1]=='\\' && !(n==3 && path[1]==':'))path[--n]=0;
 if(!n || path[n-1]=='\\' || path[n-1]==':')isdir=1;
 if(dirself && isdir){
  code=find(path,probe,0x16);
  if((code && code!=NOFIL) || n>3){doserror(p,code?code:0xd9);return;}
  /* Roots have no directory entry/date; bare drive refers to its CWD. */
  for(i=0;i<64;++i)fib[i]=0;
  copy((char*)fib+1,n?path:".");fib[14]=16;
  save_explicit();display();return;
 }
 /* Resolve an explicit directory first; then search inside it. */
 if(!wild && !isdir){
  code=find(path,probe,0x16);
  if(code){doserror(p,code);return;}
  if((probe[14]&16) && !dirself){path[n++]='\\';path[n]=0;isdir=1;}
  else {for(i=0;i<64;++i)fib[i]=probe[i];
   /* Explicit hidden/system operands are visible without -a. */
   save_explicit();display();return;}
 }
 code=find(path,fib,all?0x16:0x10);
 if(code){if(code!=NOFIL || !isdir)doserror(p,code);return;}
 do {if(!save())return;code=next();}while(!code);
 if(code!=NOFIL){doserror(p,code);return;}
 display();
}
static u8 numeric(char *s) { if(!*s)return 0;while(*s){if(*s<'0'||*s>'9')return 0;++s;}return 1; }
static u8 setwidth(char *s) {
 u16 n=0;if(!numeric(s))return 0;
 while(*s){if(n>100)return 0;n=n*10+(*s++-'0');}
 if(!n || n>255)return 0;width=n;return 1;
}
static void help(void) {
 text("LS 1.0 for MSX-DOS2\r\nUsage: LS [options] [path ...]\r\n"
 " -l  size, date, time, DOS attributes\r\n"
 " -w [N]  columns; optional width 1..255\r\n"
 " -C  columns down   -x  columns across\r\n"
 " -1  one per line   -a  hidden/system\r\n"
 " -A  like -a, except . and ..\r\n"
 " -r  reverse       -t  newest first\r\n"
 " -S  largest first -U  disk order\r\n"
 " -F / -p  append / to directories\r\n"
 " -h  sizes in K/M/G with -l\r\n"
 " -d  list directory itself\r\n"
 " --help  --version  --  end options\r\n"
 "Example: LS -la B:\\TOOLS\\*.COM\r\n"
 "Attributes: d=dir r=read-only h=hidden\r\n"
 "            s=system a=archive\r\n");
}
void main(void) {
 u8 i,j,endopts=0,quoted,loadflag=*(u8*)0x37;char *s,*q;u16 n;
 handle=1;fn=0x6f;dos();
 if(err || attr<2){
  /* DOS1 has no handle output. */
  rp=(u16)"LS requires MSX-DOS2\r\n$";fn=9;dos();fn=0;dos();return;
 }
 attr=1;sub=4;fn=0x4b;dos();width=(u8)result;if(err||!width)width=80;
 attr=1;sub=0;fn=0x4b;dos();if(!err && !(result&0x80))format=2;
 n=*(u8*)0x80;if(n>127)n=127;
 for(j=0;j<n;++j)tail[j]=((char*)0x81)[j];tail[n]=0;
 /* Snapshot LOAD_FLAG before DOS clears it. COMMAND2's PARAMETERS keeps
  * option case even when UPPER=ON. CP/M launchers must use the PSP tail. */
 if(loadflag){
  rp=(u16)path;rq=(u16)"PARAMETERS";attr=128;fn=0x6b;dos();
  if(err){error("cannot read PARAMETERS (maximum 127 bytes)",0);finish(2);return;}
  if(path[0])copy(tail,path);
 }
 s=tail;
 while(*s){
  while(*s==' '||*s=='\t')++s;if(!*s)break;
  if(argc==32){error("too many arguments",0);finish(2);return;}
  args[argc++]=q=s;quoted=0;
  while(*s){if(*s=='"'){quoted=!quoted;++s;continue;}if(!quoted && (*s==' '||*s=='\t'))break;*q++=*s++;}
  if(quoted){error("unclosed quote",0);finish(2);return;}
  if(*s)++s;*q=0;
 }
 for(i=0;i<argc;++i){
  s=args[i];
  if(!endopts && s[0]=='-' && s[1]){
   if(!cmp(s,"--")){endopts=1;continue;}
   if(!cmp(s,"--help")){help();flush();finish(0);return;}
   if(!cmp(s,"--version")){text("LS 1.0 for MSX-DOS2\r\n");flush();finish(0);return;}
   for(j=1;s[j];++j)switch(s[j]){
    case 'l':format=1;break;case '1':format=2;break;case 'C':format=0;break;case 'x':format=3;break;
    case 'a':all=1;almost=0;break;case 'A':all=1;almost=1;break;
    case 'r':reverse=1;break;case 't':sortby=1;break;case 'S':sortby=2;break;case 'U':sortby=3;break;
    case 'F':case 'p':mark=1;break;case 'd':dirself=1;break;case 'h':human=1;break;
    case 'w':
     if(format!=1)format=0;
     if(s[j+1]>='0' && s[j+1]<='9'){if(!setwidth(s+j+1))goto badwidth;j=length(s)-1;}
     else if(i+1<argc && numeric(args[i+1])){if(!setwidth(args[++i]))goto badwidth;}
     break;
    default:error("unknown option",s);finish(2);return;
   }
  }else operands[npaths++]=s;
 }
 if(!npaths)listing("");
 else for(i=0;i<npaths;++i){if(npaths>1){if(i)nl();text(operands[i]);text(":\r\n");}listing(operands[i]);}
 flush();finish(status);return;
badwidth:error("width must be 1..255",0);finish(2);
}
