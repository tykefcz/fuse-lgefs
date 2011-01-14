#ifndef HASHTAB_H
#define HASHTAB_H	1

#ifndef HASHTYPE
#define HASHTYPE	int
#endif

typedef struct _ht_node {
  HASHTYPE hash;
  int  release;
  char *val;
  void *data;
} ht_node;

typedef HASHTYPE (ht_hfunc)(const char *s);

typedef struct {
  int size,htlen;
  ht_hfunc *hf;
  ht_node  *nodes;
} ht_tab;

ht_tab *ht_new(ht_hfunc *phf);
ht_tab *ht_free(ht_tab *ht);
void *ht_find(ht_tab *ht,const char *s);
// add preallocated data - at ht_free ot ht_del data will be released
int   ht_add(ht_tab *ht,const char *s,void *d);
// add string - string copy is allocated (and released)
int   ht_addstring(ht_tab *ht,const char *s, const char *d);
// add data by size - alloc new memory, copy data and add to ht
int   ht_adddup(ht_tab *ht, const char *s, void *d, int d_size);
// add data which not be released at ht_free or ht_del
int   ht_addconst(ht_tab *ht, const char *s, void *d);
int   ht_del(ht_tab *ht,const char *s);
int   ht_size(ht_tab *ht);
// for sequential read
ht_node *ht_get(ht_tab *ht,int i);

HASHTYPE ht_hfdef(const char *s);
#endif
