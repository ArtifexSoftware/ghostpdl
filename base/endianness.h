#ifndef LEPTONICA_ENDIANNESS_H
#define LEPTONICA_ENDIANNESS_H

#include "arch.h"

#if ARCH_IS_BIG_ENDIAN
#define L_BIG_ENDIAN
#endif

#ifdef L_BIG_ENDIAN
#else
# ifndef L_LITTLE_ENDIAN
#  define L_LITTLE_ENDIAN
# endif
#endif

#endif
