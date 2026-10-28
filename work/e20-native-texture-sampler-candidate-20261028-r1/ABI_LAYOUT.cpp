#include "core/extension/gdextension_interface.gen.h"
#include "ABI_CANDIDATE.h"
#include <stddef.h>
#ifdef __cplusplus
#define CHECK_LAYOUT static_assert
#else
#define CHECK_LAYOUT _Static_assert
#endif
CHECK_LAYOUT(sizeof(GDExtensionStorageDescriptorV2)==64,"descriptor64");
CHECK_LAYOUT(sizeof(GDExtensionStorageCreateRequestV2)==120,"create120");
CHECK_LAYOUT(sizeof(GDExtensionStorageAllocationViewV2)==80,"view80");
CHECK_LAYOUT(sizeof(GDExtensionStorageBackingV2)==216,"backing216");
CHECK_LAYOUT(sizeof(GDExtensionStorageBackingQueryV2)==40,"query40");
CHECK_LAYOUT(offsetof(GDExtensionStorageBackingV2,descriptor)==sizeof(GDExtensionStorageBackingV1),"preserved V1 prefix layout, never reinterpret");
