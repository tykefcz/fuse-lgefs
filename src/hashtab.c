/*
 * hashtab implementation by Jiri Gabriel (c) 2011
 * email: tykefcz@gmail.com
 */

#include <malloc.h>
#include <string.h>
#include "hashtab.h"

HASHTYPE ht_hfdef(const char *s) {
  HASHTYPE rv;
  for(rv=0;*s;s++) rv=rv * 31 + *s;
  return rv;
}

ht_tab *ht_new(ht_hfunc *phf) {
  ht_tab *rv=NULL;
  if ((rv=malloc(sizeof(ht_tab)))==NULL) return NULL;
  rv->size=0;
  rv->htlen=10;
  rv->nodes=malloc(sizeof(ht_node)*rv->htlen);
  if (rv->nodes==NULL) { free(rv); return NULL;}
  rv->hf = phf==NULL ? ht_hfdef : phf;
  return rv;
}

ht_tab *ht_free(ht_tab *ht) {
  int i;
  if (ht != NULL) {
    if (ht->nodes != NULL) {
      for (i=0;i<ht->size;i++) {
        if (ht->nodes[i].data!=NULL && ht->nodes[i].release) 
          free(ht->nodes[i].data);
        free(ht->nodes[i].val);
      }
      free(ht->nodes);
    }
    free(ht);
  }
  return NULL;
}

static int ht_intfind(ht_tab *ht,const char *s) {
  HASHTYPE h;
  int i,j;
  if (ht==NULL || s==NULL) return -1;
  h=ht->hf(s);
  for(i=ht->size / 2,j=i;j>1;j/=2) {
    if (h>ht->nodes[i].hash) i+=j; 
    else if (h<ht->nodes[i].hash) i-=j;
    else break;
    if (i<0) i=0;
    if (i>=ht->size) i=ht->size-1;
  }
  if(i>=ht->size) i=ht->size-1;
  while((i>0) && (h<=ht->nodes[i].hash)) i--;
  if (i<0) i=0;
  for(;i<ht->size && h>=ht->nodes[i].hash;i++) {
    if (h==ht->nodes[i].hash) {
      if (strcmp(s,ht->nodes[i].val)==0) {
        return i;
      }
    }
  }
  return -1;
}

void *ht_find(ht_tab *ht,const char *s) {
  int i;
  if (ht==NULL || s==NULL) return NULL;
  i=ht_intfind(ht,s);
  if (i>=0) return ht->nodes[i].data;
  return NULL;
}

static void ht_grow(ht_tab *ht) {
  if (ht==NULL) return;
  if (ht->htlen < 50) 
    ht->htlen += 20;
  else
    ht->htlen += ht->htlen / 2;
  ht->nodes=realloc(ht->nodes,sizeof(ht_node) * ht->htlen);
  return;
}

static int ht_newz(ht_tab *ht, const char *s, void *d, int dlen) {
  HASHTYPE h;
  int i,sl2,sl,dl;
  if (ht==NULL || s==NULL) return -1;
  h=ht->hf(s);
  if (ht->size >= ht->htlen) ht_grow(ht);
  for (i=0;i<ht->size;i++) {
    if (h<ht->nodes[i].hash) break;
  }
  if (i<ht->size) {
    memmove(&(ht->nodes[i+1]),&(ht->nodes[i]),
            (ht->size - i) * sizeof(ht_node));
  } else i=ht->size;
  ht->nodes[i].hash=h;
  if (dlen<0) {
    ht->nodes[i].release=dlen+2; // -1 release -2 no release
    ht->nodes[i].val=strdup(s);
    ht->nodes[i].data=d;
  } else if (dlen >= 0) {
    ht->nodes[i].release=0;
    sl = sl2 = strlen(s);
    dl = dlen + 4;
    sl = sl + 4 - ((sl + 4) % 4);
    dl -= dl % 4;
    ht->nodes[i].val=malloc(sl + dl);
    memcpy(ht->nodes[i].val,s,sl2 + 1);
    ht->nodes[i].data=&(ht->nodes[i].val[sl]);
    memcpy(ht->nodes[i].data,d,dlen);
  }
  return ht->size++;
}

// add preallocated data - at ht_free ot ht_del data will be released
int ht_add(ht_tab *ht,const char *s,void *d) {
  return ht_newz(ht,s,d,-1);
}

// add string - string copy is allocated (and released)
int   ht_addstring(ht_tab *ht,const char *s, const char *d) {
  return ht_newz(ht,s,(void *)d,strlen(d) + 1);
}

// add data by size - alloc new memory, copy data and add to ht
int   ht_adddup(ht_tab *ht, const char *s, void *d, int d_size) {
  if (d_size<=0) return -1;
  return ht_newz(ht,s,d,d_size);
}

// add data which not be released at ht_free or ht_del
int   ht_addconst(ht_tab *ht, const char *s, void *d) {
  return ht_newz(ht,s,d,-2);
}

int   ht_del(ht_tab *ht,const char *s) {
  int i;
  if (ht==NULL || s==NULL || ht->size<=0) return -1;
  i=ht_intfind(ht,s);
  if (i>=0) {
    if (ht->nodes[i].release) free(ht->nodes[i].data);
    free(ht->nodes[i].val);
    if (i!=(ht->size - 1)) {
      memmove(&(ht->nodes[i]),&(ht->nodes[i+1]),
              (ht->size - i - 1) * sizeof(ht_node));
    }
    ht->size--;
    return 0;
  }
  return -2;
}
 
int ht_size(ht_tab *ht) {
  return ht->size;
}

// for sequential read
ht_node *ht_get(ht_tab *ht,int i) {
  if (ht==NULL || i<0 || i>=ht->size) return NULL;
  return &(ht->nodes[i]);
}
