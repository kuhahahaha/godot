#pragma once
#include "resource_snapshot_bridge.h"

AngeCaptureStatusV1 ange_resource_content_revision_v1(void *, uint64_t *);
AngeCaptureStatusV1 ange_curve_snapshot_v1(void *, const double *, uint32_t, AngeCurvePointV1 *, uint32_t, double *, uint32_t, uint32_t *, uint32_t *, double *, double *, uint64_t *);
AngeCaptureStatusV1 ange_gradient_snapshot_v1(void *, const double *, uint32_t, AngeGradientStopV1 *, uint32_t, double *, uint32_t, uint32_t *, uint32_t *, int32_t *, int32_t *, uint64_t *);
uint32_t ange_texture_snapshot_plan_v1(void *, uint32_t, AngeTextureSnapshotPlanV1 *);
uint32_t ange_texture_snapshot_v1(void *, const AngeTextureSnapshotPlanV1 *, uint8_t *, uint64_t, void *, uint64_t, AngeTextureSnapshotResultV1 *);
void register_resource_snapshot_bridge();
