#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "hashtab.h"

void test() {
  ht_tab *h;
  char tst[20];
  int i;
  ht_node *n;
  
  h=ht_new(NULL);
  for(i=0;i<20;i++) {
    snprintf(tst,sizeof(tst),"%d",i*31);
    switch (i%3) {
      case 0: ht_add(h,tst,strdup(tst)); break;
      case 1: ht_addstring(h,tst,tst); break;
      case 2: ht_adddup(h,tst,tst,strlen(tst)+1); break;
    }
  }
  ht_addconst(h,"12","konstanta");
  printf("allocated %d table size=%d\n",h->htlen,h->size);
  for(i=0;(n=ht_get(i))!=NULL;i++) {
    printf("%10x %s = %s\n",n->hash,n->val,(char *)n->data);
  }
  ht_del(h,"62"); ht_del(h,tst); ht_del(h,"0"); ht_del(h,"12");
  ht_del(h,"mana");
  printf("odmazano %d table size=%d\n",h->htlen,h->size);
  for(i=0;(n=ht_get(i))!=NULL;i++) {
    printf("%10x %s = %s\n",n->hash,n->val,(char *)n->data);
  }
  h=ht_free(h);
}

int main() {
  test();
  return 0;
}
