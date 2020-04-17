#include <v8.h>
#if defined(V8_COMPRESS_POINTERS) && (COMPRESS_POINTERS_BOOL == 1)
#error "Pointer compression is on"
#endif
