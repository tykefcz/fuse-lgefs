#ifndef ENDIAN_H
#define ENDIAN_H	1
#if defined(__BIG_ENDIAN__) || defined(_BIG_ENDIAN)

#define le64_to_cpu(x) (\
  (((x)>>56)&0xffLL) |\
  (((x)>>40)&0xff00LL) |\
  (((x)>>24)&0xff0000LL) |\
  (((x)>>8)&0xff000000LL) |\
  (((x)<<8)&0xff00000000LL) |\
  (((x)<<24)&0xff0000000000LL) |\
  (((x)<<40)&0xff000000000000LL) |\
  (((x)<<56)&0xff00000000000000LL) )

#define le32_to_cpu(x) (\
  (((x)>>24)&0xff) |\
  (((x)>>8)&0xff00) |\
  (((x)<<8)&0xff0000) |\
  (((x)<<24)&0xff000000) )
              
#define le16_to_cpu(x) ( (((x)>>8)&0xff) | (((x)<<8)&0xff00) )
#define be64_to_cpu(x) (x)
#define be32_to_cpu(x) (x)
#define be16_to_cpu(x) (x)
              
#else
              
#define le64_to_cpu(x) (x)
#define le32_to_cpu(x) (x)
#define le16_to_cpu(x) (x)

#define be64_to_cpu(x) (\
  (((x)>>56)&0xffLL) |\
  (((x)>>40)&0xff00LL) |\
  (((x)>>24)&0xff0000LL) |\
  (((x)>>8)&0xff000000LL) |\
  (((x)<<8)&0xff00000000LL) |\
  (((x)<<24)&0xff0000000000LL) |\
  (((x)<<40)&0xff000000000000LL) |\
  (((x)<<56)&0xff00000000000000LL) )

#define be32_to_cpu(x) (\
  (((x)>>24)&0xff) |\
  (((x)>>8)&0xff00) |\
  (((x)<<8)&0xff0000) |\
  (((x)<<24)&0xff000000) )
#define be16_to_cpu(x) ( (((x)>>8)&0xff) | (((x)<<8)&0xff00) )
              
#endif
              
#define cpu_to_le64(x) le64_to_cpu(x)
#define cpu_to_le32(x) le32_to_cpu(x)
#define cpu_to_le16(x) le16_to_cpu(x)
#define cpu_to_be64(x) be64_to_cpu(x)
#define cpu_to_be32(x) be32_to_cpu(x)
#define cpu_to_be16(x) be16_to_cpu(x)

#endif
