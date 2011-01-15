#include "config.h"
#include <asm/byteorder.h> 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <time.h>
#include <linux/types.h>
#include <sys/types.h>
#include <unistd.h>
#include "lglib.h"
#include "lgmme.h"

void dump(void *b,int len) {
  int i;
  unsigned char *c;
  for(c=b,i=0;i<len;c++,i++) {
    printf("%02x",*c);
  }
}

void listfile(char *name) {
  FILE *f;
  mmdb_hdr  hdr;
  mmdb_z    zaz;
  mmdb_file fi;
  mmdb_tail tail;
  unsigned char buf[128],nmb[128];
  int zaznamu,z,i,j;
  time_t tt;
  struct tm tm;
  
  f=fopen(name,"r");
  if (f==NULL) return;
  fread(&hdr,sizeof(hdr),1,f);
  zaznamu=be32_to_cpu(hdr.records);
  for (z=0;z<zaznamu;z++) {
    if (fread(&zaz,sizeof(zaz),1,f) != 1) break;
    lge_be16_2_utf8(&(zaz.jmeno[0]),(char *)buf,sizeof(zaz.jmeno) / sizeof(__be16),0);
    j=be16_to_cpu(zaz.files);
//    dump(&zaz,162);
    tt=lge_datetime(be32_to_cpu(zaz.rec_date),be32_to_cpu(zaz.rec_time));
    localtime_r(&tt,&tm);
    for (i=0;i<j;i++) {
      fread(&fi,sizeof(fi),1,f);
    }
    fread(&tail,sizeof(tail),1,f);
    lge_be16_2_utf8(&(tail.name[0]),(char *)nmb,sizeof(tail.name) / sizeof(__be16),0);
    printf("%04d%02d%02d_%02d%02d_%03d%s_%d_%s = %s\n",
           tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,tm.tm_hour,tm.tm_min,
           be16_to_cpu(zaz.channel),zaz.chan_name,zaz.zanr,buf,nmb);
    
  }
  fclose(f);
  return;
}

int main(int argc, char *argv[]) {
  listfile("MME.DB");
  return 0;
}
