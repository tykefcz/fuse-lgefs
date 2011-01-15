#ifndef LGFAT32_H
#define LGFAT32_H	1

#define LGE_ROOT_INO	1	/* == MINIX_ROOT_INO */

#define ATTR_NONE	0	/* no attribute bits */
#define ATTR_RO		1	/* read-only */
#define ATTR_HIDDEN	2	/* hidden */
#define ATTR_SYS	4	/* system */
#define ATTR_VOLUME	8	/* volume label */
#define ATTR_DIR	16	/* directory */
#define ATTR_ARCH	32	/* archived */

#define DELETED_FLAG	0x00E5 /* marks file as deleted when in name[0] */

#define MAX_FAT	0x0FFFFFF6
#define BAD_FAT	0x0FFFFFF7
#define FAT_START_ENT	2
#define LGE_NAME	45	/* maximum name length */

#pragma pack(1)
typedef struct  {
	__u8	ignored[3];	/* Boot strap short or near jump */
	__u8	system_id[8];	/* Name - can be used to special case
				   partition manager volumes "LGEINC  " */
	__le16	sector_size;	/* bytes per logical sector */
	__u8    ignored1;       /* 00 */
	__le16	hidden;	        /* reserved sectors */
	__u8	fats;		/* number of FATs */
	__u8    ignored2[4];    /* TYKEF 00 00 00 00 */
	__u8	media;		/* media code */
        __u8    ignored3[2];    /* TYKEF 00 00 00 00 */
	__le16	secs_track;	/* sectors per track 0x3F 00 */
	__le16  ignored4;       /* ?? TYKEF 00 01 */
	__le32	ignored5;	/* ?? TYKEF 3F ?? */
	__le32	total_sect;	/* number of sectors */
        __le32  fat_length;    /* TYKEF 0xE86 = 3718 / 0x190=400 */
        __u8    ignored6[8];    /* 00 00 00 00 02 00 00 00 */
        __le16  fsinfo_sec;     /* 04 00 */
        __le16  neco;           /* 06 00 */
        __le32  root_sec;	/* sector of cluster 1 = root DIR*/
        __u8    ignored7[15];   /* ?? 00000000 00000000 80002900 000000 */
        __u8    label[11];     /* volume_label "VOLUMELABE "*/
        __u8    fatid[8];      /* "FAT32   "   */
        __u8    ignored8[6];   /* 000000000000 */
        __le16  sec_per_clus;  /* 00 04 */
} lge_boot_sector;

typedef struct  {
   __be16   name[LGE_NAME]; // 0x00 - Filename max. 40 chars
                            // 0x50 = 00 00 
                            // 0x52 - Extension
   __le16   attr;           // 0x5a - File attributes
   __le32   start;          // 0x5c - Starting cluster number
   __le32   time;           // 0x60 - File creation time (ctime)
   __le32   date;           // 0x64 - File creation date (cdate)
   __le32   size;           // 0x68 - File size in bytes
   __le32   size_hi;        // 0x6c 
   __le32   time2;          // 0x70 - u str tam neco je
   __le32   res2[3];        // 0x74 - u str 000000 a jeste jednou size
} lge_dir_entry;

#pragma pack()
#endif
