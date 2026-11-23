#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#define CHIPS_IMPL
#include "z80.h"
static unsigned char mem[65536];
static z80_t cpu;
static int done,exitcode,mode,columns,found,search_attr,cursor;
static unsigned calls[256];
static char pattern[140];
typedef struct {char path[140];unsigned attr,size,date,time;} E;
static E entries[1200];static int count;
static FILE *stdout_file,*stderr_file;
static const char *original_command;
static void wr16(unsigned p,unsigned n){mem[p]=n;mem[p+1]=n>>8;}
static int match(const char *s,const char *p){
 if(!strcmp(p,"*.*"))p="*";
 if(!*p)return !*s;
 if(*p=='*'){do{if(match(s,p+1))return 1;}while(*s++);return 0;}
 return *s && (*p=='?'||toupper((unsigned char)*p)==toupper((unsigned char)*s)) && match(s+1,p+1);
}
static void find_next(void){
 unsigned p=cpu.ix;
 while(cursor<count){E *e=&entries[cursor++];
  char dir[140],file[140],wantdir[140],wantfile[140];
  strcpy(dir,e->path);char *slash=strrchr(dir,'\\');
  if(slash){strcpy(file,slash+1);slash[1]=0;}else{strcpy(file,dir);dir[0]=0;}
  strcpy(wantdir,pattern);slash=strrchr(wantdir,'\\');
  if(slash){strcpy(wantfile,slash+1);slash[1]=0;}else{strcpy(wantfile,wantdir);wantdir[0]=0;}
  if(strcmp(dir,wantdir)||!match(file,wantfile))continue;
  if((e->attr&0x16)&~search_attr)continue;
  memset(mem+p,0,64);mem[p]=255;strcpy((char*)mem+p+1,file);mem[p+14]=e->attr;
  wr16(p+15,e->time);wr16(p+17,e->date);wr16(p+21,e->size);wr16(p+23,e->size>>16);
  mem[p+25]=1;wr16(p+26,cursor);cpu.a=0;found++;return;
 }
 cpu.a=0xd7;
}
static void bdos(void){
 unsigned fn=cpu.c,n=cpu.hl,a=cpu.de;calls[fn]++;
 mem[0x37]=0;
 switch(fn){
 case 0x6f:cpu.a=0;cpu.bc=(mode&2)?0x100:0x220;cpu.de=0x220;break;
 case 0x4b:assert(cpu.b==1);cpu.de=cpu.a==4?columns:((mode&1)?0:0x82);cpu.a=0;break;
 case 0x6b:
  assert(!(mode&256));assert(!strcmp((char*)mem+cpu.hl,"PARAMETERS"));assert(cpu.b==128);
  if(mode&512){cpu.a=0xbd;break;}
  strcpy((char*)mem+cpu.de,(mode&1024)?"":original_command);cpu.a=0;break;
 case 0x40:
  assert(mem[a]!=255);strcpy(pattern,(char*)mem+a);search_attr=cpu.b;cursor=0;
  assert(!(search_attr&8));
  if(strlen(pattern)>1 && pattern[1]==':'){
   if(toupper(pattern[0])!='A' && toupper(pattern[0])!='B'){cpu.a=0xdb;break;}
   memmove(pattern,pattern+2,strlen(pattern+2)+1);
  }
  if(pattern[0]=='\\')memmove(pattern,pattern+1,strlen(pattern));
  if(!*pattern || pattern[strlen(pattern)-1]=='\\')strcat(pattern,"*.*");
  if(mode&4)cpu.a=0xdb;else find_next();break;
 case 0x41:assert(mem[cpu.ix]==255);cursor=mem[cpu.ix+26]+256*mem[cpu.ix+27];
  if(mode&8)cpu.a=0xf9;else find_next();break;
 case 0x49:
  assert(cpu.b==1||cpu.b==2);assert(a+n<65536);
  if(mode&16){cpu.a=0xd4;cpu.hl=0;break;}
  if(mode&32){cpu.a=0;cpu.hl=n-1;break;}
  fwrite(mem+a,1,n,cpu.b==1?stdout_file:stderr_file);cpu.a=0;break;
 case 9:for(;mem[a]!='$';a++){assert(a<65536);fputc(mem[a],stdout_file);}break;
 case 0:done=1;exitcode=0;break;
 case 0x62:done=1;exitcode=cpu.b;break;
 default:fprintf(stderr,"Forbidden/unexpected BDOS %02x\n",fn);abort();
 }
 // All main registers except documented results can be destroyed by DOS.
 if(fn!=0x6f){cpu.bc=0x9192;if(fn!=0x4b)cpu.de=0x9394;if(fn!=0x49)cpu.hl=0x9596;cpu.f=0xa5;}
}
int main(int argc,char**argv){
 assert(argc==8);mode=atoi(argv[4]);columns=atoi(argv[5]);
 memset(mem,0xcc,sizeof(mem));
 FILE *f=fopen(argv[1],"rb");assert(f);size_t len=fread(mem+256,1,16000,f);fclose(f);assert(len>100);
 f=fopen(argv[2],"r");assert(f);
 while(count<1200 && fscanf(f,"%139s %u %u %u %u",entries[count].path,&entries[count].attr,&entries[count].size,&entries[count].date,&entries[count].time)==5)count++;
 fclose(f);assert(strlen(argv[3])<128);strcpy((char*)mem+0x81,argv[3]);mem[0x80]=strlen(argv[3]);
 original_command=argv[3];mem[0x37]=(mode&(128|512|1024))?1:0;
 if(mode&128)for(unsigned i=0;i<mem[0x80];++i)mem[0x81+i]=toupper(mem[0x81+i]);
 mem[5]=0xc3;wr16(6,(mode&64)?0x8000:0xd000);
 stdout_file=fopen(argv[6],"wb");stderr_file=fopen(argv[7],"wb");assert(stdout_file&&stderr_file);
 uint64_t pins=z80_init(&cpu);cpu.sp=0xcff0;cpu.iff1=cpu.iff2=1;pins=z80_prefetch(&cpu,0x100);
 unsigned long long cycles=0;
 while(!done && cycles++<800000000ULL){
  pins=z80_tick(&cpu,pins);unsigned a=Z80_GET_ADDR(pins);
  if(pins&Z80_MREQ){
   if(pins&Z80_RD){unsigned v=mem[a];
    if((pins&Z80_M1) && (a==5||a==0)){
     if(a==5)bdos();else{done=1;exitcode=0;}v=0xc9;
    }Z80_SET_DATA(pins,v);
   }else if(pins&Z80_WR){assert(a>=0x4000 && a<0xd000);mem[a]=Z80_GET_DATA(pins);}
  }
  assert(!(pins&Z80_IORQ));
 }
 fclose(stdout_file);fclose(stderr_file);
 if(!done){fprintf(stderr,"TIMEOUT pc=%04x cycles=%llu\n",cpu.pc,cycles);return 99;}
 fprintf(stderr,"exit=%d cycles=%llu find=%u next=%u write=%u\n",exitcode,cycles,calls[0x40],calls[0x41],calls[0x49]);
 return exitcode;
}
