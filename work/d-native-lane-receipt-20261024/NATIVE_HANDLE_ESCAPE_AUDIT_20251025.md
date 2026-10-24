# Native Handle Escape Audit

Date: 2026-10-25

Audited `RenderingDevice::get_driver_resource()` and the capture-test escape
tracking boundary. Logical-device, queue, topmost-object, and uniform-set
exports conservatively mark device-wide native access escaped; buffer exports
mark the exact allocation escaped after owner validation. Invalid RID paths
fail closed before returning a handle.

Texture, sampler, and pipeline handles do not map into the buffer allocation
registry; treating them as buffer allocation evidence would require a broader
resource ownership ledger. No safe owner-attributed production patch was
identified in this slice.

This is source/static evidence only. The distinction limits qualification
coverage and cannot raise runtime/native/GPU/PCK/platform/performance/
production flags. Full build remains blocked by the known disk-space failure.
