#include <cstdio>
#include <cstdint>
#include <deque>
#include <memory>
enum class State : uint8_t { EMPTY, LIVE, ENQUEUED, RETURNED, UNKNOWN };
struct Cell {
	public:
		uint64_t device = 0;
		uint64_t rid = 0;
		uint64_t serial = 0;
		uint64_t backing = 0;
		State state = State::LIVE;
		// Both the actual RD resource/pending-free entry and the observer retain
		// this cell. No pointer into K storage or DSO callback survives here.
		bool matches(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) const {
			return rid && serial && backing && rid == p_rid && serial == p_serial && backing == p_backing;
		}
		bool enqueue(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
			if (state != State::LIVE || !matches(p_rid, p_serial, p_backing)) {
				state = State::UNKNOWN;
				return false;
			}
			state = State::ENQUEUED;
			return true;
		}
		void returned(uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
			state = state == State::ENQUEUED && matches(p_rid, p_serial, p_backing) ? State::RETURNED : State::UNKNOWN;
		}
	};
template<class T>struct Ref {std::shared_ptr<T> p=std::make_shared<T>();bool is_valid()const{return bool(p);}T*operator->()const{return p.get();}};
struct NativeDisposalWitness {using State=::State;Ref<Cell> cell;};
struct ID {uint64_t id;ID(uint64_t v):id(v){}operator uint64_t()const{return id;}};
struct Buffer {ID driver_id=19;int qualification_return=0;NativeDisposalWitness native_disposal;uint64_t native_disposal_rid=7,backing_serial=11;int size=128;};
struct UniformSet {ID driver_id=23;int qualification_return=0;std::deque<int*> qualification_dependencies;NativeDisposalWitness native_disposal;uint64_t native_disposal_rid=8,native_disposal_serial=12;};
template<class T>struct Queue {struct Node {T value;T&get(){return value;}};std::deque<Node> q;Node*front(){return q.empty()?nullptr:&q.front();}void pop_front(){q.pop_front();}};
int failures=0;void check(bool b){if(!b)++failures;}
struct Driver {Cell*active=nullptr;int frees=0;void buffer_free(ID){check(active->state==State::ENQUEUED);++frees;}void uniform_set_free(ID){check(active->state==State::ENQUEUED);++frees;}};
struct Frame {Queue<Buffer> buffers_to_dispose_of;Queue<UniformSet> uniform_sets_to_dispose_of;};
struct Device {Frame frames[1];Driver*driver;int buffer_memory=128;uint64_t get_instance_id()const{return 9;}void _qualification_freed(int){}
void _native_disposal_returned(NativeDisposalWitness &p_witness, uint64_t p_rid, uint64_t p_serial, uint64_t p_backing) {
	if (p_witness.cell.is_valid()) {
		if (p_witness.cell->device != uint64_t(get_instance_id())) {
			p_witness.cell->state = NativeDisposalWitness::State::UNKNOWN;
			return;
		}
		p_witness.cell->returned(p_rid, p_serial, p_backing);
	}
}
void buffers(int p_frame){
while (frames[p_frame].buffers_to_dispose_of.front()) {
		Buffer &buffer = frames[p_frame].buffers_to_dispose_of.front()->get();
		driver->buffer_free(buffer.driver_id);
		_qualification_freed(buffer.qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		if (buffer.capture_test_allocation_serial) {
			if (!capture_test_allocation_registry.dispose(buffer.driver_id.id, buffer.capture_test_allocation_serial)) {
				capture_test_coverage_failed.store(true);
			} else {
#if defined(TESTS_ENABLED) && defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED)
				RenderCaptureTestHooks::observe_buffer_dispose(uint64_t(get_instance_id()), buffer.driver_id.id, buffer.capture_test_allocation_serial);
#endif
			}
		}
#endif
		buffer_memory -= buffer.size;

		frames[p_frame].buffers_to_dispose_of.pop_front();
	}}
void sets(int p_frame){
while (frames[p_frame].uniform_sets_to_dispose_of.front()) {
		UniformSet *uniform_set = &frames[p_frame].uniform_sets_to_dispose_of.front()->get();

		driver->uniform_set_free(uniform_set->driver_id);

		_qualification_freed(uniform_set->qualification_return);
#if defined(TESTS_ENABLED) && (defined(MODULE_SKINNED_MESH_CAPTURE_TESTS_ENABLED) || defined(MODULE_PARTICLES_RUNTIME_TESTS_ENABLED))
		capture_test_resource_registry.observe_free(RenderCaptureTestResourceKind::UniformSet, uniform_set->driver_id.id);
#endif

		frames[p_frame].uniform_sets_to_dispose_of.pop_front();
	}} };
void initialize(Cell &c,uint64_t rid,uint64_t serial,uint64_t backing){c.device=9;c.rid=rid;c.serial=serial;c.backing=backing;check(c.enqueue(rid,serial,backing));}
int main(){
Driver d;Device rd;rd.driver=&d;
Buffer b;auto bw=b.native_disposal;initialize(*bw.cell.p,7,11,19);rd.frames[0].buffers_to_dispose_of.q.push_back({b});d.active=bw.cell.p.get();rd.buffers(0);check(bw.cell->state==State::RETURNED);check(d.frees==1);
UniformSet s;auto sw=s.native_disposal;initialize(*sw.cell.p,8,12,23);rd.frames[0].uniform_sets_to_dispose_of.q.push_back({s});d.active=sw.cell.p.get();rd.sets(0);check(sw.cell->state==State::RETURNED);check(d.frees==2);
// No observer or original resource is required to keep the queued cell alive.
std::weak_ptr<Cell> lifetime;
{Buffer temporary;initialize(*temporary.native_disposal.cell.p,7,11,19);lifetime=temporary.native_disposal.cell.p;rd.frames[0].buffers_to_dispose_of.q.push_back({temporary});}
check(!lifetime.expired());d.active=lifetime.lock().get();rd.buffers(0);check(lifetime.expired());
// Exact identity and a real enqueue are necessary. Unknown is never repaired.
for(int field=0;field<3;++field){Cell bad;initialize(bad,7,11,19);bad.returned(7+(field==0),11+(field==1),19+(field==2));check(bad.state==State::UNKNOWN);bad.returned(7,11,19);check(bad.state==State::UNKNOWN);}
Cell live;live.rid=7;live.serial=11;live.backing=19;live.returned(7,11,19);check(live.state==State::UNKNOWN);
Cell duplicate;initialize(duplicate,7,11,19);check(!duplicate.enqueue(7,11,19));check(duplicate.state==State::UNKNOWN);
printf("return_site_and_cell_failures=%d\n",failures);return failures?1:0;}
