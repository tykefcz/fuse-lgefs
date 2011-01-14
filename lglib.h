#ifndef LGLIB_H
#define LGLIB_H	1

#include "config.h"
#include <linux/types.h>
#include <sys/types.h>
#include <dirent.h>
#include <unistd.h>
#include "lgfat32.h"
#include "debug.h"
#include "endian.h"
#include "hashtab.h"

typedef struct {
  __u32 sector_size;
  __u32 fat_start_s;
  __u32 fat_size_s;
  __u32 clu1_s;
  __u32 root_dir_s;
  __u32 dir_e_per_s;
  __u32 root_size_s;
  __u32 clustersize;
  __u32 total_sectors;
  __u32 total_clusters;
} sb_t;

typedef struct {
  time_t ctime;
  char trname[128];
  int  parts;
} mmdb_entry;

typedef struct {
  FILE *f;
  int   parts;
  sb_t	sb[2];
  __u32 total_sectors;
  __u32 fat_sector[2];
  __u32 fat_cache[256];
  int mmdb_count;
  ht_tab *mmdb_cache;
} LGEDRV;

struct lgedirent {
  int    d_part;
  __u32	 d_ino;
  __u64	 d_off;
  __u16  d_reclen;
  __u8	 d_type;
  __u64  d_size;
  time_t d_mtime;
  time_t d_ctime;
  mmdb_entry *d_mm;
  char  name[196];
};

typedef struct {
  LGEDRV *d;
  int    part;
  __u32  cluster;
  lge_dir_entry dir_cache[512 / sizeof(lge_dir_entry)];
  int   dir_pos;
} LGDIR;

typedef struct {
  LGEDRV *d;
  int   part;
  __u32	inode;
  __u16 type;
  __u64	offset;
  __u64	size;
  char  name[LGE_NAME * 2];
  __u32 bufc;
  __u32 buflc;
  __u32 bufsize;
  __u8  buf[];
} LGFILE;


void             lge_be16_2_utf8(__be16 *a, char *b, int maxlen, int adddot);
time_t           lge_datetime(__u32 d,__u32 t);
int              lge_findfile(LGEDRV *d, const char *name,struct lgedirent *de);
LGEDRV           *lge_open(const char *devname, int read_mmdb);
LGDIR            *lge_opendir(LGEDRV *d,const char *name);
int               lge_readdir(LGDIR *dirp,struct lgedirent *de);
LGDIR            *lge_closedir(LGDIR *dirp);
LGEDRV           *lge_close(LGEDRV *drv);

LGFILE           *lge_fopen(LGEDRV *d,const char *name);
int              lge_fread(void *buf,int size, LGFILE *f);
off64_t          lge_fseek(LGFILE *f,off64_t o,int mode);
LGFILE           *lge_fclose(LGFILE *f);

#endif
