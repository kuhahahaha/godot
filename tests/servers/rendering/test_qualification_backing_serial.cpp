#include "tests/test_macros.h"
#include "servers/rendering/rendering_device.h"
#include <algorithm>
#include <array>
#include <thread>
TEST_FORCE_LINK(test_qualification_backing_serial)
class TestQualificationSerialAccess {
public:
 static uint64_t take(RenderingDevice &device) { return device._reserve_backing_serial(); }
 static void exhaust_nearly(RenderingDevice &device) { device.next_backing_serial=UINT64_MAX-1; }
};
TEST_CASE("[RenderingDevice][Qualification] original backing serial saturates without reuse") {
 RenderingDevice device;
 TestQualificationSerialAccess::exhaust_nearly(device);
 CHECK(TestQualificationSerialAccess::take(device)==UINT64_MAX-1);
 CHECK(TestQualificationSerialAccess::take(device)==UINT64_MAX);
 CHECK(TestQualificationSerialAccess::take(device)==0);
 CHECK(TestQualificationSerialAccess::take(device)==0);
}
#ifdef THREADS_ENABLED
TEST_CASE("[RenderingDevice][Qualification] asynchronous storage creation reserves distinct original serials") {
 RenderingDevice device;
 std::array<uint64_t,4096> values{};
 std::array<std::thread,8> producers;
 for(size_t thread=0;thread<producers.size();++thread) producers[thread]=std::thread([&,thread] {
  for(size_t i=0;i<512;++i)values[thread*512+i]=TestQualificationSerialAccess::take(device);
 });
 for(auto &producer:producers)producer.join();
 std::sort(values.begin(),values.end());
 bool exact=true;for(size_t i=0;i<values.size();++i)exact=exact && values[i]==i+1;
 CHECK(exact);
 CHECK(TestQualificationSerialAccess::take(device)==4097);
}
#endif
