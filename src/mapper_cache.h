/* Optional 16 KiB, DOS-owned mapper cache for the VRAM backing store.
   Minimum 128 KiB machines use VRAM directly. Extra RAM is not required.
   Page 2 is restored before returning, including the MSX-JE desktop. */
static u16 mapperCacheTags[64];
static u8 mapperCacheDirty[64],mapperCacheScratch[256];
static void transfer(u16 page,u8 *p,u8 write) {
 /* Fold high address bits additively: the document's two gap edges and
    the copy register must not all collide at the same cache index. */
 u8 slot=(page+(page>>6)+(page>>12))&63;
 if(!mapperCount){vramTransfer(page,p,write);return;}
 if(mapperCacheTags[slot]!=page){
  if(mapperCacheDirty[slot]){
   mapperTransfer(slot,mapperCacheScratch,0);
   vramTransfer(mapperCacheTags[slot],mapperCacheScratch,64);
  }
  if(!write){vramTransfer(page,p,0);mapperTransfer(slot,p,64);}
  mapperCacheTags[slot]=page;mapperCacheDirty[slot]=0;
 }else if(!write)mapperTransfer(slot,p,0);
 if(write){mapperTransfer(slot,p,64);mapperCacheDirty[slot]=1;}
}
