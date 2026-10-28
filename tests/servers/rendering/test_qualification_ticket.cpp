#include "tests/test_macros.h"
#include "core/extension/gdextension_qualification_internal.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "core/os/thread.h"
#include "servers/rendering/rendering_server.h"
#include <thread>
TEST_FORCE_LINK(test_qualification_ticket)

// These controls feed deliberately distinct source observations into the
// original ticket reducer. They do not issue Registry authority or certify a
// native dispatch; the actual Metal control remains separately required.
class TestQualificationTicketAccess {
public:
	static void continuation_capacity() {
		using namespace GDExtensionDeviceExit;
		struct Driver : TransferTestDriverBase {
			mutable uint32_t last_slots = 0;
			bool native_call_observation_supported() const override { return true; }
			uint32_t qualification_native_call_bound(uint32_t slots) const override { last_slots = slots; return slots; }
			uint64_t limit_get(Limit) override { return 65535; }
		} driver;
		RenderingDevice device; device.driver = &driver;
		auto *registry = memnew(Registry); registry->records[0].snapshot.library_generation = 1;
		Registry::QualificationBridge bridge;
		GDExtensionQualificationRecordV1 request = {};
		request.abi = {1, sizeof(GDExtensionDeviceExitAbi), 0};
		request.record_generation = request.request_nonce = request.library_generation = request.qualification_session_serial = 1;
		request.attempt = 1; request.sample_count = request.range_end = 4; request.body_groups_x = 1;
		request.source_allocation_generation = 1;
		const uint32_t push[4] = {0, 4, 0, 0}; memcpy(request.body_push, push, 16); memcpy(request.capture_push, push, 16);
		struct Case { uint32_t bytes, workers, expected_slots; Error result; };
		const Case cases[] = {
			{4, 2, 12, ERR_INVALID_DATA},
			{4, 1022, 4092, ERR_INVALID_DATA},
			{4, 1023, 0, ERR_PARAMETER_RANGE_ERROR},
			{4, UINT32_MAX, 0, ERR_PARAMETER_RANGE_ERROR},
			{16384, 5, 3090, ERR_INVALID_DATA},
			{16384, 6, 3605, ERR_PARAMETER_RANGE_ERROR},
		};
		for (const auto &c : cases) {
			CAPTURE(c.bytes); CAPTURE(c.workers);
			device.transfer_worker_pool_max_size = c.workers; request.readback_size = c.bytes;
			driver.last_slots = 0; LocalVector<RID> resources;
			CHECK(registry->qualification_record_preflight(&bridge, &device, request, resources) == c.result);
			if (c.expected_slots) CHECK(driver.last_slots == c.expected_slots);
			CHECK(resources.is_empty()); CHECK(device.draw_graph.qualification_command_count() == 0);
		}
		// Invalid RID is deliberately later than capacity: no fake pipeline or
		// production grant is needed to observe preappend slot-budget admission.
		device.driver = nullptr; memdelete(registry);
	}

	static void unknown_native_error(uint32_t mode = 0) {
		using namespace GDExtensionDeviceExit;
		using RDD = RenderingDeviceDriver;
		struct Driver : TransferTestDriverBase {
			bool pending = false, finished = false;
			Error native_call_retain(uint64_t serial) override { CHECK(serial == 17); return OK; }
			FencePollResult native_call_poll(uint64_t serial) override {
				CHECK(serial == 17);
				FencePollResult out; out.completed_serial = serial;
				out.native_error = !pending; out.native_code = pending ? 0 : -1;
				out.completed = pending && finished; return out;
			}
		} driver;
		driver.pending = mode == 2;
		RenderingDevice device; device.driver = &driver; device.frames.resize(1);
		QualificationTicket ticket; ticket.report.revision = 1; ticket.frame_refs = 1;
		RDD::NativeCallFact source; source.serial = source.completion_serial = 17; source.queue_index = 3;
		source.issue = mode == 1 ? RDD::IssueFact::KnownAccepted : RDD::IssueFact::IssuedUnknown;
		source.error = mode == 2 ? OK : FAILED; source.native_code = mode == 2 ? 0 : -1;
		GDExtensionQualificationFactV1 fact = {}; fact.issue = uint32_t(source.issue); fact.completion = 1;
		fact.native_serial = fact.completion_serial = 17; fact.backend_queue = 3;
		ticket.facts.push_back(fact); ticket.natives.push_back({source, 0, 0, 0, 1, false, false});
		if (mode == 3) {
			auto failed_source = source; failed_source.serial = failed_source.completion_serial = 18;
			failed_source.issue = RDD::IssueFact::KnownAccepted;
			auto failed_fact = fact; failed_fact.issue = 1; failed_fact.completion = 3; failed_fact.error = FAILED;
			ticket.facts.push_back(failed_fact); ticket.natives.push_back({failed_source, 1, 0, 0, 1, true, false});
		}
		device.frames[0].qualification_tickets.push_back(&ticket);
		RenderingDevice::Buffer buffer; buffer.qualification_users = 1;
		RID rid = device.storage_buffer_owner.make_rid(buffer); ticket.resource_pins.push_back(rid);
		CHECK(device._qualification_poll_tickets(0) == (mode == 2 ? ERR_BUSY : FAILED));
		CHECK(ticket.facts[0].issue == (mode == 1 ? 1 : 2));
		CHECK(ticket.facts[0].completion == (mode == 2 ? 1 : mode == 1 ? 3 : 4));
		CHECK(ticket.facts[0].native_code == (mode == 2 ? 0 : -1));
		CHECK(ticket.report.native_code == (mode == 2 ? 0 : -1));
		CHECK(ticket.report.possible_count == (mode == 1 ? 0 : 1));
		CHECK(ticket.report.known_count == (mode == 1 || mode == 3 ? 1 : 0));
		CHECK(ticket.report.state == (mode == 1 ? 7 : mode == 2 ? 4 : 6));
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		CHECK_FALSE(ticket.natives[0].owner_released);
		if (mode == 2) {
			driver.finished = true;
			CHECK(device._qualification_poll_tickets(0) == OK);
			CHECK(ticket.facts[0].issue == 1); CHECK(ticket.facts[0].completion == 2);
			CHECK(ticket.report.possible_count == 0); CHECK(ticket.report.known_count == 1);
			CHECK_FALSE(ticket.physically_retired.load());
		}
		// Synthetic reducer inputs own no GPU work. This is fixture cleanup,
		// never an acknowledgement or proof that an Unknown may retire.
		device.frames.clear(); device.storage_buffer_owner.free(rid); device.driver = nullptr;
	}

	static void no_effect_reacquires_resource_pin(bool p_attempted = false) {
		using namespace GDExtensionDeviceExit;
		RenderingDevice device;
		TransferTestDriverBase driver;
		device.driver = &driver;
		QualificationTicket ticket;
		ticket.prepared = true;
		ticket.report.revision = 1;
		ticket.resource_pins.reserve(128);
		const auto reserved_capacity = ticket.resource_pins.get_capacity();
		RenderingDevice::Buffer buffer;
		RID rid = device.storage_buffer_owner.make_rid(buffer);
		LocalVector<RID> resources;
		resources.push_back(rid);
		REQUIRE(device._qualification_pin_resources(&ticket, resources) == OK);
		CHECK(device._qualification_resource_busy(rid));
		auto &registry = Registry::get_singleton();
		if (p_attempted) {
			ticket.uploads.resize(1);
			ticket.uploads[0].attempted = true;
		}
		// The original reducer returns a zero-effect upload's physical pins
		// while its prepared ticket can still admit the same upload later.
		registry.qualification_ticket_publish(&ticket, &device);
		CHECK(ticket.physically_retired.load() == !p_attempted);
		CHECK_FALSE(ticket.admission_closed.load());
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == (p_attempted ? 1 : 0));
		CHECK(device._qualification_resource_busy(rid) == p_attempted);
		if (p_attempted) {
			ticket.admission_closed = true;
			registry.qualification_ticket_publish(&ticket, &device);
			CHECK(ticket.physically_retired.load());
			CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 0);
		}
		if (p_attempted) {
			device.storage_buffer_owner.free(rid);
			device.driver = nullptr;
			return;
		}
		CHECK_FALSE(device._qualification_resource_busy(rid));
		REQUIRE(device._qualification_pin_resources(&ticket, resources) == OK);
		CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 1);
		CHECK(device._qualification_resource_busy(rid));
		CHECK_FALSE(ticket.physically_retired.load());
		CHECK(ticket.resource_pins.get_capacity() == reserved_capacity);
		// Avoid invoking a known unbalanced release in the RED control.
		if (device.storage_buffer_owner.get_or_null(rid)->qualification_users) {
			registry.qualification_ticket_publish(&ticket, &device);
			registry.qualification_ticket_publish(&ticket, &device);
			CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users == 0);
			CHECK_FALSE(device._qualification_resource_busy(rid));
			CHECK(ticket.resource_pins.get_capacity() == reserved_capacity);
		}
		device.storage_buffer_owner.free(rid);
		device.driver = nullptr;
	}
#ifdef THREADS_ENABLED
 static void contended_continuation() {
  using namespace GDExtensionDeviceExit;
  struct Driver:TransferTestDriverBase {int waits=0;Error fence_wait(FenceID)override{++waits;return ERR_TIMEOUT;}} driver;
  RenderingDevice device;device.driver=&driver;device.frames.resize(2);device.frames_drawn=10;device.frames[0].qualification_epoch=1;
  device.draw_graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},2,RenderingDeviceDriver::CommandQueueFamilyID(1),0);device.draw_graph.begin();
  auto &staging=device.download_staging_buffers;staging.blocks.resize(1);staging.block_size=staging.max_size=64;staging.blocks.write[0].driver_id=RenderingDeviceDriver::BufferID(51);staging.blocks.write[0].frame_used=10;staging.blocks.write[0].fill_amount=32;
  RenderingDevice::Buffer buffer;buffer.driver_id=RenderingDeviceDriver::BufferID(71);buffer.size=64;buffer.backing_serial=11;buffer.draw_tracker=RenderingDeviceGraph::resource_tracker_create();buffer.draw_tracker->buffer_driver_id=buffer.driver_id;RID rid=device.storage_buffer_owner.make_rid(buffer);
  QualificationTicket ticket;ticket.recorded=true;ticket.report.revision=1;ticket.origin.readback_buffer=rid.get_id();ticket.origin.readback_size=64;ticket.result.instantiate();ticket.result->qualification_owned=true;ticket.result->bytes.resize(64);
  auto &registry=Registry::get_singleton();std::atomic<bool> locked=false,release=false;
  std::thread holder([&]{device._thread_safe_.lock();locked=true;while(!release.load())std::this_thread::yield();device._thread_safe_.unlock();});
  while(!locked.load())std::this_thread::yield();
  registry.qualification_ticket_step(&ticket,&device);
  CHECK(ticket.result->issued_size==0);CHECK(ticket.result->copies.is_empty());CHECK(device.frames[0].download_buffer_get_data_requests.is_empty());CHECK(ticket.facts.is_empty());CHECK(ticket.frame_refs==0);
  release=true;holder.join();
  registry.qualification_ticket_step(&ticket,&device);
  CHECK(ticket.result->issued_size==32);CHECK(ticket.result->copies.size()==1);CHECK(ticket.frame_refs==1);CHECK_FALSE(ticket.physically_retired.load());
  staging.blocks.write[0].frame_used=0;
  registry.qualification_ticket_step(&ticket,&device);
  CHECK(ticket.result->issued_size==64);CHECK(ticket.result->copies.size()==2);CHECK(ticket.frame_refs==1);CHECK(ticket.facts.size()==2);CHECK(driver.waits==0);
  if(ticket.result->copies.size()==2){CHECK(ticket.result->copies[0].region.src_offset==0);CHECK(ticket.result->copies[1].region.src_offset==32);CHECK(ticket.result->copies[1].region.size==32);}
  RenderingDeviceGraph::resource_tracker_free(buffer.draw_tracker);device.storage_buffer_owner.free(rid);device.frames.clear();device.download_staging_buffers.blocks.clear();device.driver=nullptr;
 }
#endif
 static void reduction(bool copy_failure) {
  using namespace GDExtensionDeviceExit;
  RenderingDevice device;TransferTestDriverBase driver;device.driver=&driver;
  QualificationTicket ticket;ticket.report.revision=1;ticket.origin.readback_size=4;
  ticket.recorded=true;ticket.ordering_closed=true;ticket.result.instantiate();
  ticket.result->qualification_owned=true;ticket.result->bytes.resize(4);ticket.result->issued_size=4;
  RenderingDevice::Buffer buffer;buffer.qualification_users=1;RID rid=device.storage_buffer_owner.make_rid(buffer);ticket.resource_pins.push_back(rid);
  GDExtensionQualificationFactV1 graph={};graph.semantic_kind=3;graph.command_slot=0;ticket.facts.push_back(graph);
  ticket.graphs.push_back({{},0,0,1,true,false});ticket.frame_refs=1;
  for(uint32_t i=0;i<2;++i) {
   RenderingDeviceDriver::NativeCallFact source;source.serial=17+i;source.completion_serial=73+i;source.queue_index=1;
   GDExtensionQualificationFactV1 fact={};fact.issue=1;fact.completion=i?1:2;
   ticket.natives.push_back({source,ticket.facts.size(),0,0,1,true,false});ticket.facts.push_back(fact);
  }
  auto &registry=Registry::get_singleton();
  registry.qualification_ticket_publish(&ticket,&device);
  CHECK(ticket.facts[0].native_serial==17);CHECK(ticket.facts[0].completion_serial==73);
  CHECK_FALSE(ticket.physically_retired.load());CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users==1);
  ticket.facts[2].completion=2;ticket.observation_dirty=true;
  if(copy_failure)ticket.result->error=ERR_CANT_ACQUIRE_RESOURCE;
  else {ticket.result->completed_size=4;ticket.result->delivered=true;}
  registry.qualification_ticket_publish(&ticket,&device);
  CHECK_FALSE(ticket.physically_retired.load());CHECK(ticket.report.owned_size==0);
  ticket.graphs[0].returned=true;ticket.frame_refs=0;
  registry.qualification_ticket_publish(&ticket,&device);
  CHECK(ticket.physically_retired.load());CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users==0);
  if(copy_failure){CHECK(ticket.report.state==5);CHECK(ticket.report.owned_size==0);CHECK(ticket.report.native_error==ERR_CANT_ACQUIRE_RESOURCE);CHECK(ticket.admission_closed.load());}
  else {CHECK(ticket.report.state==8);CHECK(ticket.report.owned_size==4);}
  device.storage_buffer_owner.free(rid);device.driver=nullptr;
 }
 static void owner_stop() {
  using namespace GDExtensionDeviceExit;
  auto *registry=memnew(Registry);RenderingDevice device;auto *previous=RenderingDevice::singleton;RenderingDevice::singleton=&device;device.is_main_instance=true;
  registry->instance=71;auto &record=registry->records[0];record.owner_thread=Thread::get_caller_id();record.snapshot.library_generation=1;record.private_refs=registry->private_limit;
  registry->device_records[0].object=&device;registry->device_records[0].live=true;registry->device_records[0].epoch=1;
  struct Holder:HolderRetirement{Holder():HolderRetirement(RecordRef{}){} ~Holder()=default;};Holder holder;holder.generation=1;
  auto *bridge=memnew(Registry::QualificationBridge);bridge->serial=11;bridge->epoch=1;bridge->holder=&holder;bridge->children=1;registry->qualification_bridges[0]=bridge;
  auto *ticket=memnew(QualificationTicket);ticket->serial=23;ticket->bridge_serial=11;ticket->report.revision=1;ticket->physically_retired=true;bridge->tickets[0]=ticket;
  auto ordinary=registry->qualification_cancel({71,11},{71,23});CHECK(ordinary.native_error==ERR_BUSY);CHECK(ordinary.reason==16);CHECK_FALSE(ticket->stop_requested);
  ticket->prepared=true;ticket->report.state=8;auto complete=registry->qualification_cancel({71,11},{71,23});CHECK(complete.native_error==ERR_BUSY);CHECK(complete.reason==16);CHECK(ticket->report.state==8);CHECK_FALSE(ticket->stop_requested);ticket->report.state=0;
#ifdef THREADS_ENABLED
  GDExtensionDeviceSessionResultV1 foreign={};std::thread other([&]{foreign=registry->qualification_cancel({71,11},{71,23});});other.join();CHECK(foreign.native_error==ERR_UNAUTHORIZED);CHECK(foreign.reason==15);CHECK_FALSE(ticket->stop_requested);
#endif
  auto stale=registry->qualification_cancel({71,11},{});CHECK(stale.native_error==ERR_DOES_NOT_EXIST);CHECK(stale.reason==17);
  auto accepted=registry->qualification_cancel({71,11},{71,23});CHECK(accepted.native_error==OK);CHECK(ticket->stop_requested);CHECK(ticket->admission_closed.load());CHECK_FALSE(ticket->physically_retired.load());CHECK_FALSE(ticket->queued);CHECK(bridge->children==1);CHECK(record.private_refs==registry->private_limit);
  CHECK(registry->qualification_cancel({71,11},{71,23}).native_error==OK);
  registry->qualification_ticket_publish(ticket,&device);CHECK(ticket->report.state==7);CHECK(ticket->report.phase==6);CHECK(ticket->report.owned_size==0);CHECK(ticket->report.required_count==0);CHECK(ticket->physically_retired.load());
  const uint64_t revision=ticket->report.revision;CHECK(registry->qualification_cancel({71,11},{71,23}).native_error==OK);CHECK(ticket->report.revision==revision);
  CHECK(registry->qualification_release({71,11},{71,23}).native_error==OK);CHECK(bridge->children==0);CHECK(record.private_refs==registry->private_limit-1);CHECK(registry->qualification_cancel({71,11},{71,23}).reason==17);
  RenderingDevice::singleton=previous;device.is_main_instance=false;registry->qualification_bridges[0]=nullptr;memdelete(ticket);memdelete(bridge);memdelete(registry);
 }
 static void unknown_keeps_pin() {
  using namespace GDExtensionDeviceExit;
  RenderingDevice device;TransferTestDriverBase driver;device.driver=&driver;QualificationTicket ticket;
  ticket.report.revision=1;ticket.append_failed=true;ticket.mapping_failed=true;
  RenderingDevice::Buffer buffer;buffer.qualification_users=1;RID rid=device.storage_buffer_owner.make_rid(buffer);ticket.resource_pins.push_back(rid);
  Registry::get_singleton().qualification_ticket_publish(&ticket,&device);
  CHECK_FALSE(ticket.physically_retired.load());CHECK(ticket.report.state==6);CHECK(ticket.report.owned_size==0);CHECK(device.storage_buffer_owner.get_or_null(rid)->qualification_users==1);
  device.storage_buffer_owner.free(rid);device.driver=nullptr;
 }
};
TEST_CASE("[Qualification] ticket keeps distinct native and completion identities until every source and frame returns") {TestQualificationTicketAccess::reduction(false);}
TEST_CASE("[Qualification] zero-effect prepared ticket reacquires original resource pins before retry") { TestQualificationTicketAccess::no_effect_reacquires_resource_pin(); }
TEST_CASE("[Qualification] attempted upload keeps original writer pin across zero-effect busy publish") { TestQualificationTicketAccess::no_effect_reacquires_resource_pin(true); }
TEST_CASE("[Qualification] settled native work releases resource pins after a logical readback copy failure") {TestQualificationTicketAccess::reduction(true);}
TEST_CASE("[Qualification] unresolved source mapping keeps failed ticket resource pins") {TestQualificationTicketAccess::unknown_keeps_pin();}

TEST_CASE("[Qualification] owner stop is opt in exact ticket monotone and preserves immutable Complete") {TestQualificationTicketAccess::owner_stop();}

#ifdef THREADS_ENABLED
TEST_CASE("[Qualification] ticket continuation refuses contended RD before graph and resumes the same suffix") {TestQualificationTicketAccess::contended_continuation();}
#endif

TEST_CASE("[Qualification] unknown submit error preserves original issue possible set and resource obligations") { TestQualificationTicketAccess::unknown_native_error(); }

TEST_CASE("[Qualification] known accepted native error stays Failed and retains obligations") { TestQualificationTicketAccess::unknown_native_error(1); }
TEST_CASE("[Qualification] callback pending source stays pending and can become known successful") { TestQualificationTicketAccess::unknown_native_error(2); }
TEST_CASE("[Qualification] mixed Failed and Unknown sources preserve unresolved Unknown") { TestQualificationTicketAccess::unknown_native_error(3); }

TEST_CASE("[Qualification] continuation preflight reserves fresh hidden worker sources per graph frame") { TestQualificationTicketAccess::continuation_capacity(); }
