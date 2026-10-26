#pragma once

#include "core/os/mutex.h"
#include "core/templates/local_vector.h"
#include <cstdint>
#include <atomic>

class RenderingDevice;
class TestQualificationPhysicalAccess;
class TestQualificationAckAccess;
namespace GDExtensionDeviceExit {
class Registry;
struct QualificationTicket;
// One exact object return cell within an original Registry child. This is not
// a completion source: only the original RD queue and driver-free return write
// it. Its parent retains it through the owner's later acknowledgement.
class QualificationPhysicalReturn {
 friend class Registry;
 friend struct QualificationTicket;
 friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
 friend class ::TestQualificationPhysicalAccess;
 friend class ::TestQualificationAckAccess;
#endif
 BinaryMutex mutex;
 uint64_t bridge = 0;
 uint64_t child = 0;
 uint64_t rid = 0;
 uint64_t serial = 0;
 uint64_t frame_epoch = 0;
 uint64_t reserved_event = 0;
 uint64_t free_event = 0;
 uint32_t frame_slot = UINT32_MAX;
 bool enqueued = false;
 std::atomic<bool> use_closed{false};
 std::atomic<uint32_t> dependent_children{0};
 LocalVector<QualificationTicket *> uses;
};
}
