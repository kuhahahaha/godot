#include <stddef.h>
#include "t1_layout_candidate.h"
_Static_assert(sizeof(T1Origin)==184, "origin");
_Static_assert(offsetof(T1Origin, request_digest)==96, "request digest offset");
_Static_assert(offsetof(T1Origin, allocation_generation)==176, "allocation offset");
_Static_assert(sizeof(T1TextureCreate)==256, "texture");
_Static_assert(sizeof(T1SamplerCreate)==240, "sampler");
_Static_assert(sizeof(T1SetCreate)==256, "set");
_Static_assert(sizeof(T1CreateReport)==88, "create report");
_Static_assert(sizeof(T1RetireReport)==128, "retire report");
_Static_assert(sizeof(T1TextureSegment)==120, "segment");
_Static_assert(_Alignof(T1Origin)==8, "alignment");
int main(void) { return 0; }
