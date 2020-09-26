#include <v8.h>
#if defined(V8_MAJOR_VERSION) && (V8_MAJOR_VERSION > 3 || V8_MINOR_VERSION > 16)
#error "Version of libv8 is newer than 3.14 or 3.15. Disable legacy API"
#endif
