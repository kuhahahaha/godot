#pragma once
#include "resource_snapshot_bridge.h"

AngeCaptureStatusV1 ange_resource_content_revision_v1(void *, uint64_t *);
AngeCaptureStatusV1 ange_curve_snapshot_v1(void *, const double *, uint32_t, AngeCurvePointV1 *, uint32_t, double *, uint32_t, uint32_t *, uint32_t *, double *, double *, uint64_t *);
AngeCaptureStatusV1 ange_gradient_snapshot_v1(void *, const double *, uint32_t, AngeGradientStopV1 *, uint32_t, double *, uint32_t, uint32_t *, uint32_t *, int32_t *, int32_t *, uint64_t *);
void register_resource_snapshot_bridge();
