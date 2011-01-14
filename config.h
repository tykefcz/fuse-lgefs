#ifndef CONFIG_H

#define CONFIG_H	1
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION	"1.0"
#endif
#ifndef __USE_FILE_OFFSET64
#define __USE_FILE_OFFSET64	1
#endif
#ifndef __USE_LARGEFILE64
#define __USE_LARGEFILE64	1
#endif
#ifdef D_FILE_OFFSET_BITS
#undefine D_FILE_OFFSET_BITS
#endif
#define D_FILE_OFFSET_BITS	64
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE    1
#endif
#endif
