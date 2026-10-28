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
  GDEXTENSION_STORAGE_USAGE_TRANSFER_TO_V2 = 4,
  GDEXTENSION_STORAGE_FILTER_NEAREST_V2 = 1,
  GDEXTENSION_STORAGE_REPEAT_CLAMP_TO_EDGE_V2 = 1,
  GDEXTENSION_QUALIFICATION_RT_STORAGE_CREATE_V2 = 10,
  GDEXTENSION_QUALIFICATION_RT_STORAGE_BACKING_V2 = 11,
  GDEXTENSION_QUALIFICATION_RT_TEXTURE_PREPARE_V2 = 12,
  GDEXTENSION_QUALIFICATION_RT_TEXTURE_TRANSFER_V2 = 13,
  GDEXTENSION_QUALIFICATION_RT_TEXTURE_RECEIPT_V2 = 15,
  GDEXTENSION_QUALIFICATION_RT_TEXTURE_LAYOUT_V2 = 16
};

/* 64 bytes, align 8. No pointers, strings, native enum ordinals or unions.
 * BUFFER: only kind nonzero, bytes in request; descriptor remainder zero.
 * TEXTURE: format=RGBA32F, width/height>0, depth/layers/mipmaps/samples=1,
 * usage=STORAGE|SAMPLED|TRANSFER_TO, filter/repeat=0; no imports/aliases.
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
 * qualified transfer append (see R2_TYPED_RECORD_TRANSFER.md); not completion.
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

/* r2 additions. All V1 declarations and the r1 V2 struct layouts stay intact.
 * Every outer V2 abi is {2,sizeof(outer struct),0}; nested V1 abi stays V1.
 */
typedef struct { /* 48 bytes, align8. Exact original Storage child identity. */
  GDExtensionStorageBackingRefV1 backing;
  uint64_t rid, backing_serial, allocation_generation;
  uint32_t allocation_slot, part;
} GDExtensionQualificationStorageIdentityV2;

typedef struct { /* 88 bytes, align8. Exact owned allocation/view/write range. */
  uint64_t parent_rid, parent_backing_serial, view_version, byte_begin, byte_end;
  uint32_t format, aspect; /* Wire RGBA32F=1, color aspect=1. */
  uint32_t base_mipmap, mipmap_count, base_layer, layer_count;
  uint32_t x, y, z, width, height, depth;
} GDExtensionQualificationTextureRegionV2;

typedef struct { /* 32 bytes. Not a separately registered object/authority. */
  uint64_t registry_instance, storage_child_serial;
  uint64_t producer_ticket_serial, content_serial;
} GDExtensionQualificationContentRefV2;

typedef struct { /* 64 bytes; output of original texture layout inspection. */
  GDExtensionDeviceExitAbi abi;
  GDExtensionStorageBackingRefV1 backing;
  uint64_t buffer_offset_alignment, row_pitch, source_byte_count, reserved;
} GDExtensionQualificationTextureLayoutV2;

/* Source version labels belong to C/B's original retained capture domain;
 * they never set native content_version. D checks exact raw64 byte digest.
 */
typedef struct { /* 56 bytes, align8. Original capture association, not permit. */
  uint64_t source_version, captured_byte_count;
  uint8_t capture_digest[32];
  uint32_t original_vector_ordinal, reserved;
} GDExtensionQualificationTextureCaptureV2;

typedef struct { /* 360 bytes, align8. Exactly one image plan per ticket. */
  GDExtensionDeviceExitAbi abi;
  GDExtensionQualificationStorageIdentityV2 source_buffer, image, sampler;
  GDExtensionQualificationTextureRegionV2 region;
  uint32_t source_upload_index, row_pitch;
  uint64_t source_offset, source_size;
  uint32_t image_set, image_binding, sampler_set, sampler_binding;
  uint32_t consumer_stage_mask; /* 1=capture,2=body,3=both. */
  uint32_t binding_mode; /* 1=dynamic SAMPLER_WITH_TEXTURE,2=separate sampler/texture. */
  uint64_t reserved;
  GDExtensionQualificationTextureCaptureV2 capture;
} GDExtensionQualificationTexturePlanV2;

typedef struct { /* 176 bytes. Base+plans reuse original checked upload path. */
  GDExtensionDeviceExitAbi abi;
  GDExtensionQualificationUploadPrepareV1 base;
  uint32_t texture_plan_count; /* Exactly1. */
  uint32_t reserved;
} GDExtensionQualificationTexturePrepareV2;

typedef struct { /* 56 bytes. No bytes, version or native handles supplied. */
  GDExtensionDeviceExitAbi abi;
  GDExtensionQualificationTicketRefV1 ticket;
  uint64_t expected_report_revision;
  uint32_t texture_plan_index, reserved; /* index=0 */
  uint64_t transfer_attempt; /* nonzero; same exact retry returns same content. */
} GDExtensionQualificationTextureTransferV2;

typedef struct { /* 304 bytes; CORE-issued snapshot, never caller authority. */
  GDExtensionQualificationContentRefV2 content;
  GDExtensionQualificationStorageIdentityV2 image, sampler;
  GDExtensionQualificationTextureRegionV2 region;
  GDExtensionQualificationTicketRefV1 producer_ticket;
  uint64_t content_version;
  uint32_t source_upload_index, texture_plan_index;
  GDExtensionQualificationTextureCaptureV2 capture;
} GDExtensionQualificationTextureSourceUseV2;

typedef struct { /* 664 bytes, align8. Read-only original record snapshot. */
  GDExtensionDeviceExitAbi abi;
  GDExtensionQualificationRecordV1 base;
  GDExtensionQualificationTicketRefV1 ticket;
  GDExtensionQualificationTextureSourceUseV2 source;
  uint64_t record_admission_revision, reserved;
} GDExtensionQualificationTextureRecordSnapshotV2;

typedef struct { /* 472 bytes, align8. Original ticket snapshot, not new actor. */
  GDExtensionDeviceExitAbi abi;
  GDExtensionQualificationContentRefV2 content;
  GDExtensionQualificationTicketRefV1 producer_ticket, consumer_ticket;
  uint64_t content_version, receipt_revision, ticket_report_revision;
  uint32_t clear_fact, transfer_fact; /* UINT32_MAX when not yet appended. */
  uint32_t capture_fact, body_fact; /* UINT32_MAX when not yet appended. */
  uint32_t dependency_count, state;
  int32_t native_error;
  uint32_t reserved32;
  uint8_t payload_digest[32]; /* Actual original checked source upload digest. */
  GDExtensionQualificationStorageIdentityV2 image;
  GDExtensionQualificationTextureRegionV2 region;
  GDExtensionQualificationStorageIdentityV2 sampler, source_buffer;
  uint32_t source_upload_index, texture_plan_index;
  uint32_t unresolved_count, reserved;
  GDExtensionQualificationTextureCaptureV2 capture;
} GDExtensionQualificationTextureReceiptV2;

typedef struct { /* 48 bytes. expected revision must match original report. */
  GDExtensionDeviceExitAbi abi;
  GDExtensionQualificationTicketRefV1 ticket;
  uint64_t expected_report_revision, reserved;
} GDExtensionQualificationTextureReceiptQueryV2;

typedef struct { /* 64 bytes. Paged from original ticket dependency vector. */
  GDExtensionDeviceExitAbi abi;
  uint32_t before_fact, after_fact;
  uint32_t mode; /* 1=actual current-graph path,2=completed earlier native sources. */
  uint32_t state; /* 0=pending,1=validated,2=failed,3=unknown. */
  uint64_t ticket_report_revision, before_frame_epoch, after_frame_epoch;
  uint32_t path_length, source_native_count;
} GDExtensionQualificationTextureDependencyV2;

typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationTextureLayoutV2)(
    GDExtensionQualificationBridgeRefV1, const GDExtensionStorageBackingQueryV2 *,
    GDExtensionQualificationTextureLayoutV2 *);
typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationTexturePrepareV2)(
    GDExtensionQualificationBridgeRefV1, const GDExtensionQualificationTexturePrepareV2 *,
    const GDExtensionQualificationUploadPlanV1 *, uint32_t upload_count,
    const GDExtensionQualificationTexturePlanV2 *, uint32_t texture_count,
    GDExtensionQualificationTicketRefV1 *, GDExtensionQualificationReportV1 *);
typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationTextureTransferV2)(
    GDExtensionQualificationBridgeRefV1, const GDExtensionQualificationTextureTransferV2 *,
    GDExtensionQualificationTextureReceiptV2 *);
typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationTextureRecordSnapshotV2)(
    GDExtensionQualificationBridgeRefV1, const GDExtensionQualificationTextureReceiptQueryV2 *,
    GDExtensionQualificationTextureRecordSnapshotV2 *);
typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationTextureReceiptV2)(
    GDExtensionQualificationBridgeRefV1, const GDExtensionQualificationTextureReceiptQueryV2 *,
    GDExtensionQualificationTextureReceiptV2 *);
typedef GDExtensionDeviceSessionResultV1
(*GDExtensionInterfaceDeviceSessionQualificationTextureDependenciesV2)(
    GDExtensionQualificationBridgeRefV1, GDExtensionQualificationTicketRefV1,
    uint64_t report_revision, uint32_t first, uint32_t capacity,
    GDExtensionQualificationTextureDependencyV2 *, uint32_t *written);

/* New registered names (above typedefs in matching order):
 * device_session_qualification_texture_layout_v2
 * device_session_qualification_texture_prepare_v2
 * device_session_qualification_texture_transfer_v2
 * device_session_qualification_texture_record_snapshot_v2 (owner read-only)
 * device_session_qualification_texture_receipt_v2
 * device_session_qualification_texture_dependencies_v2
 * Existing V1 upload/observe/facts/copy/cancel/release are the original ticket
 * ports. dependencies/receipt/record_snapshot are read-only snapshots.
 * EXISTING qualification_record_prepared_v1 / RT opcode9 remains the ONLY
 * consumer record entry. A typed ticket obtains its checked source solely
 * from the original transferred content stored in that same ticket/child.
 * No caller-specified version or second record dispatch is introduced.
 */
