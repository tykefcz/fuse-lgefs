/* 
 * LGEINC FAT32 implementation for FUSE 
 * by Jiri Gabriel <tykefcz@gmail.com>
 * 2011
 */

#include "config.h"
#define MAX_OPENFILES	20

#define FUSE_USE_VERSION  26
   
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <unistd.h>
#include <stdlib.h>
#include "lglib.h"
#include "hashtab.h"

LGEDRV *drv=NULL;

struct lgefs_config {
     int convert;
     char *device;
} config;

enum {
     KEY_HELP,
     KEY_VERSION,
};

#define MYFS_OPT(t, p, v) { t, offsetof(struct lgefs_config, p), v }

static struct fuse_opt lgefs_opts[] = {
//     MYFS_OPT("mynum=%i",          mynum, 0),
//     MYFS_OPT("-n %i",             mynum, 0),
//     MYFS_OPT("device=%s",         device, 0),
     MYFS_OPT("convert",           convert, 1),
     MYFS_OPT("noconvert",         convert, 0),
     MYFS_OPT("--convert=true",    convert, 1),
     MYFS_OPT("--convert=false",   convert, 0),

     FUSE_OPT_KEY("-V",             KEY_VERSION),
     FUSE_OPT_KEY("--version",      KEY_VERSION),
     FUSE_OPT_KEY("-h",             KEY_HELP),
     FUSE_OPT_KEY("--help",         KEY_HELP),
     FUSE_OPT_END
};

static ht_tab *dircache=NULL;

static int lgefs_getattr(const char *path, struct stat *stbuf) {
    int res = 0;
    struct lgedirent de;
    struct stat *cs;
#ifdef DEBUG
    fprintf(stderr,"getattr '%s'\n",path);
#endif
    if (dircache!=NULL) {
      cs=(struct stat *)ht_find(dircache,path);
      if (cs!=NULL) {
        memcpy(stbuf,cs,sizeof(struct stat));
        return res;
      }
    }
    memset(stbuf, 0, sizeof(struct stat));
    if(strcmp(path, "/") == 0) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_mtime = stbuf->st_ctime = stbuf->st_atime = 946681200;
        goto ga_end;
    }
    if (strcmp(path,"/B") == 0 && drv->parts > 1) {
        stbuf->st_mode = S_IFDIR | 0755;
        stbuf->st_nlink = 2;
        stbuf->st_mtime = stbuf->st_ctime = stbuf->st_atime = 946681200;
        goto ga_end;
    }

    if (lge_findfile(drv,path==NULL ? "/" : path,&de)!=0) return -ENOENT;
    stbuf->st_mtime = de.d_mtime;
    stbuf->st_ctime = de.d_ctime;
    stbuf->st_atime = de.d_mtime;
    if ((de.d_type & ATTR_DIR) == ATTR_DIR) {
      stbuf->st_mode = S_IFDIR | 0755;
      stbuf->st_nlink = 2;
      stbuf->st_size = drv->sb[de.d_part].clustersize * drv->sb[de.d_part].sector_size;
    } else if ((de.d_type & ATTR_VOLUME) != ATTR_VOLUME) {
      stbuf->st_mode = S_IFREG | 0444;
      stbuf->st_nlink = 1;
      stbuf->st_size = de.d_size;
    }
    else
      res = -ENOENT;
ga_end:
    if (res==0 && dircache!=NULL) {
      cs=malloc(sizeof(struct stat));
      memcpy(cs,stbuf,sizeof(struct stat));
      ht_add(dircache,path,cs);
    }
    return res;
}

LGDIR *opendirs[MAX_OPENFILES];

static int lgefs_opendir(const char *path, struct fuse_file_info *fi) {
    int i;
    for (i=0; i<MAX_OPENFILES && opendirs[i] != NULL;i++) ;
    if (i>=MAX_OPENFILES || opendirs[i] != NULL) return -EMFILE;
    opendirs[i]=lge_opendir(drv,path);
    if (opendirs[i]==NULL) return -ENOENT;
    return 0;
}

static int lgefs_closedir(const char *path, struct fuse_file_info *fi) {
    int i;
    i=(fi->fh & 0x000FFF);
    if (i<0 || i>MAX_OPENFILES) return -ENOENT;
    opendirs[i]=lge_closedir(opendirs[i]);
    return 0;
}

static void cache_filler(const char *p, const char *s, struct stat *stbuf) {
  struct stat *cs;
  char fp[1024];
  if (dircache!=NULL) {
    snprintf(fp,sizeof(fp),"%s/%s",p,s);
    cs=ht_find(dircache,fp);
    if (cs==NULL) {
      cs=malloc(sizeof(struct stat));
      memcpy(cs,stbuf,sizeof(struct stat));
      ht_add(dircache,fp,cs);
    }
  }
}

static int lgefs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                         off_t offset, struct fuse_file_info *fi) {
    int i;
    LGDIR *d;
    struct lgedirent de;
    struct stat st;

    i=(fi->fh & 0x000FFF);
    if (i<0 || i>MAX_OPENFILES) return -ENOENT;
    d=opendirs[i];
    if (d==NULL) return -ENOENT;

    memset(&st, 0, sizeof(struct stat));
    st.st_mode = S_IFDIR | 0755;
    st.st_nlink = 2;
    st.st_mtime = st.st_ctime = st.st_atime = 946681200;
    if ((offset==0) && (strcmp(path, "/") == 0)) {
      filler(buf, ".", &st, 0);
      filler(buf, "..", &st, 0);
      cache_filler(path,"..",&st);
      if (drv->parts>1) filler(buf,"B", &st,0);
    } else if ((offset==0) && (strcmp(path,"/B") == 0)) {
      filler(buf, ".", &st, 0);
      filler(buf, "..", &st, 0);
      cache_filler(path,"..",&st);
    }

    while (lge_readdir(d,&de)==0) {
      st.st_mtime = st.st_atime = de.d_mtime;
      st.st_ctime = de.d_ctime;
      if ((de.d_type & ATTR_DIR) == ATTR_DIR) {
        st.st_mode = S_IFDIR | 0755;
        st.st_nlink = 2;
        st.st_size = drv->sb[de.d_part].clustersize * drv->sb[de.d_part].sector_size;
      } else  {
        st.st_mode = S_IFREG | 0444;
        st.st_nlink = 1;
        st.st_size = de.d_size;
      }
      if ((de.d_type & ATTR_VOLUME) != ATTR_VOLUME) {
        filler(buf,de.name , &st, 0);
        cache_filler(path,de.name,&st);
      }
    }
    return 0;
}

LGFILE *openfiles[MAX_OPENFILES];

static int lgefs_open(const char *path, struct fuse_file_info *fi) {
    int i;
    if((fi->flags & 3) != O_RDONLY)
        return -EACCES;
    for (i=0; i<MAX_OPENFILES && openfiles[i] != NULL;i++) ;
    if (i>=MAX_OPENFILES || openfiles[i] != NULL) return -EMFILE;
    openfiles[i]=lge_fopen(drv,path);
    if (openfiles[i] == NULL) return -ENOENT;
    fi->fh=i;
    return 0;
}

static int lgefs_close(const char *path, struct fuse_file_info *fi) {
    int i;
    i=(fi->fh & 0x000FFF);
    if (i<0 || i>MAX_OPENFILES) return -ENOENT;
    openfiles[i]=lge_fclose(openfiles[i]);
    return 0;
}

static int lgefs_read(const char *path, char *buf, size_t size, off_t offset,
                      struct fuse_file_info *fi) {
    size_t len;
    int i;
    LGFILE *f;
    i=(fi->fh & 0x000FFF);
    if (i<0 || i>MAX_OPENFILES) return -ENOENT;
    f=openfiles[i];
    if (f==NULL) return -ENOENT;
    if (f->offset != offset) lge_fseek(f,offset,SEEK_SET);
    len=lge_fread(buf,size,f);
    return len;
}

void lgefs_destroy(void *data) {
  drv=lge_close(drv);
}

static struct fuse_operations lgefs_oper = {
    .getattr = lgefs_getattr,
    .opendir = lgefs_opendir,
    .readdir = lgefs_readdir,
    .releasedir = lgefs_closedir,
    .open   = lgefs_open,
    .read   = lgefs_read,
    .release = lgefs_close,
    .destroy = lgefs_destroy,
};

static int lgefs_opt_proc(void *data, const char *arg, int key, struct fuse_args *outargs)
{
     struct lgefs_config *cnf;
     
     cnf=(struct lgefs_config *)data;

     switch (key) {
     case KEY_HELP:
             fprintf(stderr,
                     "usage: %s mountpoint device [options]\n"
                     "\n"
                     "general options:\n"
                     "    -o opt,[opt...]  mount options\n"
                     "    -h   --help      print help\n"
                     "    -V   --version   print version\n"
                     "\n"
                     "lgefs options:\n"
                     "    -o convert\n"
                     "    -o noconvert\n"
                     "    --convert=BOOL    same as 'convert' or 'noconvert'\n"
                     , outargs->argv[0]);
             fuse_opt_add_arg(outargs, "-ho");
             fuse_main(outargs->argc, outargs->argv, &lgefs_oper, NULL);
             exit(1);

     case KEY_VERSION:
             fprintf(stderr, "lgefs version %s\n", PACKAGE_VERSION);
             fuse_opt_add_arg(outargs, "--version");
             fuse_main(outargs->argc, outargs->argv, &lgefs_oper, NULL);
             exit(0);
     case FUSE_OPT_KEY_NONOPT:
       if (cnf->device == NULL) {
         cnf->device = strdup(arg);
         return 0;
       } 
     }
     return 1;
}

int main(int argc, char *argv[]) {
     int rv;
     struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

     memset(&config, 0, sizeof(config));
     config.convert=1;
     memset(&opendirs,0,sizeof(opendirs));
     memset(&openfiles,0,sizeof(openfiles));
     dircache=ht_new(NULL);

     rv=fuse_opt_parse(&args, &config, lgefs_opts, lgefs_opt_proc);
     if (rv) return 1;
     
     fuse_opt_add_arg(&args, "-s");     
     if (config.device == NULL) {
       fprintf(stderr,"Device or file must be specified\n");
       return 2;
     }
     drv=lge_open(config.device,config.convert);
     if (drv==NULL) {
       fprintf(stderr,"Device %s is not LGEINC filesystem\n",config.device);
       free(config.device);
       return 3;
     }
     free(config.device); config.device=NULL;
     dircache=ht_new(NULL);

     rv=fuse_main(args.argc, args.argv, &lgefs_oper, NULL);
     
     drv=lge_close(drv);
     dircache=ht_free(dircache);
     return rv;
}
