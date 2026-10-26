#include "tests/test_macros.h"
#include "core/templates/command_queue_mt.h"
#include <atomic>
#include <chrono>
#include <thread>

TEST_FORCE_LINK(test_command_queue_bounded)

namespace TestCommandQueueBounded {
class Access {
public:
 static void lock(CommandQueueMT &queue) { queue.mutex.lock(); }
 static void unlock(CommandQueueMT &queue) { queue.mutex.unlock(); }
};
struct Target {
 uint64_t values[4] = {};
 uint32_t count = 0;
 void call(uint64_t value) { if(count<4)values[count++]=value; }
};
TEST_CASE("[CommandQueue][Qualification] full bounded enqueue has no effect and retry preserves FIFO") {
 CommandQueueMT queue;
 Target target;
 CHECK_FALSE(queue.try_push_bounded(0,&target,&Target::call,uint64_t(77)));
 queue.flush_all();
 CHECK(target.count==0);
 REQUIRE(queue.try_push_bounded(64,&target,&Target::call,uint64_t(11)));
 CHECK_FALSE(queue.try_push_bounded(64,&target,&Target::call,uint64_t(22)));
 queue.flush_all();
 REQUIRE(target.count==1);
 CHECK(target.values[0]==11);
 REQUIRE(queue.try_push_bounded(64,&target,&Target::call,uint64_t(22)));
 queue.push(&target,&Target::call,uint64_t(33));
 queue.flush_all();
 REQUIRE(target.count==3);
 CHECK(target.values[1]==22);
 CHECK(target.values[2]==33);
}

#ifdef THREADS_ENABLED
TEST_CASE("[CommandQueue][Qualification] contended bounded enqueue refuses without waiting or publication") {
 CommandQueueMT queue; Target target;
 std::atomic<bool> returned=false;
 bool accepted=true;
 Access::lock(queue);
 std::thread producer([&]{ accepted=queue.try_push_bounded(64,&target,&Target::call,uint64_t(42));returned.store(true); });
 const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(1);
 while(!returned.load() && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
 const bool refused_before_unlock=returned.load();
 Access::unlock(queue);producer.join();
 CHECK(refused_before_unlock);
 CHECK_FALSE(accepted);
 queue.flush_all();CHECK(target.count==0);
}
#endif
}
