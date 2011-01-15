#ifndef LGMME_H
#define LGMME_H	1

#pragma pack(1)
typedef struct  { // 72bytes = 18 x 32bitu
  __be32  pa[2];
  __be32  records;
  __be32  pb[15];
} mmdb_hdr;

typedef struct { // 362 bytes
  __be32  pa[29];        // 29
  __be16  channel;
  __u8    ra;
  __u8    chan_name[20];
  __u8    rb;            // 35
  __be32  rec_date;      // 36
  __be32  rec_time;      // 37
  __be32  delka_sec;     // 38
  __be64  file_size;     // 40
  __u8    zanr;
  __u8    rc;
  __be16  jmeno[39];     // 60
  __be16  files;         // 60.5
} mmdb_z;

typedef struct { // 232 bytes
  __be16 qa;
  __be32 pa[2];
  __be16 filename_str[32];
  __be16 filename_idx[32];
  __be32 pb[23];
  __be16 qb;
} mmdb_file;

typedef struct { // 130 bytes
  __be32 pa[14];  // bylo 32*4 + 2
  __be16 qa;
  __be16 name[34];
  __be32 pb;     // 35*2 + 9*4
} mmdb_tail;
#pragma pack()

#endif
