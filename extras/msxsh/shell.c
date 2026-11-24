/* MSXSH: original bash-like subset, not GNU Bash. */
typedef unsigned char u8;
typedef unsigned int u16;
#include "resident.inc"
#define LINE 256
#define WORD 0
#define PIPE 1
#define AND 2
#define OR 3
#define SEMI 4
#define INPUT 5
#define OUTPUT 6
#define APPEND 7
#define BYTE(a) (*(volatile u8*)(a))
#define WORD16(a) (*(volatile u16*)(a))
typedef struct {u16 start,end;u8 type;} Token;
static u8 fn,sub,attr,err,outh,ioerror,last,expandstatus,quit,ready,startparse;
static u16 rp,rq,rr,result,amount;
static char line[LINE],argbuf[1024],*argv[32],input[128],output[128];
static u8 argc,appendmode,ntok;
static Token tok[64];
static u16 argused;
static char envbuf[256],pathname[128],program[128],pathlist[256],cmdtail[128];
static char cwd[128],cdpart[64],normal[128],varname[64],numberbuf[12];
static u8 fib[64],header[12];
static char history[8][LINE],draft[LINE];
static u8 hcount,hhead,scriptfh,sbuf[512];
static u16 spos,slen;
static char startup[256];
/* Shell variables survive child execution as part of the mapper snapshot. */
typedef struct {char name[32],value[256];u8 used,exported;} Variable;
static Variable variables[8];
static u8 loopcount,flow;

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
 ld a,(_ready)
 or a
 ld a,(_sub)
 jr z,sh_dos_direct
 call 0xc006
 jr sh_dos_done
sh_dos_direct:
 call 5
sh_dos_done:
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
static void initresident(void) __naked {
 __asm
 call 0xc00c
 ret
 __endasm;
}
static void launch(void) __naked {
 __asm
 call 0xc009
 ret
 __endasm;
}
static void release(void) __naked {
 __asm
 call 0xc00f
 ret
 __endasm;
}
static u16 len(const char *s){u16 n=0;while(s[n])n++;return n;}
static void copy(char *d,const char *s){while((*d++=*s++));}
static u8 eq(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static u8 upper(u8 c){return c>='a'&&c<='z'?c-32:c;}
static u8 equalcase(const char *a,const char *b){while(*a&&upper(*a)==upper(*b)){a++;b++;}return *a==*b;}
static u8 bounded(char *d,const char *s,u16 cap){if(len(s)>=cap)return 0;copy(d,s);return 1;}
static void bytes(const char *s,u16 n){if(!n)return;rp=(u16)s;rq=n;attr=outh;fn=0x49;dos();if(err||amount!=n)ioerror=1;}
static void text(const char *s){bytes(s,len(s));}
static void put(char c){bytes(&c,1);}
static void nl(void){text("\r\n");}
static char *decimal(u16 v){u8 i=11;numberbuf[i]=0;do{numberbuf[--i]='0'+v%10;v/=10;}while(v);return numberbuf+i;}
static void diagnostic(const char *s){u8 old=outh;outh=2;text("msxsh: ");text(s);nl();outh=old;}
static void doserror(const char *s,u8 code){u8 old=outh;outh=2;text("msxsh: ");text(s);text(" (DOS ");text(decimal(code));text(")\r\n");outh=old;}
static void closehandle(u8 h){fn=0x45;attr=h;dos();}
static u8 getenv(const char *name){u8 i;for(i=0;i<8;i++)if(variables[i].used&&eq(name,variables[i].name)){copy(envbuf,variables[i].value);return 1;}fn=0x6b;rq=(u16)name;rp=(u16)envbuf;attr=255;dos();if(err){envbuf[0]=0;return 0;}return 1;}
static u8 setenv(const char *name,const char *value){fn=0x6c;rq=(u16)name;rp=(u16)value;dos();if(err){doserror("environment",err);return 0;}return 1;}
static u8 currentdrive(void){fn=0x19;dos();return err;}
static u8 getcwd(u8 drive){fn=0x59;attr=drive+1;rp=(u16)cdpart;dos();if(err){doserror("current directory",err);return 0;}cwd[0]='A'+drive;cwd[1]=':';cwd[2]='\\';cwd[3]=0;if(cdpart[0]=='\\')copy(cwd+2,cdpart);else copy(cwd+3,cdpart);return 1;}
static u8 fullpath(const char *s,char *dest){u8 drive=currentdrive();u16 n,i,k;const char *p=s;
 if(p[0]&&p[1]==':'){if(upper(p[0])<'A'||upper(p[0])>'H'){diagnostic("drive must be A: through H:");return 0;}drive=upper(p[0])-'A';p+=2;}
 if(*p=='/'||*p=='\\'){normal[0]='A'+drive;normal[1]=':';normal[2]='\\';normal[3]=0;}else{if(!getcwd(drive))return 0;copy(normal,cwd);}n=len(normal);
 while(*p){while(*p=='/'||*p=='\\')p++;if(!*p)break;for(k=0;p[k]&&p[k]!='/'&&p[k]!='\\';k++);
  if(k==1&&p[0]=='.'){}else if(k==2&&p[0]=='.'&&p[1]=='.'){while(n>3&&normal[n-1]!='\\')n--;if(n>3)n--;normal[n]=0;}
  else{if(n>3&&normal[n-1]!='\\'){if(n+1>=128)return 0;normal[n++]='\\';}if(n+k>=128){diagnostic("path too long");return 0;}for(i=0;i<k;i++)normal[n++]=p[i];normal[n]=0;}p+=k;
 }copy(dest,normal);return 1;
}
static u8 namechar(u8 c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||c=='_'||(c>='0'&&c<='9');}
static u8 validname(const char *s){if(!*s||(*s>='0'&&*s<='9'))return 0;while(*s)if(!namechar(*s++))return 0;return 1;}
static u8 variable(const char *name,const char *value,u8 exporting){u8 i,slot=255;Variable *v;
 if(!validname(name)||len(name)>31||len(value)>255){diagnostic("variable limit: name 31, value 255 bytes");return 0;}
 for(i=0;i<8;i++){if(variables[i].used&&eq(name,variables[i].name)){slot=i;break;}if(!variables[i].used&&slot==255)slot=i;}
 if(slot==255){diagnostic("at most 8 shell variables");return 0;}v=&variables[slot];exporting|=v->used?v->exported:0;
 /* An assignment to an inherited environment variable stays exported. */
 if(!v->used&&!exporting){fn=0x6b;rq=(u16)name;rp=(u16)envbuf;attr=255;dos();if(!err&&envbuf[0])exporting=1;}
 if(exporting&&!setenv(name,value))return 0;copy(v->name,name);copy(v->value,value);v->used=1;v->exported=exporting;return 1;
}
static u8 number(const char *s,int *out){u16 n=0;u8 neg=0;if(*s=='-'||*s=='+')neg=*s++=='-';if(!*s)return 0;while(*s){if(*s<'0'||*s>'9'||n>3276)return 0;n=n*10+*s++-'0';if(n>(neg?32768u:32767u))return 0;}*out=neg?(int)(0u-n):(int)n;return 1;}
static u8 testcmd(void){u8 n=argc-1,off=1,invert=0,ok=0;int x,y;char *a,*op,*b;
 if(eq(argv[0],"[")){if(!n||!eq(argv[argc-1],"]")){diagnostic("[ requires closing ]");return 2;}n--;}
 if(n&&eq(argv[off],"!")){invert=1;off++;n--;}
 if(!n)ok=0;else if(n==1)ok=argv[off][0]!=0;
 else if(n==2){op=argv[off];a=argv[off+1];if(eq(op,"-n"))ok=*a!=0;else if(eq(op,"-z"))ok=*a==0;
  else if(eq(op,"-e")||eq(op,"-f")||eq(op,"-d")){if(!fullpath(a,pathname))return 2;if(len(pathname)==3){fn=0x59;attr=upper(pathname[0])-'A'+1;rp=(u16)cdpart;dos();ok=!err&&!eq(op,"-f");}else{fn=0x40;rp=(u16)pathname;rr=(u16)fib;attr=0x37;dos();ok=!err;if(ok){if(eq(op,"-f"))ok=!(fib[14]&16);if(eq(op,"-d"))ok=(fib[14]&16)!=0;}}}
  else goto badtest;
 }else if(n==3){a=argv[off];op=argv[off+1];b=argv[off+2];if(eq(op,"=")||eq(op,"=="))ok=eq(a,b);else if(eq(op,"!="))ok=!eq(a,b);else{if(!number(a,&x)||!number(b,&y))goto badtest;if(eq(op,"-eq"))ok=x==y;else if(eq(op,"-ne"))ok=x!=y;else if(eq(op,"-lt"))ok=x<y;else if(eq(op,"-le"))ok=x<=y;else if(eq(op,"-gt"))ok=x>y;else if(eq(op,"-ge"))ok=x>=y;else goto badtest;}}
 else goto badtest;return (ok!=invert)?0:1;
badtest:diagnostic("unsupported test expression or integer out of range");return 2;
}
static u8 special(u8 c){return c=='|'||c=='&'||c==';'||c=='<'||c=='>';}
static u8 escapable(u8 c){return c==' '||c=='\t'||c=='$'||c=='"'||c=='\''||c=='\\'||special(c);}
static u8 tokenize(void){u16 i=0,start;u8 quote=0,t;ntok=0;while(line[i]){while(line[i]==' '||line[i]=='\t')i++;if(!line[i]||line[i]=='#')break;if(ntok==64){diagnostic("too many tokens");return 0;}start=i;
  if(special(line[i])){t=0;switch(line[i++]){case '|':t=PIPE;if(line[i]=='|'){i++;t=OR;}break;case '&':if(line[i]!='&'){diagnostic("background jobs unsupported");return 0;}i++;t=AND;break;case ';':t=SEMI;break;case '<':t=INPUT;break;case '>':t=OUTPUT;if(line[i]=='>'){i++;t=APPEND;}break;}}
  else{t=WORD;quote=0;while(line[i]){u8 c=line[i];if(!quote&&(c==' '||c=='\t'||special(c)))break;if(c=='\\'&&quote!='\''&&line[i+1]&&escapable(line[i+1])){i+=2;continue;}if(c=='\''||c=='"'){if(!quote)quote=c;else if(quote==c)quote=0;}i++;}if(quote){diagnostic("unclosed quote");return 0;}}
  tok[ntok].start=start;tok[ntok].end=i;tok[ntok++].type=t;}return 1;
}
static u8 addchar(char c){if(argused>=sizeof(argbuf)-1){diagnostic("expanded arguments too long");return 0;}argbuf[argused++]=c;return 1;}
static u8 expand(u8 ti){u16 i=tok[ti].start,end=tok[ti].end,k;u8 q=0,brace;char c;while(i<end){c=line[i++];
  if(c=='\\'&&q!='\''&&i<end&&escapable(line[i])){if(!addchar(line[i++]))return 0;continue;}
  if(c=='\''||c=='"'){if(!q){q=c;continue;}if(q==c){q=0;continue;}}
  if(c=='$'&&q!='\''&&i<end&&!startparse){if(line[i]=='?'){i++;copy(envbuf,decimal(expandstatus));}else{brace=line[i]=='{';if(brace)i++;k=0;while(i<end&&namechar(line[i])){if(k>=63){diagnostic("variable name too long");return 0;}varname[k++]=line[i++];}varname[k]=0;if(brace){if(i==end||line[i]!='}'){diagnostic("bad variable expansion");return 0;}i++;}if(!k){if(brace||line[i]=='('){diagnostic("unsupported variable/substitution syntax");return 0;}if(!addchar('$'))return 0;continue;}getenv(varname);}for(k=0;envbuf[k];k++)if(!addchar(envbuf[k]))return 0;
  }else{if(c=='`'&&q!='\''&&!startparse){diagnostic("command substitution unsupported");return 0;}if(!addchar(c))return 0;}}
 return addchar(0);
}
static u8 prepare(u8 a,u8 b){u8 i,t;u16 start;argc=0;argused=0;input[0]=output[0]=0;appendmode=0;for(i=a;i<b;i++){t=tok[i].type;if(t==WORD){if(argc==32){diagnostic("too many arguments");return 0;}argv[argc++]=argbuf+argused;if(!expand(i))return 0;}else if(t>=INPUT){if(++i==b||tok[i].type!=WORD){diagnostic("redirection needs a filename");return 0;}start=argused;if(!expand(i))return 0;if(!argbuf[start]){diagnostic("empty redirection filename");return 0;}if(!bounded(t==INPUT?input:output,argbuf+start,128)){diagnostic("path too long");return 0;}if(t!=INPUT)appendmode=t==APPEND;}else{diagnostic("unexpected operator");return 0;}}if(!argc){diagnostic("command required");return 0;}return 1;}
static u8 checksyntax(void){u8 i,command=0,needword=0;for(i=0;i<ntok;i++){u8 t=tok[i].type;if(needword){if(t!=WORD){diagnostic("redirection needs filename");return 0;}needword=0;continue;}if(t==WORD)command=1;else if(t>=INPUT)needword=1;else{if(!command){diagnostic("empty command");return 0;}command=0;if(i+1==ntok&&t!=SEMI){diagnostic("trailing operator");return 0;}}}if(needword||(!command&&ntok&&tok[ntok-1].type>=INPUT)){diagnostic("incomplete command");return 0;}return 1;}
static u8 findfile(const char *s){fn=0x40;rp=(u16)s;rr=(u16)fib;attr=0x27;dos();return !err&&!(fib[14]&16);}
static u8 candidate(const char *dir,const char *cmd){u16 n=len(dir),k=len(cmd);u8 dot=0;const char *p;if(n+k+6>=128)return 0;copy(pathname,dir);if(n&&pathname[n-1]!=':'&&pathname[n-1]!='\\'&&pathname[n-1]!='/')pathname[n++]='\\';copy(pathname+n,cmd);p=cmd;while(*p){if(*p==':'||*p=='/'||*p=='\\')dot=0;else if(*p=='.')dot=1;p++;}if(!dot)copy(pathname+len(pathname),".COM");if(!fullpath(pathname,program))return 0;return findfile(program);}
static u8 locate(void){u16 i,start,n;u8 explicitpath=0;for(i=0;argv[0][i];i++)if(argv[0][i]=='/'||argv[0][i]=='\\'||argv[0][i]==':')explicitpath=1;if(candidate("",argv[0]))return 1;if(explicitpath)return 0;getenv("PATH");copy(pathlist,envbuf);i=0;while(pathlist[i]){start=i;while(pathlist[i]&&pathlist[i]!=';')i++;n=i-start;if(n<128){char saved=pathlist[i];pathlist[i]=0;if(candidate(pathlist+start,argv[0]))return 1;pathlist[i]=saved;}if(pathlist[i])i++;}return 0;}
static u8 maketail(void){u8 i,quote;u16 j,n=0;for(i=1;i<argc;i++){quote=!argv[i][0];for(j=0;argv[i][j];j++){if(argv[i][j]=='"'){diagnostic("literal double quotes in external arguments unsupported");return 0;}if(argv[i][j]==' '||argv[i][j]=='\t')quote=1;}if(n+len(argv[i])+1+(quote?2:0)>126){diagnostic("external tail exceeds 126 bytes");return 0;}cmdtail[n++]=' ';if(quote)cmdtail[n++]='"';for(j=0;argv[i][j];j++)cmdtail[n++]=argv[i][j];if(quote)cmdtail[n++]='"';}cmdtail[n]=0;return 1;}
static u8 external(void){u8 parent,fh,code;u16 size,i;
 if(!locate()){diagnostic("command not found");return 127;}i=len(program);if(i<4||!equalcase(program+i-4,".COM")){diagnostic("external programs must be .COM");return 126;}if(!maketail())return 2;
 fn=0x60;dos();if(err){doserror("fork",err);return 2;}parent=attr;fn=0x43;sub=1;rp=(u16)program;dos();if(err){code=err;goto failure;}fh=attr;
 fn=0x4a;sub=2;attr=fh;rp=rq=0;dos();if(err){code=err;goto failure;}size=amount;if(result||!size||size>0xbe00){diagnostic("COM empty/too large (maximum 48640 bytes)");code=126;goto join;}
 fn=0x4a;sub=0;attr=fh;rp=rq=0;dos();if(err){code=err;goto failure;}fn=0x48;attr=fh;rp=(u16)header;rq=11;dos();if(err){code=err;goto failure;}if(amount==11){header[11]=0;if(eq((char*)header+3,"MSXSH001")){diagnostic("nested MSXSH unsupported");code=126;goto join;}}
 fn=0x4a;sub=0;attr=fh;rp=rq=0;dos();if(err){code=err;goto failure;}if(!setenv("PARAMETERS",cmdtail)||!setenv("PROGRAM",program)){code=2;goto join;}
 BYTE(0xc601)=fh;WORD16(0xc602)=size;BYTE(0xc604)=parent;BYTE(0xc605)=0;BYTE(0xc700)=len(cmdtail);for(i=0;i<127;i++)BYTE(0xc701+i)=i<=len(cmdtail)?cmdtail[i]:0;
 launch();return BYTE(0xc605);
failure:doserror("load COM",code);code=126;
join:fn=0x61;attr=parent;dos();if(err){doserror("join",err);return 2;}return code;
}
static u8 builtin(void){u8 i,j;u16 n;char *p;
 if(eq(argv[0],"test")||eq(argv[0],"["))return testcmd();
 if(eq(argv[0],"break")||eq(argv[0],"continue")){if(argc!=1||!loopcount){diagnostic("break/continue require a for loop and no arguments");return 2;}flow=eq(argv[0],"break")?1:2;return 0;}
 /* Only assignment-only commands; command-prefix assignments are rejected. */
 for(n=0;argv[0][n]&&argv[0][n]!='=';n++);
 if(argv[0][n]=='='){for(i=0;i<argc;i++){p=argv[i];for(n=0;p[n]&&p[n]!='=';n++);if(!p[n]){diagnostic("command-prefix assignments unsupported");return 2;}p[n]=0;if(!validname(p)){diagnostic("invalid variable name");return 2;}p[n]='=';}for(i=0;i<argc;i++){p=argv[i];for(n=0;p[n]!='=';n++);p[n]=0;if(!variable(p,p+n+1,0))return 2;}return 0;}
 if(eq(argv[0],"true"))return 0;if(eq(argv[0],"false"))return 1;
 if(eq(argv[0],"echo")){i=1;j=0;if(argc>1&&eq(argv[1],"-n")){i=2;j=1;}for(;i<argc;i++){if(i>(j?2:1))put(' ');text(argv[i]);}if(!j)nl();return 0;}
 if(eq(argv[0],"pwd")){if(!getcwd(currentdrive()))return 2;for(i=0;cwd[i];i++)put(cwd[i]=='\\'?'/':cwd[i]);nl();return 0;}
 if(eq(argv[0],"cd")){if(argc>2){diagnostic("cd takes one directory");return 2;}if(argc==1){getenv("HOME");p=envbuf[0]?envbuf:"/";}else p=argv[1];if(!fullpath(p,pathname))return 2;fn=0x5a;rp=(u16)pathname;dos();if(err){doserror("cd",err);return 2;}fn=0x0e;rp=upper(pathname[0])-'A';dos();return 0;}
 if(eq(argv[0],"export")){if(argc<2){diagnostic("usage: export NAME[=value] ...");return 2;}for(i=1;i<argc;i++){p=argv[i];for(n=0;p[n]&&p[n]!='=';n++);if(p[n]){p[n]=0;if(!variable(p,p+n+1,1))return 2;}else{getenv(p);copy(startup,envbuf);if(!variable(p,startup,1))return 2;}}return 0;}
 if(eq(argv[0],"unset")){for(i=1;i<argc;i++){if(!validname(argv[i])){diagnostic("invalid variable name");return 2;}if(!setenv(argv[i],""))return 2;for(j=0;j<8;j++)if(variables[j].used&&eq(variables[j].name,argv[i]))variables[j].used=0;}return 0;}
 if(eq(argv[0],"exit")){if(argc>2){diagnostic("exit takes one status");return 2;}n=last;if(argc==2){n=0;p=argv[1];if(!*p){diagnostic("exit requires 0..255");return 2;}while(*p){if(*p<'0'||*p>'9'||n>255){diagnostic("exit requires 0..255");return 2;}n=n*10+*p++-'0';}if(n>255){diagnostic("exit requires 0..255");return 2;}}quit=1;return n;}
 if(eq(argv[0],"history")){for(i=0;i<hcount;i++){j=(hhead+8-hcount+i)%8;text(decimal(i+1));put(' ');text(history[j]);nl();}return 0;}
 if(eq(argv[0],"help")){text("MSXSH 0.2: bash-like subset, not GNU Bash\r\nBuiltins: cd pwd echo export unset true false history exit help\r\nSyntax: ; && || | < > >>  quotes  $NAME ${NAME} $?\r\nCOM: current directory then DOS PATH (semicolon separated).\r\nRAM pipes: 4096 bytes/stage, sequential, DOS2 handles 0/1 only.\r\nControl: if/elif/else/fi, for/in/do/done, test/[ ], break/continue.\r\nNAME=value; export NAME; unset NAME. No jobs/functions/substitutions.\r\n");return 0;}return external();
}
static u8 restorehandles(u8 ib,u8 ob){u8 failed=0;if(ib!=255){closehandle(0);if(err&&err!=0xc2)failed=1;fn=0x47;attr=ib;dos();if(err||attr!=0)failed=1;closehandle(ib);}if(ob!=255){closehandle(1);if(err&&err!=0xc2)failed=1;fn=0x47;attr=ob;dos();if(err||attr!=1)failed=1;closehandle(ob);}return failed;}
static u8 command(u8 pin,u8 pout){u8 ib=255,ob=255,code=2,h;u16 k;ioerror=0;BYTE(0xc608)=0;BYTE(0xc624)=BYTE(0xc625)=0;
 if((pin!=255||pout!=255)&&(eq(argv[0],"cd")||eq(argv[0],"export")||eq(argv[0],"unset")||eq(argv[0],"exit")||eq(argv[0],"break")||eq(argv[0],"continue"))){diagnostic("state-changing builtins unsupported in pipelines");BYTE(0xc625)=1;return 2;}
 if(pin!=255||pout!=255){for(k=0;argv[0][k]&&argv[0][k]!='=';k++);if(argv[0][k]){diagnostic("assignment unsupported in pipelines");BYTE(0xc625)=1;return 2;}}
 if(input[0]){if(!fullpath(input,pathname))goto done;fn=0x47;attr=0;dos();if(err){doserror("duplicate stdin",err);goto done;}ib=attr;closehandle(0);fn=0x43;sub=5;rp=(u16)pathname;dos();if(err){doserror("open input",err);goto done;}if(attr!=0){diagnostic("stdin handle mismatch");closehandle(attr);goto done;}}
 if(output[0]){if(!fullpath(output,pathname))goto done;fn=0x47;attr=1;dos();if(err){doserror("duplicate stdout",err);goto done;}ob=attr;closehandle(1);
  if(appendmode){fn=0x43;sub=6;rp=(u16)pathname;dos();if(err==0xd7){fn=0x44;sub=6;attr=0x80;rp=(u16)pathname;dos();}}else{fn=0x44;sub=6;attr=0;rp=(u16)pathname;dos();}
  if(err){doserror("open output",err);goto done;}h=attr;if(h!=1){diagnostic("stdout handle mismatch");closehandle(h);goto done;}if(appendmode){fn=0x4a;sub=2;attr=1;rp=rq=0;dos();if(err){doserror("append seek",err);goto done;}}
 }
 BYTE(0xc609)=pin;BYTE(0xc60a)=pout;WORD16(0xc610)=0;BYTE(0xc608)=(pin!=255&&!input[0]?1:0)|(pout!=255&&!output[0]?2:0);
 code=builtin();if(ioerror)code=2;
done:BYTE(0xc608)=0;
 if(restorehandles(ib,ob)){diagnostic("cannot restore standard handles; exiting");quit=1;code=2;}
 if(BYTE(0xc624)){diagnostic("RAM pipe exceeds 4096 bytes; pipeline stopped");code=2;}if(BYTE(0xc625)){diagnostic("unsupported DOS call on RAM pipe; pipeline stopped");code=2;}return code;
}
static void execute(void){u8 a=0,b,c,d,link=SEMI,run,prev=255,nextpipe=0,pout;
 if(!tokenize()||!checksyntax()){last=2;return;}
 while(a<ntok&&!quit&&!flow){b=a;while(b<ntok&&tok[b].type!=SEMI&&tok[b].type!=AND&&tok[b].type!=OR)b++;run=link==SEMI||(link==AND&&!last)||(link==OR&&last);
  if(run){expandstatus=last;prev=255;nextpipe=0;c=a;while(c<b&&!quit&&!flow){d=c;while(d<b&&tok[d].type!=PIPE)d++;pout=d<b?nextpipe:255;if(pout!=255)WORD16(0xc60c+2*pout)=0;
    if(!prepare(c,d)){last=2;break;}last=command(prev,pout);if(BYTE(0xc624)||BYTE(0xc625))break;prev=pout;nextpipe^=1;c=d+1;}}
  if(b==ntok)break;link=tok[b].type;a=b+1;
 }
}
#include "control.h"

static void left(u16 n){while(n--)text("\033D");}
static void right(u16 n){while(n--)text("\033C");}
static void prompt(void){u16 i;if(collecting){text("> ");return;}if(getcwd(currentdrive()))for(i=0;cwd[i];i++)put(cwd[i]=='\\'?'/':cwd[i]);text("$ ");}
static u8 readline(void){u16 n=0,pos=0,i;u8 c,hoff=0,idx;line[0]=0;prompt();for(;;){fn=6;rp=255;dos();c=err;if(!c)continue;
  if(c==13){nl();line[n]=0;return 1;}if(c==3){text("^C\r\n");last=130;blockused=collecting=0;line[0]=0;return 1;}if(c==4&&!n){nl();return 0;}
  if(c==30||c==31){if(!hcount)continue;if(!hoff)copy(draft,line);if(c==30){if(hoff<hcount)hoff++;}else if(hoff)hoff--;left(pos);text("\033K");if(hoff){idx=(hhead+8-hoff)%8;copy(line,history[idx]);}else copy(line,draft);n=pos=len(line);text(line);continue;}
  if(c==29||c==2){if(pos){left(1);pos--;}continue;}if(c==28||c==6){if(pos<n){right(1);pos++;}continue;}if(c==1){left(pos);pos=0;continue;}if(c==5){right(n-pos);pos=n;continue;}
  if(c==8){if(!pos)continue;left(1);pos--;for(i=pos;i<n;i++)line[i]=line[i+1];n--;text(line+pos);put(' ');left(n-pos+1);continue;}if(c==127||c==4){if(pos==n)continue;for(i=pos;i<n;i++)line[i]=line[i+1];n--;text(line+pos);put(' ');left(n-pos+1);continue;}
  if(c<32)continue;if(n==255){put(7);continue;}for(i=n;i>pos;i--)line[i]=line[i-1];line[pos]=c;line[++n]=0;text(line+pos);pos++;left(n-pos);
 }}
static void remember(void){if(!line[0])return;if(hcount&&eq(history[(hhead+7)%8],line))return;copy(history[hhead],line);hhead=(hhead+1)%8;if(hcount<8)hcount++;}
static u8 scriptline(void){u16 n=0;u8 c;for(;;){if(spos==slen){fn=0x48;attr=scriptfh;rp=(u16)sbuf;rq=512;dos();if(err&&err!=0xc7){doserror("read script",err);last=2;return 0;}slen=err?0:amount;spos=0;if(!slen){line[n]=0;return n!=0;}}c=sbuf[spos++];if(!c){diagnostic("NUL in script");last=2;return 0;}if(c=='\r')continue;if(c=='\n'){line[n]=0;return 1;}if(n==255){diagnostic("script line exceeds 255 bytes");last=2;return 0;}line[n++]=c;}}
void main(void){u16 i,n;u8 load=BYTE(0x37),mode=0;outh=1;fn=0x6f;dos();if(err||attr<2){fn=9;rp=(u16)"MSXSH requires MSX-DOS2\r\n$";dos();return;}
 n=BYTE(0x80);if(n>127)n=127;for(i=0;i<n;i++)startup[i]=BYTE(0x81+i);startup[n]=0;if(load){getenv("PARAMETERS");copy(startup,envbuf);}
 for(i=0;i<sizeof(resident_image);i++)BYTE(0xc000+i)=resident_image[i];for(i=0xc600;i<0xc680;i++)BYTE(i)=0;WORD16(0xc612)=WORD16(6);initresident();
 if(BYTE(0xc600)){diagnostic("need four free primary mapper segments (64KB)");last=2;goto finish;}ready=1;copy(line,startup);expandstatus=0;startparse=1;
 if(!tokenize()){last=2;goto finish;}if(ntok){if(!prepare(0,ntok)){last=2;goto finish;}if(eq(argv[0],"--help")){text("MSXSH 0.2: bash-like subset, not GNU Bash\r\nUsage: MSXSH [-c \"commands\" | SCRIPT.SH]\r\n");goto finish;}if(eq(argv[0],"-c")){if(argc!=2){diagnostic("-c needs one quoted command string");last=2;goto finish;}copy(startup,argv[1]);mode=1;}else{if(argc!=1){diagnostic("one script filename only");last=2;goto finish;}if(!fullpath(argv[0],pathname)){last=2;goto finish;}fn=0x43;sub=1;rp=(u16)pathname;dos();if(err){doserror("open script",err);last=2;goto finish;}scriptfh=attr;mode=2;}}
 startparse=0;if(mode==1){copy(line,startup);feed();endblock();}else if(mode==2){while(!quit&&scriptline()){feed();if(controlfatal)break;}endblock();closehandle(scriptfh);if(err){doserror("close script",err);last=2;}}else{text("MSXSH 0.2 - experimental bash-like shell; help for limits\r\n");while(!quit&&readline()){remember();feed();}endblock();}
finish:BYTE(0xc608)=0;if(ready)release();ready=0;attr=last;fn=0x62;dos();
}


