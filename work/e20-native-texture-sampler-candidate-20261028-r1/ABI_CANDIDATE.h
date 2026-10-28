/* REVIEW CANDIDATE ONLY. Not installed, generated, compiled or registered.
 * The existing generated GDExtension ABI declarations must precede this file.
 * Add these declarations to gdextension_interface.json after root adoption.
 * V1 declarations, function names, byte sizes and opcode meanings stay intact.
 */
#include <stdint.h>

/* Wire constants, deliberately independent of Godot enum ordinals. */
enum {
  GDEXTENSION_STORAGE_BUFFER_V2 = 1,
  GDEXTENSION_STORAGE_TEXTURE_2D_RGBA32F_V2 = 2,
  GDEXTENSION_STORAGE_SAMPLER_NEAREST_CLAMP_V2 = 3,
  GDEXTENSION_STORAGE_FORMAT_NONE_V2 = 0,
  GDEXTENSION_STORAGE_FORMAT_RGBA32F_V2 = 1,
  GDEXTENSION_STORAGE_USAGE_STORAGE_V2 = 1,
  GDEXTENSION_STORAGE_USAGE_SAMPLED_V2 = 2,
  GDEXTENSION_STORAGE_FILTER_NEAREST_V2 = 1,
  GDEXTENSION_STORAGE_REPEAT_CLAMP_TO_EDGE_V2 = 1,
  GDEXTENSION_QUALIFICATION_RT_STORAGE_CREATE_V2 = 10,
  GDEXTENSION_QUALIFICATION_RT_STORAGE_BACKING_V2 = 11
};

/* 64 bytes, align 8. No pointers, strings, native enum ordinals or unions.
 * BUFFER: only kind nonzero, bytes in request; descriptor remainder zero.
 * TEXTURE: format=RGBA32F, width/height>0, depth/layers/mipmaps/samples=1,
 * usage=STORAGE|SAMPLED, filter/repeat=0; no imports, overrides or aliases.
 * SAMPLER: only kind/filter/repeat nonzero, filter=NEAREST/repeat=CLAMP_TO_EDGE.
 */
typedef struct {
  uint32_t kind, format, width, height, depth, layers, mipmaps, samples;
  uint64_t usage;
  uint32_t filter, repeat;
  uint64_t reserved[2];
} GDExtensionStorageDescriptorV2;

/* 120 bytes, align 8. abi={2,120,0} (not the historical V1 header size).
 * bytes=buffer bytes, or checked width*height*16 for texture, or 0 for sampler.
 * Slot/part/gen/attempt retain their original StorageOwner meaning.
 */
typedef struct {
  GDExtensionDeviceExitAbi abi;
  uint64_t bytes;
  uint32_t allocation_slot, part;
  uint64_t allocation_generation, create_attempt;
  GDExtensionStorageDescriptorV2 descriptor;
  uint64_t reserved;
} GDExtensionStorageCreateRequestV2;

/* 80 bytes, align 8. Snapshot of ORIGINAL RD object metadata, never source
 * approval. For owned texture parent_rid/parent_serial name itself (actual
 * Texture.owner remains null). No claimed parent for sampler. View version=1.
 * Texture content_version=0 at empty creation and is advanced ONLY by original
 * qualified producer append (see ADOPTION.md); it is not GPU completion.
 * Byte range is [0,bytes); texture subresource is color,mip0,layer0,count1.
 * Sampler ranges, subresource and content_version are all zero.
 */
typedef struct {
  uint64_t parent_rid, parent_backing_serial, view_version, content_version;
  uint64_t byte_begin, byte_end;
  uint32_t base_mipmap, mipmap_count, base_layer, layer_count;
  uint32_t resource_kind, format;
  uint64_t reserved;
} GDExtensionStorageAllocationViewV2;

/* 216 bytes, align 8. abi={2,216,0}. First 72 bytes have the V1 layout,
 * but this struct must not be passed to a V1 output API or reinterpret-cast.
 * The shared backing ref is still the ORIGINAL Registry Storage child.
 * byte_size is logical payload size (sampler=0), usage_bits is RD usage for
 * buffer/texture and 0 for sampler; descriptor uses wire enum values above.
 * native_error=OK,state=1 means a known live original object; state=2 means
 * retained unresolved creation, NEVER a successful usable object.
 */
typedef struct {
  GDExtensionDeviceExitAbi abi;
  GDExtensionStorageBackingRefV1 backing;
  uint64_t rid, backing_serial, byte_size, usage_bits;
  int32_t native_error;
  uint32_t state;
  GDExtensionStorageDescriptorV2 descriptor;
  GDExtensionStorageAllocationViewV2 view;
} GDExtensionStorageBackingV2;

/* 40 bytes, abi={2,40,0}; query cannot acquire a pin or create authority.
 * Exact same-bridge original child and observed serial must match.
 */
typedef struct {
  GDExtensionDeviceExitAbi abi;
  GDExtensionStorageBackingRefV1 backing;
  uint64_t observed_backing_serial;
} GDExtensionStorageBackingQueryV2;

typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationStorageCreateV2)(
    GDExtensionQualificationBridgeRefV1,
    const GDExtensionStorageCreateRequestV2 *, GDExtensionStorageBackingV2 *);
typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationStorageBackingV2)(
    GDExtensionQualificationBridgeRefV1,
    const GDExtensionStorageBackingQueryV2 *, GDExtensionStorageBackingV2 *);

/* Register names:
 * device_session_qualification_storage_create_v2
 * device_session_qualification_storage_backing_v2
 * Retirement remains EXACTLY the existing V1 retire/observe/release APIs,
 * refs, report, budget, original cell and driver-return authority.
 */
