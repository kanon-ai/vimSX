/* Bounded compound-command parser. No heap, disk spool or recursive scripts.
 * Parse the whole compound block before any of its commands can execute.
 * Node indices and offsets survive external COMs in the existing snapshot. */
#define BLOCKSIZE 4096
#define NONODE 255
#define ENDWORD 8
#define NSIMPLE 0
#define NIF 1
#define NFOR 2
typedef struct {u8 kind,next,left,right,other;u16 start,end;} Node;
typedef struct {char name[32],values[512];u16 used;} Loop;
static char block[BLOCKSIZE];
static Node nodes[96];
static Loop loops[4];
static u16 blockused,scanpos,wordstart,wordend;
static u8 wordkind,nodecount,parseerror,collecting,controlseen,controlfatal;

static u8 keyword(const char *s){u16 i=wordstart;if(wordkind!=WORD)return 0;while(*s&&i<wordend)if(block[i++]!=*s++)return 0;return !*s&&i==wordend;}
static u8 closing(void){return keyword("then")||keyword("elif")||keyword("else")||keyword("fi")||keyword("do")||keyword("done");}
static void scan(void){u8 c,q=0;while(block[scanpos]==' '||block[scanpos]=='\t'||block[scanpos]=='\r')scanpos++;
 if(block[scanpos]=='#')while(block[scanpos]&&block[scanpos]!='\n')scanpos++;
 wordstart=scanpos;c=block[scanpos];wordkind=ENDWORD;if(!c){wordend=scanpos;return;}
 if(c=='\n'||c==';'){scanpos++;wordkind=SEMI;}
 else if(special(c)){scanpos++;switch(c){case '|':wordkind=PIPE;if(block[scanpos]=='|'){scanpos++;wordkind=OR;}break;case '&':wordkind=AND;if(block[scanpos]!='&')parseerror=1;else scanpos++;break;case '<':wordkind=INPUT;break;case '>':wordkind=OUTPUT;if(block[scanpos]=='>'){scanpos++;wordkind=APPEND;}break;}}
 else{wordkind=WORD;while((c=block[scanpos])){if(!q&&(c==' '||c=='\t'||c=='\r'||c=='\n'||special(c)))break;if(c=='\n'){parseerror=1;break;}if(c=='\\'&&q!='\''&&block[scanpos+1]&&escapable(block[scanpos+1])){scanpos+=2;continue;}if(c=='\''||c=='"'){if(!q)q=c;else if(q==c)q=0;}scanpos++;}if(q)parseerror=1;}
 wordend=scanpos;
}
static u8 newnode(u8 kind){u8 n;if(kind!=NSIMPLE)controlseen=1;if(nodecount==96){parseerror=1;return NONODE;}n=nodecount++;nodes[n].kind=kind;nodes[n].next=nodes[n].left=nodes[n].right=nodes[n].other=NONODE;return n;}
static u8 required(const char *s){if(keyword(s)){scan();return 1;}parseerror=wordkind==ENDWORD?2:1;return 0;}
static u8 segment(u16 a,u16 b){u16 i;if(b-a>=LINE){diagnostic("command or for header exceeds 255 bytes");return 0;}for(i=0;a<b;i++)line[i]=block[a++];line[i]=0;return 1;}
static u8 parselist(u8 depth);
static u8 parseif(u8 depth){u8 n;if(depth>=4){parseerror=1;return NONODE;}n=newnode(NIF);if(n==NONODE)return n;scan();nodes[n].left=parselist(depth+1);if(parseerror||!required("then"))return n;if(nodes[n].left==NONODE){parseerror=1;return n;}
 nodes[n].right=parselist(depth+1);if(parseerror)return n;if(nodes[n].right==NONODE&&wordkind!=ENDWORD){parseerror=1;return n;}
 if(keyword("elif")){nodes[n].other=parseif(depth+1);return n;}
 if(keyword("else")){scan();nodes[n].other=parselist(depth+1);if(parseerror)return n;if(nodes[n].other==NONODE&&wordkind!=ENDWORD){parseerror=1;return n;}}
 required("fi");return n;
}
static u8 parsefor(u8 depth){u8 n;u16 i;if(depth>=4){parseerror=1;return NONODE;}n=newnode(NFOR);if(n==NONODE)return n;scan();nodes[n].start=wordstart;
 if(wordkind==ENDWORD){parseerror=2;return n;}if(wordkind!=WORD||wordend-wordstart>31){parseerror=1;return n;}
 for(i=wordstart;i<wordend;i++)if(!namechar(block[i])||(i==wordstart&&block[i]>='0'&&block[i]<='9')){parseerror=1;return n;}
 scan();if(!required("in"))return n;while(wordkind==WORD&&!parseerror)scan();nodes[n].end=wordstart;
 if(wordkind==ENDWORD){parseerror=2;return n;}if(wordkind!=SEMI){parseerror=1;return n;}while(wordkind==SEMI)scan();if(!required("do"))return n;
 nodes[n].left=parselist(depth+1);if(parseerror)return n;if(nodes[n].left==NONODE&&wordkind!=ENDWORD){parseerror=1;return n;}required("done");return n;
}
static u8 parselist(u8 depth){u8 head=NONODE,tail=NONODE,n;u16 start,end;while(!parseerror){while(wordkind==SEMI)scan();if(parseerror||wordkind==ENDWORD||closing())break;
  if(keyword("if"))n=parseif(depth);else if(keyword("for"))n=parsefor(depth);else{n=newnode(NSIMPLE);if(n==NONODE)break;start=wordstart;end=wordend;while(wordkind!=ENDWORD&&wordkind!=SEMI&&!parseerror){end=wordend;scan();}nodes[n].start=start;nodes[n].end=end;
   if(!parseerror&&(!segment(start,end)||!tokenize()||!checksyntax()))parseerror=1;
  }
  if(parseerror)break;if(tail==NONODE)head=n;else if(tail<96)nodes[tail].next=n;else{parseerror=1;break;}tail=n;
  if(wordkind!=SEMI&&wordkind!=ENDWORD){parseerror=1;break;}
 }return head;
}
static void runlist(u8 n,u8 depth);
static void runfor(u8 n,u8 depth){u8 i;u16 j,offset=0;Loop *v;if(depth>=4){diagnostic("nesting exceeds 4 levels");last=2;return;}v=&loops[depth];v->used=0;
 if(!segment(nodes[n].start,nodes[n].end)||!tokenize()){last=2;return;}copy(v->name,"");for(j=tok[0].start;j<tok[0].end;j++)v->name[j-tok[0].start]=line[j];v->name[j-tok[0].start]=0;
 argused=0;expandstatus=last;for(i=2;i<ntok;i++)if(!expand(i)){last=2;return;}
 if(argused>sizeof(v->values)){diagnostic("for values exceed 512 bytes");last=2;return;}for(j=0;j<argused;j++)v->values[j]=argbuf[j];v->used=argused;last=0;loopcount++;
 while(offset<v->used&&!quit){if(!variable(v->name,v->values+offset,0)){last=2;break;}runlist(nodes[n].left,depth+1);if(flow==1){flow=0;break;}flow=0;while(offset<v->used&&v->values[offset])offset++;offset++;}
 loopcount--;
}
static void runlist(u8 n,u8 depth){while(n!=NONODE&&!quit&&!flow){if(nodes[n].kind==NSIMPLE){if(!segment(nodes[n].start,nodes[n].end)){last=2;return;}execute();}
  else if(nodes[n].kind==NFOR)runfor(n,depth);
  else{runlist(nodes[n].left,depth+1);if(!quit&&!flow){if(!last)runlist(nodes[n].right,depth+1);else if(nodes[n].other!=NONODE)runlist(nodes[n].other,depth+1);else last=0;}}
  n=nodes[n].next;
 }}
/* Returns to the caller while a multi-line if/for is still incomplete. */
static void feed(void){u16 n=len(line),i;u8 root;controlfatal=0;if(blockused+n+2>BLOCKSIZE){diagnostic("compound block exceeds 4095 bytes");last=2;blockused=collecting=0;controlfatal=1;return;}
 for(i=0;i<n;i++)block[blockused++]=line[i];block[blockused++]='\n';block[blockused]=0;scanpos=0;nodecount=parseerror=controlseen=0;scan();root=parselist(0);
 if(!parseerror&&wordkind!=ENDWORD)parseerror=1;
 if(parseerror==2){collecting=1;return;}
 if(parseerror){diagnostic("invalid control syntax, nesting or node limit");last=2;controlfatal=controlseen||collecting;}else runlist(root,0);
 blockused=collecting=0;
}
static void endblock(void){if(collecting){diagnostic("unexpected end: missing then/fi/in/do/done");last=2;}blockused=collecting=0;}
