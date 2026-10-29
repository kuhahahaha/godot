#include <stdint.h>
typedef struct { uint32_t abi_version, struct_size; uint64_t reserved; } T1Abi;
typedef struct { uint64_t registry_instance, handle_serial; } T1Ref;
typedef struct {
 T1Abi abi; T1Ref ticket;
 uint64_t record_generation, request_nonce, library_generation, qualification_session_serial;
 uint64_t preparation_generation, operation_serial, parent_serial, reservation_serial;
 uint8_t request_digest[32], unit_digest[32];
 uint32_t vector_ordinal, attempt, binding_slot, allocation_slot;
 uint64_t allocation_generation;
} T1Origin;
typedef struct {
 T1Origin origin;
 uint32_t format, width, height, depth, layers, mip_count, samples, usage;
 uint32_t base_layer, base_mip;
 uint8_t payload_digest[32];
} T1TextureCreate;
typedef struct {
 T1Origin origin;
 uint32_t mag_filter, min_filter, mip_filter, repeat_u, repeat_v, repeat_w, flags, reserved;
 uint32_t lod_bias_bits, anisotropy_bits, min_lod_bits, max_lod_bits, compare_op, border_color;
} T1SamplerCreate;
typedef struct {
 T1Origin origin; T1Ref native_unit;
 uint64_t shader_serial;
 uint32_t set_index, member_count, payload_bytes, reserved;
 uint8_t map_digest[32];
} T1SetCreate;
typedef struct {
 T1Abi abi; T1Ref child;
 uint64_t object_serial, rid, operation_serial;
 uint32_t kind, state, known_mask, possible_mask;
 int32_t native_error; uint32_t phase;
 int64_t native_code;
} T1CreateReport;
typedef struct {
 T1Abi abi; T1Ref child;
 uint64_t retire_operation, object_serial, rid, frame_epoch, free_event, dependency_serial, revision;
 uint32_t kind, state, return_status, reason, frame_slot, reserved;
 int32_t native_error; uint32_t reserved2;
 int64_t native_code;
} T1RetireReport;
typedef struct {
 T1Abi abi; T1Ref child;
 uint64_t object_serial, graph_instance, graph_frame, source_begin, source_end;
 uint32_t graph_kind, graph_index, graph_offset, segment_index, mip, layer;
 uint32_t x, y, width, height, row_pitch, reserved;
} T1TextureSegment;
