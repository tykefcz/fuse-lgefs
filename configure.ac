#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.67])
AC_INIT(fuse-lgefs, 1.1, tykefcz@gmail.com)
AC_CONFIG_SRCDIR([src/lgefuse.c])
AC_CONFIG_HEADERS([include/config.h:include/config-h.in])
AC_CONFIG_FILES([Makefile
src/Makefile])

AM_INIT_AUTOMAKE

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h stddef.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_SYS_LARGEFILE
AC_FUNC_FSEEKO
AC_CHECK_FUNCS([fseeko64])
AC_CHECK_TYPE([off64_t], [],
   [
      AC_DEFINE_UNQUOTED([off64_t], [int64_t],
      [Define to `int64_t' if <sys/types.h> does not define.])
   ],
   [#include <sys/types.h>]
)
	
# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_FUNC_REALLOC
AC_CHECK_FUNCS([localtime_r memmove memset strcasecmp strdup])
PKG_CHECK_MODULES(FUSE, fuse)

AC_OUTPUT
