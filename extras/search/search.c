/* MSX-DOS2 fixed-string GREP and recursive FIND. No filesystem mutations. */
typedef unsigned char u8;
typedef unsigned int u16;
typedef unsigned long u32;
#ifdef GREP
#define APP "grep"
#else
#define APP "find"
#endif
static u8 fn,sub,attr,err,status,handle;
static u16 rp,rq,rr,amount,result,outlen;
static char out[256],tail[256],path[128],query[128],apipath[128],pattern[256];
static char *args[32],*roots[32];
static u8 argc,nroots,recursive,maxdepth,typefilter,namefilter;
static u8 frames[32][64],rootfib[64];
static u8 ignorecase,invert,linenum,countonly,listonly,showname,hidename,anymatch;
static u8 fh,chunk[512];
static char line[8192];
static u16 used,patlen;
static u32 lineno,matches;

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
static void finish(u8 c){attr=c;fn=0x62;dos();}
static u16 length(const char *s){u16 n=0;while(s[n])n++;return n;}
static void copy(char *d,const char *s){while((*d++=*s++));}
static u8 eq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static u8 fold(u8 c){return c>='A'&&c<='Z'?c+32:c;}
static void flush(void){u16 n=outlen;if(!n)return;fn=0x49;attr=handle;rp=(u16)out;rq=n;dos();outlen=0;if(err||amount!=n)finish(2);}
static void put(char c){if(outlen==256)flush();out[outlen++]=c;}
static void text(const char *s){while(*s)put(*s++);}
static void nl(void){text("\r\n");}
static void num(u32 n){char s[11];u8 i=0;do{s[i++]='0'+n%10;n/=10;}while(n);while(i)put(s[--i]);}
static void error(const char *s,const char *item){flush();handle=2;text(APP ": ");text(s);if(item){text(": ");text(item);}nl();flush();handle=1;status=2;}
static void derror(const char *p,u8 code){flush();handle=2;text(APP ": DOS error ");num(code);text(": ");text(p);nl();flush();handle=1;status=2;}
static void bad(const char *s){error(s,0);finish(2);}
/* DOS2's root has no dot entry. Keep . in display paths, strip dot
 * components only in the pathname passed to DOS. */
static char *dospath(const char *s){u16 i=0,j=0;while(s[i]){if((!i||s[i-1]=='\\'||s[i-1]==':')&&s[i]=='.'&&(!s[i+1]||s[i+1]=='\\')){i++;if(s[i])i++;}else apipath[j++]=s[i++];}apipath[j]=0;return apipath;}
static u8 first(char *p,u8 *f){rp=(u16)dospath(p);rr=(u16)f;attr=0x37;fn=0x40;dos();return err;}
static u8 next(u8 *f){rr=(u16)f;fn=0x41;dos();return err;}
static u8 dots(const char *s){return s[0]=='.'&&(!s[1]||(s[1]=='.'&&!s[2]));}
static u8 append(const char *s){u16 n=length(path),k=length(s);if(n+k>=sizeof(path)){error("path too long",path);return 0;}copy(path+n,s);return 1;}
static u8 slash(void){u16 n=length(path);if(n&&path[n-1]!='\\'&&path[n-1]!=':')return append("\\");return 1;}
static u8 wildcard(const char *s){while(*s){if(*s=='*'||*s=='?')return 1;s++;}return 0;}
/* -name uses DOS-style case-insensitive * and ?, not regular expressions. */
static u8 glob(const char *p,const char *s){const char *star=0,*retry=0;while(*s){if(*p=='*'){star=++p;retry=s;}else if(*p=='?'||fold(*p)==fold(*s)){p++;s++;}else if(star){p=star;s=++retry;}else return 0;}while(*p=='*')p++;return !*p;}

#ifdef GREP
static u8 contains(void){u16 i,j;if(!patlen)return 1;if(patlen>used)return 0;for(i=0;i<=used-patlen;i++){for(j=0;j<patlen;j++){u8 a=line[i+j],b=pattern[j];if(ignorecase){a=fold(a);b=fold(b);}if(a!=b)break;}if(j==patlen)return 1;}return 0;}
static void prefix(void){if(showname&&!hidename){text(path);put(':');}}
static u8 emitline(void){u16 i;u8 hit;lineno++;if(used&&line[used-1]=='\r')used--;hit=contains();if(invert)hit=!hit;if(hit){anymatch=1;matches++;if(listonly){text(path);nl();return 1;}if(!countonly){prefix();if(linenum){num(lineno);put(':');}for(i=0;i<used;i++)put(line[i]);nl();}}used=0;return 0;}
static void scanfile(u8 stdinfile){u16 i,n;u8 stop=0,e;
 used=0;lineno=matches=0;
 if(stdinfile)fh=0;else{fn=0x43;sub=1;rp=(u16)dospath(path);dos();if(err){derror(path,err);return;}fh=attr;}
 while(!stop){fn=0x48;attr=fh;rp=(u16)chunk;rq=512;dos();e=err;n=amount;
  if(e&&e!=0xc7){derror(path,e);break;}
  if(e==0xc7||!n){if(used)emitline();break;}
  for(i=0;i<n;i++){if(chunk[i]=='\n'){if(emitline()){stop=1;break;}}else{if(used==sizeof(line)){error("line exceeds 8192 bytes",path);stop=1;break;}line[used++]=chunk[i];}}
 }
 if(!stdinfile){fn=0x45;attr=fh;dos();if(err)derror(path,err);}
 if(countonly&&!listonly){prefix();num(matches);nl();}
}
static void visit(u8 dir){if(!dir)scanfile(0);}
#else
static void visit(u8 dir){char *s=path,*p=path;while(*p){if(*p=='\\'||*p==':')s=p+1;p++;}if(!*s)s=path;
 if(typefilter&&typefilter!=(dir?'d':'f'))return;
 if(namefilter&&!glob(pattern,s))return;
 text(path);nl();
}
#endif

/* Each active directory owns a separate DOS search continuation block.
 * Visiting/reading a file never overwrites its parent's search state. */
static void walk(u8 level){u16 base,child;u8 e,dir;u8 *f;
 if(level>=maxdepth)return;
 if(level>=32){error("directory nesting exceeds 32",path);return;}
 base=length(path);if(!slash())return;child=length(path);
 if(!append("*.*")){path[base]=0;return;}copy(query,path);path[child]=0;
 f=frames[level];e=first(query,f);
 while(!e){
  if(!(f[14]&8)&&!dots((char*)f+1)){
   path[child]=0;
   if(append((char*)f+1)){dir=(f[14]&16)!=0;visit(dir);if(dir&&recursive)walk(level+1);}
  }
  e=next(f);
 }
 path[base]=0;if(e!=0xd7)derror(path,e);
}
static void operand(char *s){u16 n,i,base=0;u8 e,dir;
 if(length(s)>=sizeof(path)){error("path too long",s);return;}
 copy(path,s);for(i=0;path[i];i++)if(path[i]=='/')path[i]='\\';n=length(path);
 if(!n){error("empty path",s);return;}
 while(n>1&&path[n-1]=='\\'&&!(n==3&&path[1]==':'))path[--n]=0;
 if(eq(path,".")||eq(path,"..")||path[n-1]=='\\'||path[n-1]==':'){
#ifdef GREP
  if(!recursive){error("directory requires -r",path);return;}
#endif
  if(!slash()||!append("*.*")){path[n]=0;return;}
  copy(query,path);path[n]=0;e=first(query,rootfib);
  if(e&&e!=0xd7){derror(s,e);return;}
  visit(1);walk(0);return;
 }
 for(i=0;i<n;i++)if(path[i]=='\\'||path[i]==':')base=i+1;
 copy(query,path);e=first(query,rootfib);
 if(e){derror(s,e);return;}
 while(!e){
  if(!(rootfib[14]&8)&&!dots((char*)rootfib+1)){
   path[base]=0;if(append((char*)rootfib+1)){
    dir=(rootfib[14]&16)!=0;
#ifdef GREP
    if(dir&&!recursive)error("directory requires -r",path);else
#endif
    {visit(dir);if(dir&&recursive)walk(0);}
   }
  }
  e=next(rootfib);
 }
 if(e!=0xd7)derror(s,e);
}
static void help(void){
#ifdef GREP
 text("GREP 1.0 - fixed strings only, MSX-DOS2\r\n"
 "grep [-FinvclrHh] [-e PATTERN] [--] PATTERN [FILE|DIR ...]\r\n"
 "-r recursive  -n line numbers  -i ASCII ignore-case  -v invert\r\n"
 "-c count lines  -l matching filenames  -H/-h show/hide filenames\r\n"
 "-F fixed (default); no regex. No files: stdin (-r: current directory).\r\n"
 "DOS wildcards allowed in file operands. Maximum line: 8192 bytes.\r\n");
#else
 text("FIND 1.0 - recursive file listing, MSX-DOS2\r\n"
 "find [PATH ...] [-name GLOB] [-type f|d] [-maxdepth N]\r\n"
 "Default PATH is . ; root depth is 0. Hidden/system entries included.\r\n"
 "-name: DOS case-insensitive * and ?; no regex. Conditions are AND.\r\n");
#endif
 flush();finish(0);
}
void main(void){u8 load=*(u8*)0x37,i,j,quoted,endopts=0,havepattern=0;u16 n;char *p,*q,*s;
 handle=1;maxdepth=255;
 fn=0x6f;dos();if(err||attr<2){fn=9;rp=(u16)"Search requires MSX-DOS2\r\n$";dos();return;}
 n=*(u8*)0x80;if(n>127)n=127;for(i=0;i<n;i++)tail[i]=((char*)0x81)[i];tail[n]=0;
 if(load){fn=0x6b;rq=(u16)"PARAMETERS";rp=(u16)tail;attr=255;dos();if(err)bad("command line too long");}
 p=tail;while(*p){while(*p==' '||*p=='\t')p++;if(!*p)break;if(argc==32)bad("too many arguments");args[argc++]=q=p;quoted=0;
  while(*p){if(*p=='"'){quoted=!quoted;p++;continue;}if(!quoted&&(*p==' '||*p=='\t'))break;*q++=*p++;}
  if(quoted)bad("unclosed quote");if(*p)p++;*q=0;
 }
 for(i=0;i<argc;i++){s=args[i];if(!endopts&&eq(s,"--help"))help();if(!endopts&&eq(s,"--")){endopts=1;continue;}
#ifdef GREP
  if(!endopts&&s[0]=='-'&&s[1]){
   for(j=1;s[j];j++)switch(s[j]){
    case 'F':break;case 'i':ignorecase=1;break;case 'n':linenum=1;break;case 'v':invert=1;break;
    case 'c':countonly=1;break;case 'l':listonly=1;break;case 'r':case 'R':recursive=1;break;
    case 'H':showname=1;hidename=0;break;case 'h':hidename=1;break;
    case 'e':if(havepattern)bad("one fixed pattern only");if(s[j+1]){copy(pattern,s+j+1);j=length(s)-1;}else{if(++i==argc)bad("-e needs a pattern");copy(pattern,args[i]);}havepattern=1;break;
    default:bad("unknown option; use --help");
   }
  }else if(!havepattern){copy(pattern,s);havepattern=1;}else roots[nroots++]=s;
#else
  recursive=1;
  if(!endopts&&s[0]=='-'&&s[1]){
   if(eq(s,"-name")){if(++i==argc)bad("-name needs a pattern");copy(pattern,args[i]);namefilter=1;}
   else if(eq(s,"-type")){if(++i==argc||args[i][1]||(args[i][0]!='f'&&args[i][0]!='d'))bad("-type must be f or d");typefilter=args[i][0];}
   else if(eq(s,"-maxdepth")){if(++i==argc||!args[i][0])bad("-maxdepth needs 0..32");n=0;for(p=args[i];*p;p++){if(*p<'0'||*p>'9'||n>32)bad("-maxdepth needs 0..32");n=n*10+*p-'0';}if(n>32)bad("-maxdepth needs 0..32");maxdepth=n;}
   else bad("unknown option; use --help");
  }else roots[nroots++]=s;
#endif
 }
#ifdef GREP
 if(!havepattern)bad("pattern required; use --help");patlen=length(pattern);
 if(nroots>1||recursive)showname=1;for(i=0;i<nroots;i++)if(wildcard(roots[i]))showname=1;
 if(!nroots){if(recursive)operand(".");else{copy(path,"(standard input)");scanfile(1);}}
 else for(i=0;i<nroots;i++){if(eq(roots[i],"-")){copy(path,"(standard input)");scanfile(1);}else operand(roots[i]);}
 flush();finish(status?2:(anymatch?0:1));
#else
 recursive=1;if(!nroots)operand(".");else for(i=0;i<nroots;i++)operand(roots[i]);flush();finish(status);
#endif
}
