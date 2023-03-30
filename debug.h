#define DBG_INFO  0
#define DBG_WARN  1
#define DBG_TRACE 2

#if DEBUG
#include <clib/debug_protos.h>
#define Info KPrintF
#else
#define Info
#endif

#if DEBUG >= DBG_WARN
#define Warn KPrintF
#else
#define Warn
#endif

#if DEBUG >= DBG_TRACE
#define Trace KPrintF
#else
#define Trace
#endif
