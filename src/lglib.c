#include "config.h"
#include <asm/byteorder.h> 
#include <stdlib.h>
#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64	1
#endif
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <dirent.h>
#include <time.h>
#include "lglib.h"
#include "lgmme.h"
#include "hashtab.h"

#ifdef DEBUG
#include <stdarg.h>
void debug_printf(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr,fmt, ap);
  va_end(ap);
}
#endif 

void lge_be16_2_utf8(__be16 *a, char *b, int max,int adddot) {
  int i,j;
  unsigned int ch;
  for (i=0,j=0;i<max ;i++) {
    if (a[i] != 0) {
      ch=be16_to_cpu(a[i]);
      if (i==adddot && j && b[j-1] != '.') b[j++]='.';
      if (ch <= 0x7F) {
        b[j++]=(ch & 0x7F);
      } else {
        if (ch <= 0x800) {
          b[j++]=((ch >> 6) & 0x1F) | 0xC0; // 0x7FF = 110????? 10??????
          b[j++]=(ch & 0x3F) | 0x80;
        } else {
          b[j++]=((ch >> 12) & 0x0F) | 0xE0; // 0xFFFF 1110???? 10?????? 10??????
          b[j++]=((ch >> 6) & 0x3F) | 0x80;
          b[j++]=(ch & 0x3F) | 0x80;
        }
      }
    } else {
      if (adddot!=0 && i<adddot) i=adddot - 1;
      else break;
    }
  }
  b[j]=0;
}

time_t lge_datetime(__u32 d,__u32 t) {
  struct tm tm;
  time_t rv;
  memset(&tm,0,sizeof(struct tm));
  tm.tm_year = (d >> 16) - 1900;
  tm.tm_mon = ((d & 0x0FF00) >> 8) - 1;
  tm.tm_mday = d & 0x0FF;
  tm.tm_hour = (t >> 24) & 0x1F;
  tm.tm_min = (t >> 16) & 0x03F;
  tm.tm_sec = (t >> 8) & 0x03F;
  rv = mktime(&tm);
  if ((t & 0x0FFFFU) == 0x3beeU) rv++;
  return rv;
}

static void *sector_read(void *buf,LGEDRV *d,__u32 sector) {
  off64_t o=d->sb[0].sector_size; 
  if (d==NULL || d->f == NULL || sector > d->total_sectors) return NULL;
  o*=sector;
  fseeko64(d->f,o,SEEK_SET);
  debug(("reading sector on offset 0x%llx\n",o));
  if (fread(buf,d->sb[0].sector_size,1,d->f) == 1) return buf;
  return NULL;
}
static void *cluster_read(void *buf,LGEDRV *d,int part,__u32 cluster, int firstsector, int sectorcount) {
  off64_t o;
  sb_t *sb;
  if (d==NULL || part<0 || part>=d->parts) return NULL;
  sb=&(d->sb[part]);
  if (d->f == NULL || cluster==0 || cluster >= sb->total_clusters) return NULL;
  o=(cluster - 1) * sb->clustersize + sb->clu1_s + firstsector;
  o=o * sb->sector_size;
  fseeko64(d->f,o,SEEK_SET);
  if (firstsector < 0) firstsector = 0;
  if (sectorcount == 0 ) sectorcount=1;
  if (sectorcount < 0 || sectorcount > (sb->clustersize - firstsector)) 
    sectorcount=sb->clustersize - firstsector;
//  debug(("reading cluster %u/%u on offs 0x%llx\n",cluster,firstsector,o));
  if (fread(buf,sb->sector_size,sectorcount,d->f) == sectorcount) return buf;
  return NULL;
}

static int lge_fat_read(LGEDRV *d,int part, __u32 cluster) {
  __u32 clustersector;
  sb_t *sb;
  sb=&(d->sb[part]);
  if (cluster==0 || cluster >= sb->total_clusters) {
    debug(("lge_fat_read:Invalid drive %p or cluster %lu >= %lu\n",d,cluster,
          (d!=NULL ? sb->total_clusters : 0)));
    return -1;
  }
  clustersector = cluster / (sb->sector_size / 4);
  if (clustersector > sb->fat_size_s) return -1;
  if (d->fat_sector[part] != clustersector) {
    debug(("read fat_cache%d for cluster %lu.\n",part,cluster));
    if (sector_read(&(d->fat_cache[part * 128]),d,clustersector + sb->fat_start_s) != NULL)
      d->fat_sector[part] = clustersector;
    else
      return -1;
  }
  return cluster % (sb->sector_size / 4);
}

static __u32 next_cluster(LGEDRV *d, int part, __u32 c) {
  int i;
  __u32 rv;
  rv = c / (d->sb[part].sector_size / 4);
  if (rv > d->sb[part].fat_size_s) return 0;
  if (d->fat_sector[part] != rv) i=lge_fat_read(d,part,c);
  else i=c % (d->sb[part].sector_size / 4);
  if (i<0) return 0;
  rv=d->fat_cache[(part*128)+i];
  debug(("next_cluster%d %ud->%ud\n",part,c,rv));
  if (rv < (__u32)(MAX_FAT)) return rv;
  return 0;
}

static void fillcache(LGEDRV *d) {
  LGFILE *f=NULL;
  mmdb_hdr  hdr;
  mmdb_z    zaz;
  mmdb_file fi;
  mmdb_tail tail;
  mmdb_entry e;
  unsigned char buf[128],nmb[128];
  int zaznamu,z,i,j,k;
  time_t tt;
  struct tm tm;
  
  d->mmdb_count=0;
  if (d->mmdb_cache != NULL) ht_free(d->mmdb_cache);
  d->mmdb_cache=NULL;
  f=lge_fopen(d,"/B/MME.DB");
  if (f==NULL) return;
  if (lge_fread(&hdr,sizeof(hdr),f) != sizeof(hdr)) return;
  zaznamu=be32_to_cpu(hdr.records);
  if (zaznamu > 2048 || zaznamu <= 0) goto errend;
  d->mmdb_cache=ht_new(NULL);
  if (d->mmdb_cache==NULL) goto errend;
  for (z=0;z<zaznamu;z++) {
    if (lge_fread(&zaz,sizeof(zaz),f) != sizeof(zaz)) break;
    lge_be16_2_utf8(&(zaz.jmeno[0]),(char *)buf,sizeof(zaz.jmeno) / sizeof(__be16),0);
    j=be16_to_cpu(zaz.files);
    tt=lge_datetime(be32_to_cpu(zaz.rec_date),be32_to_cpu(zaz.rec_time));
    localtime_r(&tt,&tm);
    for (i=0;i<j;i++) {
      if (lge_fread(&fi,sizeof(fi),f) != sizeof(fi)) goto errend;
    }
    if (lge_fread(&tail,sizeof(tail),f) != sizeof(tail)) goto errend;
    lge_be16_2_utf8(&(tail.name[0]),(char *)nmb,sizeof(tail.name) / sizeof(__be16),0);
    if (strncmp((char *)nmb,"A:\\0000",7)==0) {
      for(k=0;buf[k] && k < 128;k++) {
        if (buf[k]=='"' || buf[k]==':' || buf[k]=='\\' || 
            buf[k]=='/' || buf[k]=='*' || buf[k]=='?'  || 
            buf[k]<=32U ) 
          buf[k]='_';
      }
      k=strlen((char*)nmb)-4;
      if (k>=0 && strcmp((char *)&(nmb[k]),".str")==0) nmb[k]=0;
      e.ctime=tt;
      e.parts=j;
      snprintf(e.trname,sizeof(e.trname),
             "%04d%02d%02d_%02d%02d_%03d%s_%d_%s",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,tm.tm_hour,tm.tm_min,
             be16_to_cpu(zaz.channel),zaz.chan_name,zaz.zanr,buf);
      ht_adddup(d->mmdb_cache,(char *)&(nmb[7]),&e,sizeof(mmdb_entry));
    }
  }
  lge_fclose(f);
  d->mmdb_count=ht_size(d->mmdb_cache);
#ifdef DEBUG
  fprintf(stderr,"Name translate cache:\n");
  for(i=0;i<d->mmdb_count;i++) {
    mmdb_entry *pe;
    ht_node *n;
    n=ht_get(d->mmdb_cache,i);
    pe=n->data;
    fprintf(stderr,"%03d %s %s\n",i,n->val,pe->trname);
  }
#endif
  return;
errend:
  if (f!=NULL) lge_fclose(f);
  d->mmdb_count=0;
  if (d->mmdb_cache != NULL) d->mmdb_cache=ht_free(d->mmdb_cache);
  return;
}


LGEDRV *lge_open(const char *devname,int readcache) {
  LGEDRV *d;
  lge_boot_sector bs;
  sb_t *sb;
  char name[512];
  off64_t o;
  __u32 part; // sector of boot sector
  __u32 off1;
  __u32 p0size;
  
  debug(("Opening drive %s\n",devname));
  d=malloc(sizeof(LGEDRV));
  if (d==NULL) { debug(("Alloc error\n")); errno=ENOMEM; return NULL; }
  memset(d,0,sizeof(LGEDRV));
  sb=&(d->sb[0]);
  d->fat_sector[0] = BAD_FAT;
  d->fat_sector[1] = BAD_FAT;
  d->f=fopen(devname,"r");
  if (d->f==NULL) {
    strcpy(name,"/dev/");
    strncat(name,devname,sizeof(name)-strlen(name)-1);
    d->f=fopen(name,"r");
    if (d->f==NULL) {
      debug(("Cannot open %s\n"));
      errno=ENOENT;
      free(d);
      return NULL;
    }
  }
  if (fread(name,sizeof(name),1,d->f) != 1) {
    fclose(d->f); free(d);
    debug(("Read BS/MBR error\n"));
    return NULL;
  }
  memcpy(&bs,name,sizeof(bs));
  part=0;
  off1=0;
  p0size=0;
  if (memcmp(bs.system_id,"LGEINC  ",8) != 0) {
    if (name[0x1FE] == 0x55 && name[0x1C1]==0x0B) {
      __le32 *l32;
      l32=(__le32 *)&(name[0x1C2]);
      part=le32_to_cpu(l32[1]);
      p0size=le32_to_cpu(l32[2]);
      off1=le32_to_cpu(l32[6]);
      if (off1 != 0) off1=le32_to_cpu(l32[0]);
      debug(("Partition table size1=0x%lxs size2=0x%lxs\n",
            p0size,le32_to_cpu(l32[6])));
      o=part; o*=0x200;
      fseeko64(d->f,o,SEEK_SET);
      if (fread(&bs,sizeof(bs),1,d->f) != 1) {
        fclose(d->f); free(d);
        debug(("Read BS error\n"));
        return NULL;
      }
    } else {
      fclose(d->f); free(d);
      debug(("Not a LGEINC drive\n"));
      return NULL;
    }
  }
  sb->sector_size = le16_to_cpu(bs.sector_size);
  sb->fat_start_s = le16_to_cpu(bs.hidden) + part;
  sb->clu1_s      = sb->fat_start_s + le32_to_cpu(bs.root_sec);
  sb->fat_size_s  = le32_to_cpu(bs.fat_length);
  sb->root_dir_s  = sb->clu1_s;
  sb->clustersize = le16_to_cpu(bs.sec_per_clus);
  sb->dir_e_per_s = sb->sector_size / sizeof(lge_dir_entry);
  sb->root_size_s = sb->dir_e_per_s * sb->clustersize;
  sb->total_sectors = le32_to_cpu(bs.total_sect);
  sb->total_clusters = (sb->total_sectors - sb->clu1_s) / sb->clustersize;
  debug(("sector_size    0x%x\n",sb->sector_size));
  debug(("fat_start_at   0x%x\n",sb->fat_start_s));
  debug(("fat_size_s     0x%x\n",sb->fat_size_s));
  debug(("clusters_at    0x%x\n",sb->clu1_s));
  debug(("dir_e_per_s    0x%x\n",sb->dir_e_per_s));
  debug(("total_sectors  0x%x\n",sb->total_sectors));
  debug(("total_clusters 0x%x\n",sb->total_clusters));
//  debug(("sizeof(dir_e)  0x%x\n",sizeof(lge_dir_entry)));
  debug(("sizeof(off_t)  %d\n",sizeof(o)));
  if (memcmp(bs.system_id,"LGEINC  ",8) != 0) {
    debug(("This is no LGEINC fs (%s)\n",bs.system_id));
    fclose(d->f); free(d);
    return NULL;
  }
  d->parts=1;
  d->total_sectors=sb->total_sectors + part;
  sector_read(name,d,sb->fat_start_s);
  if ((__u8)(name[0]) != bs.media) {
    debug(("Incorrect media in fat start %x <> %x\n",name[0],bs.media));
    return lge_close(d);
  }
  if (off1 != 0) {
    o=part + 1; o*=0x200;
    fseeko64(d->f,o,SEEK_SET);
    if (fread(&bs,sizeof(bs),1,d->f) != 1) {
      fclose(d->f); free(d);
      debug(("Read BS2 error\n"));
      return d;
    }
    sb=&(d->sb[1]);
    sb->sector_size = le16_to_cpu(bs.sector_size);
    sb->fat_start_s = le16_to_cpu(bs.hidden) + part + off1;
    sb->clu1_s      = p0size + part + le16_to_cpu(bs.hidden) + 
                      le32_to_cpu(bs.root_sec);
    sb->fat_size_s  = le32_to_cpu(bs.fat_length);
    sb->root_dir_s  = sb->clu1_s;
    sb->clustersize = le16_to_cpu(bs.sec_per_clus);
    sb->dir_e_per_s = sb->sector_size / sizeof(lge_dir_entry);
    sb->root_size_s = sb->dir_e_per_s * sb->clustersize;
    sb->total_sectors = le32_to_cpu(bs.total_sect);
    sb->total_clusters = (sb->total_sectors - sb->clu1_s) / sb->clustersize;
    debug(("sector_size    1 0x%x\n",sb->sector_size));
    debug(("fat_start_at   1 0x%x\n",sb->fat_start_s));
    debug(("fat_size_s     1 0x%x\n",sb->fat_size_s));
    debug(("clusters_at    1 0x%x\n",sb->clu1_s));
    debug(("dir_e_per_s    1 0x%x\n",sb->dir_e_per_s));
    debug(("total_sectors  1 0x%x\n",sb->total_sectors));
    debug(("total_clusters 1 0x%x\n",sb->total_clusters));
    debug(("sizeof(dir_e)  1 0x%x\n",sizeof(lge_dir_entry)));
    debug(("sizeof(off_t)  1 %d\n",sizeof(o)));
    if (memcmp(bs.system_id,"LGEINC  ",8) != 0) {
      debug(("This is no LGEINC fs (%s)\n",bs.system_id));
      return d;
    }
    sector_read(name,d,sb->fat_start_s);
    if ((__u8)(name[0]) != bs.media) {
      debug(("Incorrect media in fat start %x <> %x\n",name[0],bs.media));
      return d;
    }
    if (sb->sector_size != d->sb[0].sector_size) {
      debug(("Sector size not match error.\n"));
      return d;
    }
    d->parts=2;
    d->total_sectors+=sb->total_sectors;
    if (readcache) fillcache(d);
  }
  return d;
}

static LGDIR *inode_dir(LGEDRV *d,int part, __u32 inode) {
  LGDIR *dirp;
  int offset;
  sb_t *sb;
  if (d==NULL || part<0 || part>=d->parts) return NULL;
  sb=&(d->sb[part]);
  if (sb->root_dir_s == 0 || d->f==NULL) {
    debug(("Dire not open.\n"));
    errno=ENOENT; return NULL;
  }
  offset=lge_fat_read(d,part,inode);
  if (offset < 0) {debug(("fat%d read failed\n",part));errno=ENOENT; return NULL;};
  dirp=malloc(sizeof(LGDIR));
  if (dirp==NULL) { debug(("Alloc error\n")); errno=ENOMEM; return NULL; }
  memset(dirp,0,sizeof(LGDIR));
  dirp->cluster=inode;
  dirp->dir_pos=-1;
  dirp->d=d;
  dirp->part=part;
  return dirp;
}

int lge_findfile(LGEDRV *d, const char *name,struct lgedirent *de) {
  LGDIR *dirp;
  char nmbuf[4096],*p,*q;
  int nmlen,part;
  __u32 c;
  
  if (d==NULL || d->f==NULL) return -EIO;
  strncpy(nmbuf,name,sizeof(nmbuf)-1);
  nmlen=strlen(name);
  p=&(nmbuf[0]);
  c=1;
  while (*p=='/') p++;
  if (d->parts==2 && p[0]=='B' && (p[1]=='/' || p[1]==0)) {
    part=1;
    p+=2;
  } else {
    part=0;
  }
  while (1) {
    q=p;
    while ((*q != '/') && (*q != 0)) q++;
    while (*q=='/') {*q=0; q++;};
    debug(("lge_findfile%d c=%d '%s'\n",part,c,p));
    if ((dirp=inode_dir(d,part,c))==NULL) return -ENOTDIR;
    // root dir return
    if ((*p == 0) && (c==1)) {
      memset(de,0,sizeof(struct lgedirent));
      de->d_ino = 1;
      de->d_off = 0;
      de->d_reclen = sizeof(struct lgedirent);
      de->d_type = ATTR_DIR;
      de->d_size = de->d_off;
      de->d_mtime = 946681200;
      de->d_ctime = de->d_mtime;
      de->d_part = part;
      de->d_mm = NULL;
      break;
    }
    c=0;
    while (lge_readdir(dirp,de)==0) {
      if (strcasecmp(de->name,p)==0) {
        c=de->d_ino;
        break;
      }
    }
    dirp=lge_closedir(dirp);
    if (c==0) return -ENOTDIR;
    if (*q == 0) break;
    p=q;
  }
  debug(("found%d %s on %d\n",de->d_part,name,c));
  return 0;
}

LGDIR *lge_opendir(LGEDRV *d, const char *name) {
  struct lgedirent de;
  if (lge_findfile(d,name==NULL ? "/" : name,&de)!=0) return NULL;
  if ((de.d_type & ATTR_DIR)!=ATTR_DIR) return NULL;
  return inode_dir(d,de.d_part,de.d_ino);
}

int lge_readdir(LGDIR *dirp,struct lgedirent *de) {
  LGEDRV *d;
  lge_dir_entry *le;
  int pos,k;
  sb_t *sb;
  char segm[6];
  
  if (dirp==NULL) return -EIO;
  if (dirp->dir_pos < -1) return -EIO;
  d=dirp->d;
  if (dirp->part >= d->parts || dirp->part < 0) return -EIO;
  sb=&(d->sb[dirp->part]);
  while (1) {
    dirp->dir_pos++;
//    debug(("reading direntry c=%u pos %d\n",dirp->cluster,dirp->dir_pos));
    if (dirp->dir_pos % sb->dir_e_per_s == 0) {
      if ((dirp->dir_pos / sb->dir_e_per_s) > sb->clustersize) {
        debug(("next sector %d\n",dirp->dir_pos));
        // root ma jen jeden cluster
        if (dirp->cluster==1) goto l_enoent;
        dirp->cluster = next_cluster(d,dirp->part,dirp->cluster);
        if (dirp->cluster <= 1) goto l_enoent;
        dirp->dir_pos=0;
      }
      cluster_read(dirp->dir_cache,d,dirp->part,dirp->cluster,dirp->dir_pos / sb->dir_e_per_s,1);
    }
    pos = dirp->dir_pos % sb->dir_e_per_s;
    le=&(dirp->dir_cache[pos]);
//    debug(("dir entry %d = %04x%04x type %04x cluster=%u size %u\n",
//           pos,be16_to_cpu(le->name[0]),be16_to_cpu(le->name[1]),
//           le16_to_cpu(le->attr),le32_to_cpu(le->start),le32_to_cpu(le->size)));
    // end of dir - empty
    if (le->name[0] == 0) goto l_enoent;
    // deleted entry - search next
    if (be16_to_cpu(le->name[0]) != DELETED_FLAG) break;
  }
  memset(de,0,sizeof(struct lgedirent));
  de->d_part = dirp->part;
  de->d_ino = le32_to_cpu(le->start);
  de->d_off = le32_to_cpu(le->size_hi);
  de->d_off = (de->d_off << 32) + le32_to_cpu(le->size);
  de->d_reclen = sizeof(struct lgedirent);
  de->d_type = le16_to_cpu(le->attr);
  de->d_size = de->d_off;
  de->d_mtime = lge_datetime(le32_to_cpu(le->date),le32_to_cpu(le->time) << 8);
  de->d_ctime = de->d_mtime;
  de->d_mm = NULL;
  lge_be16_2_utf8(le->name,de->name,LGE_NAME,41);
  if (dirp->part==0 && dirp->cluster==1 && d->mmdb_count>0) {
    k=strlen(de->name) - 4;
    if (k>4 && (strcmp((char*)&(de->name[k]),".str")==0 ||
                strcmp((char*)&(de->name[k]),".idx")==0)) {
      de->name[k]=0;
      if (d->mmdb_cache) 
        de->d_mm=ht_find(d->mmdb_cache,(char *)&(de->name[4]));
      k=strlen(de->name);
      de->name[k]='.';
      if (de->d_mm != NULL) {
        if (de->d_mm->parts>1) {
          segm[0]='.';
          memcpy(&(segm[1]),de->name,4);
          segm[5]=0;
        } else {
          segm[0]=0;
        }
        snprintf(de->name,sizeof(de->name),"%s%s%s",
                 de->d_mm->trname,segm,de->name[k+1]=='s'?".mpg":".idx");
        de->d_ctime = de->d_mm->ctime;
      } 
#ifdef DEBUG
      else {
        fprintf(stderr,"file %s not found in mmdb\n",de->name);
      }
#endif
    }
  }
  return 0; 
  
l_enoent:
  dirp->dir_pos=-2;
  errno=ENOENT; // ENOTDIR
  return -ENOENT;
}

LGDIR *lge_closedir(LGDIR *dirp) {
  if (dirp != NULL) free(dirp);
  return NULL;
}

LGEDRV *lge_close(LGEDRV *drv) {
  if (drv != NULL) {
    if (drv->f != NULL) fclose(drv->f);
    free(drv);
  }
  return NULL;
}

LGFILE *lge_fopen(LGEDRV *d,const char *name) {
  LGFILE *f;
  struct lgedirent de;
  sb_t *sb;
  if (d == NULL || d->f == NULL) {errno=EIO; return NULL;}
  
  if (lge_findfile(d,name,&de)!=0) {errno=ENOENT; return NULL;}
  sb=&(d->sb[de.d_part]);
  f=malloc(sizeof(LGFILE) + sb->clustersize * sb->sector_size);
  if (f==NULL) return NULL;
  f->d = d;
  f->part = de.d_part;
  f->offset = 0;
  f->size = de.d_size;
  strncpy(f->name,name,sizeof(f->name)-1);
  f->name[sizeof(f->name)-1]=0;
  f->buflc=0;
  f->bufsize=sb->clustersize * sb->sector_size;
  f->inode = de.d_ino;
  f->bufc = de.d_ino;
  f->type = de.d_type;
  if (cluster_read(&(f->buf[0]),d,f->part,f->inode, 0, sb->clustersize)==NULL) {
    f->bufsize=0;
  }
  return f;
}

int lge_fread(void *buf,int size, LGFILE *f) {
//  off64_t o;
  __u64 log_c,off_c,cs;
  int left,ted;
  __u8 *bb;
  sb_t *sb;
  if (f==NULL || f->d==NULL || buf==NULL) {errno=EIO; return 0;};
  if (f->part < 0 || f->part >= f->d->parts) {errno=EIO; return 0;};
  sb=&(f->d->sb[f->part]);
  bb=buf;
  cs = sb->clustersize * sb->sector_size;
  left = size;
  while (left > 0) {
    log_c = f->offset / cs;
    off_c = f->offset % cs;
    if (log_c < f->buflc) { f->buflc=0; f->bufc=f->inode; f->bufsize=0;};
    while (log_c > f->buflc) { 
      f->bufsize=0;
      f->bufc = next_cluster(f->d,f->part,f->bufc);
      if (f->bufc <= 1) {errno=EIO; return size-left;};
      f->buflc++;
    }
    if (f->bufsize==0) {
      if (cluster_read(&(f->buf[0]),f->d,f->part,f->bufc, 0, sb->clustersize)==NULL) {
        errno=EIO; return size-left;
      }
      f->bufsize=cs;
    }
    ted=cs - off_c;
    if (ted > left) ted=left;
    memcpy(&(bb[size-left]),&(f->buf[off_c]),ted);
    debug(("fread offset=%lld log_c=%u physc=0x%x off_c=%u left=%d ted=%d\n",
            f->offset,log_c,f->bufc,off_c,left,ted));
    left-=ted;
    f->offset+=ted;
  }
  return size-left;
}

off64_t lge_fseek(LGFILE *f,off64_t o,int mode) {
  off64_t np;
  if (f==NULL) return 0;
  if (mode==SEEK_SET) np = o;
  else if (mode==SEEK_CUR) np = f->offset + o;
  else np = f->size + o; // SEEK_END
  if (f->offset != np) {
    f->offset=np;
    f->bufsize=0;
    f->buflc=0;
    f->bufc=f->inode;
  }
  return f->offset;
}

LGFILE *lge_fclose(LGFILE *f) {
  if (f!=NULL) free(f);
  return NULL;
}
