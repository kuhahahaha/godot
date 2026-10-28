#ifndef ANGE_RESOURCE_SNAPSHOT_BRIDGE_V1_H
#define ANGE_RESOURCE_SNAPSHOT_BRIDGE_V1_H

/* Godot-core / ANGE host adapter seam.
 * Curve/gradient and E22 texture snapshot exports are observational source
 * reads on a retained original object; they do not mint owner authority or
 * E20 transaction receipts.
 * Supported layout target: C11/C++17 on arm64 macOS with 8-byte aligned
 * IEEE-754 binary64 double and the matching Godot/GDExtension calling ABI.
 * E22 scratch real_t width is carried explicitly by its plan (4 or 8 bytes).
 * Other platform ABIs require their own layout/call verification. */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ANGE_CURVE_SNAPSHOT_PROC_V1 "ange_curve_snapshot_v1"
#define ANGE_GRADIENT_SNAPSHOT_PROC_V1 "ange_gradient_snapshot_v1"
#define ANGE_RESOURCE_CONTENT_REVISION_PROC_V1 "ange_resource_content_revision_v1"

/* E22 direct-final r3 planning ABI. Godot core registers these exports from
 * the original retained CurveTexture/GradientTexture1D object path. */
#define ANGE_TEXTURE_SNAPSHOT_PLAN_PROC_V1 "ange_texture_snapshot_plan_v1"
#define ANGE_TEXTURE_SNAPSHOT_PROC_V1 "ange_texture_snapshot_v1"

typedef uint32_t AngeCaptureStatusV1;
enum {
    ANGE_CAPTURE_OK_V1 = 0,
    ANGE_CAPTURE_WRONG_TYPE_V1 = 1,
    ANGE_CAPTURE_WRONG_THREAD_V1 = 2,
    ANGE_CAPTURE_LIMIT_V1 = 3,
    ANGE_CAPTURE_BAD_ARGUMENT_V1 = 4,
    ANGE_CAPTURE_SOURCE_BUSY_V1 = 5
};

typedef struct AngeCurvePointV1 {
    double x, y, left, right;
    int32_t left_mode, right_mode;
} AngeCurvePointV1;

typedef struct AngeGradientStopV1 {
    double offset, r, g, b, a;
} AngeGradientStopV1;

/* source is an already retained live Godot Object pointer, obtained from the
 * original selected Ref. An arbitrary or dangling pointer is outside this
 * typed ABI. All calls run on the admitted Godot main thread. */
typedef AngeCaptureStatusV1 (*AngeResourceContentRevisionFnV1)(
    void *source, uint64_t *content_revision);

typedef AngeCaptureStatusV1 (*AngeCurveSnapshotFnV1)(
    void *source, const double *sample_x, uint32_t sample_count,
    AngeCurvePointV1 *points, uint32_t point_capacity,
    double *samples, uint32_t sample_capacity,
    uint32_t *points_written, uint32_t *samples_written,
    double *domain_min, double *domain_max, uint64_t *content_revision);

typedef AngeCaptureStatusV1 (*AngeGradientSnapshotFnV1)(
    void *source, const double *sample_x, uint32_t sample_count,
    AngeGradientStopV1 *stops, uint32_t stop_capacity,
    double *rgba_samples, uint32_t rgba_capacity,
    uint32_t *stops_written, uint32_t *rgba_written,
    int32_t *mode, int32_t *color_space, uint64_t *content_revision);

typedef struct AngeTextureSourceTupleV1 {
    uint64_t wrapper_id, wrapper_revision, child_id, child_revision;
    uint32_t source_kind, width, native_mode, child_present;
} AngeTextureSourceTupleV1;

typedef struct AngeTextureSnapshotPlanV1 {
    AngeTextureSourceTupleV1 source;
    uint32_t output_format, real_bytes, point_count, bake_resolution;
    uint64_t output_bytes, scratch_bytes;
} AngeTextureSnapshotPlanV1;

typedef struct AngeTextureSnapshotResultV1 {
    AngeTextureSourceTupleV1 source;
    uint32_t initialized_pixels, status;
} AngeTextureSnapshotResultV1;

typedef uint32_t (*AngeTextureSnapshotPlanFnV1)(
    void *retained_wrapper, uint32_t output_format,
    AngeTextureSnapshotPlanV1 *out);
typedef uint32_t (*AngeTextureSnapshotFnV1)(
    void *retained_wrapper, const AngeTextureSnapshotPlanV1 *expected,
    uint8_t *output, uint64_t output_capacity,
    void *scratch, uint64_t scratch_capacity,
    AngeTextureSnapshotResultV1 *out);

enum {
    ANGE_TEXTURE_SOURCE_CURVE_V1 = 1,
    ANGE_TEXTURE_SOURCE_GRADIENT_1D_V1 = 2,
    ANGE_TEXTURE_FORMAT_RGBAF_V1 = 1,
    ANGE_TEXTURE_FORMAT_RGBA8_V1 = 2,
    ANGE_TEXTURE_CURVE_RGB_V1 = 1,
    ANGE_TEXTURE_CURVE_RED_V1 = 2,
    ANGE_TEXTURE_GRADIENT_HDR_V1 = 1,
    ANGE_TEXTURE_GRADIENT_LDR_V1 = 2,
    ANGE_TEXTURE_OK_V1 = 0,
    ANGE_TEXTURE_WRONG_TYPE_V1 = 1,
    ANGE_TEXTURE_WRONG_THREAD_V1 = 2,
    ANGE_TEXTURE_LIMIT_V1 = 3,
    ANGE_TEXTURE_BAD_ARGUMENT_V1 = 4,
    ANGE_TEXTURE_STALE_SOURCE_V1 = 5,
    ANGE_TEXTURE_INVALID_SOURCE_V1 = 6,
    ANGE_TEXTURE_UNSUPPORTED_V1 = 7
};

#ifdef __cplusplus
}
static_assert(sizeof(AngeCurvePointV1) == 40 && alignof(AngeCurvePointV1) == 8);
static_assert(offsetof(AngeCurvePointV1, x) == 0);
static_assert(offsetof(AngeCurvePointV1, y) == 8);
static_assert(offsetof(AngeCurvePointV1, left) == 16);
static_assert(offsetof(AngeCurvePointV1, right) == 24);
static_assert(offsetof(AngeCurvePointV1, left_mode) == 32);
static_assert(offsetof(AngeCurvePointV1, right_mode) == 36);
static_assert(sizeof(AngeGradientStopV1) == 40 && alignof(AngeGradientStopV1) == 8);
static_assert(offsetof(AngeGradientStopV1, offset) == 0);
static_assert(offsetof(AngeGradientStopV1, r) == 8);
static_assert(offsetof(AngeGradientStopV1, g) == 16);
static_assert(offsetof(AngeGradientStopV1, b) == 24);
static_assert(offsetof(AngeGradientStopV1, a) == 32);
static_assert(sizeof(AngeTextureSourceTupleV1) == 48 && alignof(AngeTextureSourceTupleV1) == 8);
static_assert(offsetof(AngeTextureSourceTupleV1, wrapper_id) == 0);
static_assert(offsetof(AngeTextureSourceTupleV1, wrapper_revision) == 8);
static_assert(offsetof(AngeTextureSourceTupleV1, child_id) == 16);
static_assert(offsetof(AngeTextureSourceTupleV1, child_revision) == 24);
static_assert(offsetof(AngeTextureSourceTupleV1, source_kind) == 32);
static_assert(offsetof(AngeTextureSourceTupleV1, width) == 36);
static_assert(offsetof(AngeTextureSourceTupleV1, native_mode) == 40);
static_assert(offsetof(AngeTextureSourceTupleV1, child_present) == 44);
static_assert(sizeof(AngeTextureSnapshotPlanV1) == 80 && alignof(AngeTextureSnapshotPlanV1) == 8);
static_assert(offsetof(AngeTextureSnapshotPlanV1, source) == 0);
static_assert(offsetof(AngeTextureSnapshotPlanV1, output_format) == 48);
static_assert(offsetof(AngeTextureSnapshotPlanV1, real_bytes) == 52);
static_assert(offsetof(AngeTextureSnapshotPlanV1, point_count) == 56);
static_assert(offsetof(AngeTextureSnapshotPlanV1, bake_resolution) == 60);
static_assert(offsetof(AngeTextureSnapshotPlanV1, output_bytes) == 64);
static_assert(offsetof(AngeTextureSnapshotPlanV1, scratch_bytes) == 72);
static_assert(sizeof(AngeTextureSnapshotResultV1) == 56 && alignof(AngeTextureSnapshotResultV1) == 8);
static_assert(offsetof(AngeTextureSnapshotResultV1, source) == 0);
static_assert(offsetof(AngeTextureSnapshotResultV1, initialized_pixels) == 48);
static_assert(offsetof(AngeTextureSnapshotResultV1, status) == 52);
#else
_Static_assert(sizeof(AngeCurvePointV1) == 40 && _Alignof(AngeCurvePointV1) == 8,
               "Curve snapshot ABI layout");
_Static_assert(offsetof(AngeCurvePointV1, x) == 0, "Curve x offset");
_Static_assert(offsetof(AngeCurvePointV1, y) == 8, "Curve y offset");
_Static_assert(offsetof(AngeCurvePointV1, left) == 16, "Curve left offset");
_Static_assert(offsetof(AngeCurvePointV1, right) == 24, "Curve right offset");
_Static_assert(offsetof(AngeCurvePointV1, left_mode) == 32, "Curve mode offset");
_Static_assert(offsetof(AngeCurvePointV1, right_mode) == 36, "Curve mode offset");
_Static_assert(sizeof(AngeGradientStopV1) == 40 && _Alignof(AngeGradientStopV1) == 8,
               "Gradient snapshot ABI layout");
_Static_assert(offsetof(AngeGradientStopV1, offset) == 0, "Gradient offset offset");
_Static_assert(offsetof(AngeGradientStopV1, r) == 8, "Gradient r offset");
_Static_assert(offsetof(AngeGradientStopV1, g) == 16, "Gradient g offset");
_Static_assert(offsetof(AngeGradientStopV1, b) == 24, "Gradient b offset");
_Static_assert(offsetof(AngeGradientStopV1, a) == 32, "Gradient alpha offset");
_Static_assert(sizeof(AngeTextureSourceTupleV1) == 48 && _Alignof(AngeTextureSourceTupleV1) == 8, "Texture tuple layout");
_Static_assert(offsetof(AngeTextureSourceTupleV1, wrapper_id) == 0, "Texture wrapper ID offset");
_Static_assert(offsetof(AngeTextureSourceTupleV1, wrapper_revision) == 8, "Texture wrapper revision offset");
_Static_assert(offsetof(AngeTextureSourceTupleV1, child_id) == 16, "Texture child ID offset");
_Static_assert(offsetof(AngeTextureSourceTupleV1, child_revision) == 24, "Texture child revision offset");
_Static_assert(offsetof(AngeTextureSourceTupleV1, source_kind) == 32, "Texture source kind offset");
_Static_assert(offsetof(AngeTextureSourceTupleV1, width) == 36, "Texture width offset");
_Static_assert(offsetof(AngeTextureSourceTupleV1, native_mode) == 40, "Texture mode offset");
_Static_assert(offsetof(AngeTextureSourceTupleV1, child_present) == 44, "Texture child presence offset");
_Static_assert(sizeof(AngeTextureSnapshotPlanV1) == 80 && _Alignof(AngeTextureSnapshotPlanV1) == 8, "Texture plan layout");
_Static_assert(offsetof(AngeTextureSnapshotPlanV1, source) == 0, "Texture plan source offset");
_Static_assert(offsetof(AngeTextureSnapshotPlanV1, output_format) == 48, "Texture plan format offset");
_Static_assert(offsetof(AngeTextureSnapshotPlanV1, real_bytes) == 52, "Texture plan real width offset");
_Static_assert(offsetof(AngeTextureSnapshotPlanV1, point_count) == 56, "Texture plan point count offset");
_Static_assert(offsetof(AngeTextureSnapshotPlanV1, bake_resolution) == 60, "Texture plan bake resolution offset");
_Static_assert(offsetof(AngeTextureSnapshotPlanV1, output_bytes) == 64, "Texture plan output bytes offset");
_Static_assert(offsetof(AngeTextureSnapshotPlanV1, scratch_bytes) == 72, "Texture plan scratch bytes offset");
_Static_assert(sizeof(AngeTextureSnapshotResultV1) == 56 && _Alignof(AngeTextureSnapshotResultV1) == 8, "Texture result layout");
_Static_assert(offsetof(AngeTextureSnapshotResultV1, source) == 0, "Texture result source offset");
_Static_assert(offsetof(AngeTextureSnapshotResultV1, initialized_pixels) == 48, "Texture result pixel count offset");
_Static_assert(offsetof(AngeTextureSnapshotResultV1, status) == 52, "Texture result status offset");
#endif

#endif
