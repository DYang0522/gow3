#include "bbgpu.h"

// Online play is deliberately disabled in the first hardware-validation build.
// The Bloodborne runtime's offline HLE services remain available.
extern "C" void bbnet_configure(const char *, const char *, const char *, const char *) {}
extern "C" void bbnet_initialize(void) {}
extern "C" void bbnet_register(void) {}
