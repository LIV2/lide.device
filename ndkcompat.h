#ifndef __NDK_COMPAT_H
#define __NDK_COMPAT_H

#include <inttypes.h>

/* ULONG has changed from NDK 3.9 to NDK 3.2.
 * However, PRI*32 did not. What is the right way to implement this?
 */
#if INCLUDE_VERSION < 47
#undef PRIu32
#define PRIu32 "lu"
#undef PRId32
#define PRId32 "ld"
#undef PRIx32
#define PRIx32 "lx"
#endif

#endif