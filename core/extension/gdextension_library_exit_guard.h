/**************************************************************************/
/*  gdextension_library_exit_guard.h                                    */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#pragma once
#include "core/templates/local_vector.h"
#include "core/templates/rid.h"

#include "core/error/error_list.h"
#include "core/extension/gdextension_interface.gen.h"
#include "core/os/mutex.h"

class RenderingDevice;
class TestQualificationPhysicalAccess;
class TestQualificationAckAccess;
class TestQualificationTicketAccess;
class TestQualificationRtAccess;
class RenderingDeviceDriver;
#ifdef TESTS_ENABLED
class TestVulkanCompletionAccess;
#endif
class GDExtension;
class GDExtensionManager;
class GDExtensionLoader;
class GDExtensionLibraryLoader;
class Object;
class ClassDB;
#ifdef TESTS_ENABLED
namespace TestDeviceExitEntry {
struct Access;
}
namespace TestDeviceExitLibraryClose {
struct Access;
void observe_close_failure();
} //namespace TestDeviceExitLibraryClose
#endif
#ifdef TESTS_ENABLED
namespace TestDeviceExitContract {
struct RegistryAccess;
}
#endif

namespace GDExtensionDeviceExit {
struct QualificationTicket;

enum class CloseRoute { INIT_ABORT = 1,
	RELOAD = 2,
	LAST_REF = 3,
	EXPLICIT = 4,
	SHUTDOWN = 5,
	TEST_CLEANUP = 6,
	LEVEL_DEINIT = 7,
	SETUP_ABORT = 8,
	DISPLAY_ABORT = 9 };
struct CoreCloseReturnFact {
	uint64_t library_generation = 0;
	uint64_t exit_attempt = 0;
	CloseRoute route = CloseRoute::EXPLICIT;
	bool close_returned = false;
	Error close_error = OK;
	bool os_close_attempted = false;
	bool os_close_returned = false;
	Error os_close_error = OK;
};
void require_close_success(Error p_error, CloseRoute p_route);
// Owning lifecycle callers cannot unwind partially dispatched extension levels.
// This checks dispatch refusal only, not the absent binding/core G2 contract.
void require_level_dispatch_success(Error p_error, int32_t p_level, bool p_initializing);
enum class LifetimeFailure { ASSOCIATION_BUSY,
	LIMIT,
	UNREGISTERED };
[[noreturn]] void stop_lifetime_entry(LifetimeFailure p_failure);
// Last typed pointer-entry result on this thread. Core-owned literals only;
// recording never allocates, enters user handlers, or touches an Object.
void record_binding_entry_result(Error p_error);
const char *get_binding_entry_diagnostic();

class Registry;
class BootstrapScope;
class CallbackScope;
class BindingPublicationScope;
class BindingLifetime;
class CleanupAction;
class CleanupScope;
class NativeCloseScope;
class LevelScope;
class CoreEvidenceAdmission;
class PublicationAck;
struct HolderRetirement;

// Core-only lifetime reference. No Object or extension code participates in release.
class RecordRef {
	friend class NativeCloseScope;
	friend class LevelScope;
	friend class CoreEvidenceAdmission;
	friend class PublicationAck;
	friend struct HolderRetirement;
	friend class RegistrationContext;
	friend class CallbackIdentityScope;
	friend class Registry;
	friend class BootstrapScope;
	friend class CallbackScope;
	friend class BindingPublicationScope;
	friend class BindingLifetime;
	friend class CleanupAction;
	friend class CleanupScope;
#ifdef TESTS_ENABLED
	friend struct TestDeviceExitContract::RegistryAccess;
	friend struct TestDeviceExitLibraryClose::Access;
	friend struct TestDeviceExitEntry::Access;
#endif
	Registry *registry = nullptr;
	uint32_t slot = 0;
	RecordRef(Registry *p_registry, uint32_t p_slot) : registry(p_registry), slot(p_slot) {}

public:
	RecordRef() = default;
	RecordRef(const RecordRef &) = delete;
	RecordRef &operator=(const RecordRef &) = delete;
	RecordRef(RecordRef &&p_other) noexcept;
	RecordRef &operator=(RecordRef &&p_other) noexcept;
	~RecordRef();
	void reset();
	bool is_valid() const { return registry != nullptr; }
	uint64_t get_generation() const;
};
enum class CleanupPhase : uint8_t { UNSCHEDULED = 0,
	USER = 1,
	BINDING = 2,
	FUTURE_LEVEL = 3 };

// Core-owned cells are allocated at acquisition and remain intact until the
// admitting frame returns. No DSO destructor or allocation is needed on release.
struct HolderRetirement {
	RecordRef reference;
	uint64_t serial = 0;
	uint64_t acquired_thread = 0;
	uint64_t retired_thread = 0;
	uint64_t retirement_attempt = 0;
	uint64_t return_frame = 0;
	bool pending = false;
	bool cleanup = false;
	bool typed_code_hold = false;
	uint32_t qualification_children = 0;
	bool core_owned = false;
	bool core_object_action = false;
	bool core_disposal_returned = false;
	uint64_t core_disposal_frame = 0;
	uint64_t core_disposal_thread = 0;
	uint64_t core_object = 0;
	uint64_t generation = 0;
	uint64_t terminal_action = 0;
	// Original level action that will dispose this exact Object association.
	// The Object cleanup action above remains the callback-entry authority.
	uint64_t scheduled_action = 0;
	uint64_t handle_serial = 0;
	CleanupPhase phase = CleanupPhase::UNSCHEDULED;
	HolderRetirement *record_next = nullptr;
	HolderRetirement *frame_next = nullptr;
	void (*destroy)(HolderRetirement *) = nullptr;

protected:
	explicit HolderRetirement(RecordRef &&p_reference);
	~HolderRetirement() = default;
};

// Internal state products adapted by the generated typed lifecycle ABI.
struct GuardToken { uint64_t registry_instance = 0; uint64_t handle_serial = 0; };
struct GuardedLibraryRef { uint64_t registry_instance = 0; uint64_t handle_serial = 0; };
struct InitializationLevelContextRef { uint64_t registry_instance = 0; uint64_t handle_serial = 0; };
struct DeinitLevelContextRef { uint64_t registry_instance = 0; uint64_t handle_serial = 0; };
using DeinitPreflightContextRef = GDExtensionDeviceExitDeinitPreflightContextRef;
struct RegisteredCleanupRef { uint64_t registry_instance = 0; uint64_t handle_serial = 0; };
struct LibraryCodeHoldRef { uint64_t registry_instance = 0; uint64_t handle_serial = 0; };
using UserLevelFnV1 = void (*)(void *, GDExtensionInitializationLevel);
using PreflightFnV1 = int32_t (*)(DeinitPreflightContextRef, void *);
struct LevelNeedsV1 { uint32_t user_services = 0; uint32_t binding_services = 0; };
struct GuardRegistrationV1 {
	GDExtensionDeviceExitAbi abi{};
	uint32_t binding_protocol = 0;
	uint32_t level_bitmap = 0;
	uint32_t user_cleanup_bitmap = 0;
	uint32_t binding_cleanup_bitmap = 0;
	uint64_t entry_classes = 0;
	LevelNeedsV1 needs[4];
	GDExtensionInitialization expected_initialization{};
	UserLevelFnV1 user_initializer = nullptr;
	UserLevelFnV1 user_terminator = nullptr;
	PreflightFnV1 preflight = nullptr;
	void *preflight_userdata = nullptr;
};
struct ExitDiagnosticV1 {
	GDExtensionDeviceExitAbi abi{};
	int32_t disposition = ERR_UNAVAILABLE;
	uint32_t level = UINT32_MAX;
	uint64_t pending_count = 0;
};
enum class GuardEnrollment { UNAVAILABLE, PENDING_BOOTSTRAP, QUALIFIED, REVOKED };
enum class LevelPhase { NONE, INITIALIZE, DEINITIALIZE, PREFLIGHT, SCENE_READY };
enum class SceneReadyFailureReason { NO_SCENE_TREE, NO_ROOT_WORLD, NO_PUBLISHED_RENDERING_SERVER, ACTUAL_RD_UNSUPPORTED, LOCAL_RD_UNSUPPORTED, CALLBACK_REJECTED, PAIR_UNSCHEDULED, CALLBACK_QUOTA };
enum class TerminalActionKind { OBJECT, LEVEL_USER, LEVEL_BINDING };
struct LevelAction {
	uint64_t serial = 0;
	uint64_t generation = 0;
	uint64_t owner_thread = 0;
	uint32_t level = 0;
	TerminalActionKind kind = TerminalActionKind::LEVEL_USER;
	uint32_t services = 0;
	UserLevelFnV1 wrapper = nullptr;
	UserLevelFnV1 terminator = nullptr;
	void *userdata = nullptr;
	bool returned = false;
};

// Private core storage products, not public ABI or guarded/world authority.
// Each product pins only independent core memory, never the extension Object.
class CoreEvidenceAdmission {
	friend class Registry;
	RecordRef reference;
	uint64_t generation = 0;
	uint64_t attempt = 0;
	uint64_t publication_serial = 0;
	CloseRoute route = CloseRoute::EXPLICIT;

public:
	CoreEvidenceAdmission() = default;
	CoreEvidenceAdmission(const CoreEvidenceAdmission &) = delete;
	CoreEvidenceAdmission &operator=(const CoreEvidenceAdmission &) = delete;
	CoreEvidenceAdmission(CoreEvidenceAdmission &&) noexcept = default;
	CoreEvidenceAdmission &operator=(CoreEvidenceAdmission &&) noexcept = default;
};

class PublicationAck {
	friend class Registry;
	RecordRef reference;
	uint64_t generation = 0;
	uint64_t attempt = 0;
	uint64_t publication_serial = 0;
	CloseRoute route = CloseRoute::EXPLICIT;
	uint8_t digest[32] = {};

public:
	PublicationAck() = default;
	PublicationAck(const PublicationAck &) = delete;
	PublicationAck &operator=(const PublicationAck &) = delete;
	PublicationAck(PublicationAck &&) noexcept = default;
	PublicationAck &operator=(PublicationAck &&) noexcept = default;
};

// Copyable core metadata ownership for class descriptors and placeholders. Copies
// share one private Registry reference; they never acquire DSO code authority.
class SharedRecord {
	struct Storage;
	Storage *storage = nullptr;

public:
	SharedRecord() = default;
	explicit SharedRecord(RecordRef &&p_reference);
	SharedRecord(const SharedRecord &p_other);
	SharedRecord(SharedRecord &&p_other) noexcept;
	SharedRecord &operator=(SharedRecord p_other);
	~SharedRecord();
	const RecordRef &get_reference() const;
};

// Persistent ownership of stored binding code. Copies share one holder;
// retirement follows the actual free callback return, not mere pointer removal.
class BindingLifetime {
	friend class ::Object;
	friend class ::ClassDB;
	void record_core_disposal();
	void bind_core_action(uint64_t p_object);
	Error acquire_core(const RecordRef &p_reference);
	struct Storage;
	Storage *storage = nullptr;

public:
	BindingLifetime() = default;
	BindingLifetime(const BindingLifetime &p_other);
	BindingLifetime(BindingLifetime &&p_other) noexcept;
	BindingLifetime &operator=(BindingLifetime p_other);
	~BindingLifetime();
	Error acquire(const RecordRef &p_reference);
};

// Borrowed core entry identity, distinct from public metadata capabilities.
// One TLS stack covers bootstrap, normal entry and teardown so a nested foreign
// generation or cleanup frame cannot borrow an outer registration authority.
class RegistrationContext {
	friend class CleanupAction;
	friend class BindingLifetime;
	friend class CallbackIdentityScope;
	friend class Registry;
	friend class BootstrapScope;
	friend class CallbackScope;
	friend class LevelScope;
	friend class CleanupScope;
	static thread_local RegistrationContext *current;
	RegistrationContext *previous = nullptr;
	const RecordRef *reference = nullptr;
	bool registration_allowed = false;
	bool admitted = false;
	uint64_t frame_id = 0;
	LevelPhase level_phase = LevelPhase::NONE;
	int32_t level = -1;
	uint8_t terminal_stage = 0;
	bool ack = false;
	bool diagnostic_published = false;
	uint64_t scene_ready_schedule_object = 0;
	Error scene_ready_schedule_error = OK;
	int32_t diagnostic_result = ERR_UNAVAILABLE;
	uint64_t diagnostic_pending = 0;
	HolderRetirement *retiring_holders = nullptr;
	void enter(const RecordRef &p_reference, bool p_registration_allowed, bool p_admitted = true);
	void leave();
};

// Select the actual callback owner from an already admitted cohort. This
// borrows its stack frame without another admission, reference or allocation.
class CallbackIdentityScope {
	friend class ::Object;
	RegistrationContext context;
	explicit CallbackIdentityScope(const RecordRef &p_reference);
	CallbackIdentityScope(const CallbackIdentityScope &) = delete;
	CallbackIdentityScope &operator=(const CallbackIdentityScope &) = delete;
	~CallbackIdentityScope();
};

// Core callback frame, not a public code capability. The private reference and
// active count survive nested DSO-to-core calls until the outer frame returns.
class CallbackScope {
	friend class LevelScope;
	RecordRef reference;
	RegistrationContext context;
	Error error = ERR_INVALID_DATA;

public:
	explicit CallbackScope(const RecordRef &p_reference);
	CallbackScope(const CallbackScope &) = delete;
	CallbackScope &operator=(const CallbackScope &) = delete;
	~CallbackScope();
	Error get_error() const { return error; }
};

// Observes the original initializer tuple and level frame; a void callback
// return is recorded independently of the unavailable guarded/G2 issuer.
class LevelScope {
	friend class ::GDExtension;
	RecordRef reference;
	RegistrationContext context;
	int32_t level;
	bool guarded = false;
	bool initializing;
	Error error;
	LevelScope(const RecordRef &p_reference, GDExtension *p_owner, const GDExtensionInitialization &p_initialization, int32_t p_level, bool p_initializing);

public:
	~LevelScope();
	Error get_error() const { return error; }
};

// Exact core-issued teardown authority. It retains the existing generation,
// but never grants normal entry, new references or association publication.
class CleanupAction {
	friend class ::Object;
	void record_core_disposal();
	friend class Registry;
	friend class BindingLifetime;
	friend class CleanupScope;
#ifdef TESTS_ENABLED
	friend struct TestDeviceExitEntry::Access;
#endif
	struct Storage;
	Storage *storage = nullptr;

public:
	CleanupAction() = default;
	CleanupAction(const CleanupAction &) = delete;
	CleanupAction &operator=(const CleanupAction &) = delete;
	~CleanupAction();
	CleanupPhase get_phase() const;
	bool is_scheduled() const;
};

class CleanupScope {
	RegistrationContext context;
	CleanupAction::Storage *storage = nullptr;
	Error error = ERR_INVALID_DATA;
	bool comparison_only = false;

public:
	CleanupScope(const CleanupAction &p_action, const RecordRef &p_generation, uint64_t p_object_id, uint64_t p_action_id, bool p_comparison_only = false);
	CleanupScope(const CleanupScope &) = delete;
	CleanupScope &operator=(const CleanupScope &) = delete;
	~CleanupScope();
	Error get_error() const { return error; }
};

// Only core metadata publication may occur under this short gate lock.
// The caller holds the Object metadata lock first; never call DSO code here.
class BindingPublicationScope {
	Registry *registry = nullptr;
	Error error = ERR_INVALID_DATA;

public:
	explicit BindingPublicationScope(const RecordRef &p_reference);
	BindingPublicationScope(const BindingPublicationScope &) = delete;
	BindingPublicationScope &operator=(const BindingPublicationScope &) = delete;
	~BindingPublicationScope();
	Error get_error() const { return error; }
};

class BootstrapScope {
	friend class Registry;
	friend class ::GDExtension;
#ifdef TESTS_ENABLED
	friend struct TestDeviceExitContract::RegistryAccess;
	friend struct TestDeviceExitLibraryClose::Access;
	friend struct TestDeviceExitEntry::Access;
#endif
	static thread_local BootstrapScope *current;
	BootstrapScope *previous;
	RecordRef reference;
	RegistrationContext context;
	GDExtension *expected;
	BootstrapScope(RecordRef &&p_reference, GDExtension *p_expected);
	Error observe_tuple(const GDExtensionInitialization &p_initialization, Error p_error);

public:
	BootstrapScope(const BootstrapScope &) = delete;
	BootstrapScope &operator=(const BootstrapScope &) = delete;
	~BootstrapScope();
};

// Source-only observation context. A different loader on the same thread
// cannot attach its OS result to this generation.
class NativeCloseScope {
	friend class ::GDExtension;
	friend class ::GDExtensionLibraryLoader;
	static thread_local NativeCloseScope *current;
	NativeCloseScope *previous;
	const RecordRef &reference;
	const GDExtensionLoader *loader;
	NativeCloseScope(const RecordRef &p_reference, const GDExtensionLoader *p_loader);
	~NativeCloseScope();
	NativeCloseScope(const NativeCloseScope &) = delete;
	NativeCloseScope &operator=(const NativeCloseScope &) = delete;
	static void observe(const GDExtensionLoader *p_loader, bool p_returned, Error p_error = OK);
};

// Bounded core record storage. Public views expose metadata only; full guarded
// admission remains unavailable until the complete entry/holder contract closes.
class Registry {
	friend class ::GDExtension;
	friend class ::GDExtensionManager;

public:
	static constexpr uint32_t MAX_LIVE_GENERATIONS = 1024;

private:
	friend class NativeCloseScope;
	friend class LevelScope;
	friend class CoreEvidenceAdmission;
	friend class PublicationAck;
	friend struct HolderRetirement;
	friend class RegistrationContext;
	friend class RecordRef;
	friend class BootstrapScope;
	friend class CallbackScope;
	friend class BindingPublicationScope;
	friend class BindingLifetime;
	friend class CleanupAction;
	friend class CleanupScope;
#ifdef TESTS_ENABLED
 friend class ::TestVulkanCompletionAccess;
	friend struct TestDeviceExitContract::RegistryAccess;
	friend struct TestDeviceExitLibraryClose::Access;
	friend struct TestDeviceExitEntry::Access;
#endif
	static constexpr uint32_t RECORD_CAPACITY = MAX_LIVE_GENERATIONS;
	static constexpr uint32_t VIEW_CAPACITY = 8192;
	static constexpr uint32_t PRIVATE_CAPACITY = 4096;
	static constexpr uint32_t INVALID_SLOT = UINT32_MAX;
	enum class Phase { RUNNING,
		FINALIZING,
		FINALIZED };
	struct Sequence {
		uint64_t next = 1;
		bool exhausted = false;
		bool take(uint64_t &r_value);
	};
	struct LevelState {
		// Enrollment reserves mandatory wrapper contexts before publication.
		uint64_t reserved_initialize_frame = 0;
		uint64_t reserved_deinitialize_frame = 0;
		uint64_t initialize_view = 0;
		uint64_t deinitialize_view = 0;
		uint64_t initialize_frame = 0;
		uint64_t deinitialize_frame = 0;
		bool initialize_returned = false;
		bool deinitialize_returned = false;
		bool initialized = false;
		bool finalized = false;
		LevelAction user_action;
		LevelAction binding_action;
	};
	// One bounded core source projection. This is local storage, not the
	// launch-bound ANGECAP wire schema or an original owner export grant.
	static constexpr uint32_t CORE_EVIDENCE_BYTES = 256;
	struct CoreExportGrant {
		uint8_t bytes[CORE_EVIDENCE_BYTES] = {};
		uint32_t size = 0;
	};
	struct Record {
		bool occupied = false;
		bool attached = false;
		bool scope_live = false;
		bool quiescing = false;
		bool close_started = false;
		uint32_t active_callbacks = 0;
		uint32_t cleanup_reservations = 0;
		uint32_t binding_holders = 0;
		uint32_t return_pending_holders = 0;
		HolderRetirement *holders = nullptr;
		uint32_t cleanup_by_phase[4] = {};
		uint32_t active_cleanup_by_phase[4] = {};
		uint32_t retiring_cleanup_by_phase[4] = {};
		GDExtension *original_owner = nullptr; // Identity only; never dereferenced.
		uint64_t owner_thread = 0;
		uint64_t bootstrap_frame = 0;
		uint64_t evidence_serial = 0;
		uint64_t evidence_attempt = 0;
		CloseRoute evidence_route = CloseRoute::EXPLICIT;
		uint32_t evidence_revision = 0;
		uint32_t evidence_member_count = 0;
		bool evidence_committed = false;
		bool evidence_ack_received = false;
		uint8_t evidence_digest[32] = {};
		CoreExportGrant evidence;
		bool qualification_main_issued = false;
		bool preflight_sealed = false;
		CloseRoute preflight_route = CloseRoute::EXPLICIT;
		bool bootstrap_tuple_observed = false;
		GDExtensionInitialization original_initialization = {};
		GuardEnrollment guard_enrollment = GuardEnrollment::UNAVAILABLE;
		GuardRegistrationV1 guard;
		uint64_t guard_token = 0;
		uint64_t reserved_control_frame = 0;
		uint64_t reserved_control_view = 0;
		bool control_reference_reserved = false;
		uint32_t control_view_slot = INVALID_SLOT;
		bool control_complete = false;
		bool preflight_accepted = false;
		LevelState levels[4];
		enum class SceneReadyState { ABSENT, REGISTERED, PENDING, IN_FRAME, DONE, CANCELLED_BEFORE_EFFECTS, UNSUPPORTED_BEFORE_EFFECTS, FAILED_LATE_PARTIAL };
		SceneReadyState scene_ready_state = SceneReadyState::ABSENT;
		uint64_t reserved_scene_ready_frame = 0;
		uint64_t scene_ready_view_serial = 0;
		uint32_t scene_ready_view_slot = INVALID_SLOT;
		enum class SceneReadyFlavor { NONE, V1, V2 };
		SceneReadyFlavor scene_ready_flavor = SceneReadyFlavor::NONE;
		GDExtensionSceneInstallRequirementsV2 scene_install_requirements = {};
		GDExtensionSceneInstallPlanV2 scene_install_plan = {};
		bool scene_install_plan_selected = false;
		GDExtensionDeviceExitSceneReadyCallbackV1 scene_ready_callback = nullptr;
		GDExtensionDeviceExitSceneReadyCallbackV2 scene_ready_callback_v2 = nullptr;
		void *scene_ready_userdata = nullptr;
		Error scene_ready_error = OK;
		SceneReadyFailureReason scene_ready_failure_reason = SceneReadyFailureReason::CALLBACK_REJECTED;
		CloseRoute scene_ready_cancel_route = CloseRoute::EXPLICIT;
		uint64_t exit_attempt = 0;
		CloseRoute close_route = CloseRoute::EXPLICIT;
		uint32_t close_source = 0;
		bool close_returned = false;
		int32_t close_error = OK;
		bool os_close_attempted = false;
		bool os_close_returned = false;
		Error os_close_error = OK;
		uint32_t private_refs = 0;
		uint32_t external_refs = 0;
		uint64_t authority_serial = 0;
		GDExtensionDeviceExitLibrarySnapshot snapshot = {};
		char failure_detail[64] = {};
	};
	struct BindingOwner {
		GDExtension *token = nullptr;
		uint32_t record = INVALID_SLOT;
	};
	enum class ViewDomain { METADATA, LIBRARY, INITIALIZE, DEINITIALIZE, PREFLIGHT, ACTION, SCENE_READY };
	struct View {
		uint64_t serial = 0;
		uint32_t record = 0;
		ViewDomain domain = ViewDomain::METADATA;
		uint64_t frame = 0;
		uint64_t action = 0;
	};
	BinaryMutex mutex;
	Phase phase = Phase::FINALIZED;
	bool engine_cycle_ended = true;
	uint32_t live_services = 0;
	uint64_t instance = 0;
	Sequence generations;
	Sequence serials;
	Sequence exit_attempts;
	Sequence holder_serials;
	Sequence frame_serials;
	Sequence evidence_serials;
	Sequence action_serials;
	Record records[RECORD_CAPACITY];
	View views[VIEW_CAPACITY];
	BindingOwner binding_owners[RECORD_CAPACITY];
	uint32_t record_limit = RECORD_CAPACITY;
	uint32_t view_limit = VIEW_CAPACITY;
	uint32_t private_limit = PRIVATE_CAPACITY;
	uint32_t action_limit = 8;
	char reservation_failure[64] = {};

	Registry() = default;
	Registry(const Registry &) = delete;
	Registry &operator=(const Registry &) = delete;
	static bool issue_instance(uint64_t &r_instance);
	uint32_t find_view(GDExtensionDeviceExitLibraryViewRef p_view) const;
	Error issue_view(uint32_t p_record, GDExtensionDeviceExitLibraryViewRef *r_view);
	void release_private(uint32_t p_slot);
	void retire_binding_holder(HolderRetirement *p_holder);
	void finish_retiring_holders(const RegistrationContext &p_frame);
	void unlink_holder_locked(HolderRetirement *p_holder);
	Error prepare_core_evidence(const RecordRef &p_ref, CloseRoute p_route, CoreEvidenceAdmission &r_admission);
	Error commit_core_evidence(const CoreEvidenceAdmission &p_admission, PublicationAck &r_ack);
	Error register_core_event_export(const CoreEvidenceAdmission &p_admission, CoreExportGrant &r_export);
	bool core_admission_matches_locked(const CoreEvidenceAdmission &p_admission) const;
	Error prepare_selected_preclose(const RecordRef &p_ref, CloseRoute p_route);
	Error consume_core_ack(const RecordRef &p_ref, const PublicationAck &p_ack);
	Error issue_borrow_locked(ViewDomain p_domain, uint64_t p_action, uint64_t &r_serial);
	Error resolve_borrow_locked(uint64_t p_instance, uint64_t p_serial, ViewDomain p_domain, View *&r_view);
	void expire_frame_locked(uint64_t p_frame);
	Error guarded_preflight(const RecordRef &p_ref, CloseRoute p_route);
	bool guard_qualified(const RecordRef &p_ref);
	bool action_safe_locked(const Record &p_record, uint64_t p_action) const;
	void revoke_guard_locked(Record &p_record);
	void clear_guard_control_locked(Record &p_record);
	void retire_scene_ready_reservation_locked(Record &p_record);
	Error cancel_pending_scene_ready(const RecordRef &p_ref, CloseRoute p_route);
	Error dispatch_scene_ready(const RecordRef &p_ref);
	bool scene_ready_pending(const RecordRef &p_ref);
	Error scene_ready_initialization_result(const RecordRef &p_ref);
	Error reject_pending_scene_ready_before_effects(const RecordRef &p_ref, SceneReadyFailureReason p_reason);
	bool scene_ready_main_device_available(const RecordRef &p_ref, Object *p_device);
 Error select_scene_install_plan(const RecordRef &,GDExtensionSceneInstallPlanV2 &,SceneReadyFailureReason &);
 Error scene_install_plan_locked(const Record &,GDExtensionSceneInstallPlanV2 &,SceneReadyFailureReason &) const;
 bool scene_install_plan_matches_locked(const Record &,SceneReadyFailureReason * = nullptr) const;

	void reclaim(uint32_t p_slot);
	void try_finalize();

	// Internal substrate only; guarded qualification still requires the complete
	// original owner, level, service and control enrollment contract.
	bool physical_can_close_locked(uint32_t p_slot) const;
	Error preflight_exit(const RecordRef &p_ref, CloseRoute p_route);

public:
private:
#ifdef TESTS_ENABLED
 friend class ::TestQualificationPhysicalAccess;
 friend class ::TestQualificationAckAccess;
 friend class ::TestQualificationTicketAccess;
 friend class ::TestQualificationRtAccess;
#endif
 friend class ::RenderingDevice;
 friend class ::RenderingDeviceDriver;
 struct DeviceRecord { Object *object=nullptr; uint64_t epoch=0, session=0, thread=0, execution_thread=0; bool execution_migrated=false; bool live=false; uint32_t refs=0; Sequence sessions; };
 struct DeviceRef { uint64_t serial=0; uint32_t device=0, library=0; bool session=false; bool qualification_main=false; };
 DeviceRecord device_records[RECORD_CAPACITY];
 DeviceRef device_refs[VIEW_CAPACITY];
 Sequence device_epochs, device_ref_serials;
 struct QualificationBridge;
 QualificationBridge *qualification_bridges[RECORD_CAPACITY] = {};
 static thread_local QualificationBridge *active_qualification_bridge;
 static thread_local uint64_t active_qualification_serial;
 bool qualification_accepted_call_locked(QualificationBridge *,uint32_t);
 Error qualification_rt_schedule(GDExtensionQualificationBridgeRefV1);
 Error qualification_cleanup_retire_matches_original_child_locked(QualificationBridge *,uint32_t,const void *);
 Error qualification_rt_ticket_locked(QualificationBridge *,const GDExtensionQualificationRtRequestV1 &,const uint8_t *,const uint8_t *,QualificationTicket *&);

 Sequence qualification_serials;
 Error qualification_schedule(GDExtensionQualificationBridgeRefV1, uint64_t, uint32_t, const GDExtensionQualificationBudgetV1 &);
 void qualification_retire_step(QualificationBridge *, RenderingDevice *, uint64_t, uint32_t);
 void qualification_ticket_publish(QualificationTicket *, RenderingDevice *);
 void qualification_ticket_step(QualificationTicket *, RenderingDevice *);
 Error qualification_upload_destination(QualificationBridge *,RenderingDevice *,const GDExtensionQualificationUploadPlanV1 &);
 Error qualification_record_preflight(QualificationBridge *, RenderingDevice *, const GDExtensionQualificationRecordV1 &, LocalVector<RID> &);
 void qualification_record_append(QualificationTicket *, RenderingDevice *);
 Error qualification_ticket_locked(QualificationBridge *, GDExtensionQualificationTicketRefV1, QualificationTicket *&);
 Error qualification_bridge_locked(GDExtensionQualificationBridgeRefV1, bool, bool, QualificationBridge *&, RenderingDevice *&,uint32_t=0);

 Error device_created(Object*);
 Error device_execution_moved(Object *, uint64_t, uint64_t);
 void device_closed(Object*);
 void device_lost(Object*);
 void device_lost(uint64_t,uint64_t);
 void device_loss_identity(Object*,uint64_t&,uint64_t&);
 void device_destroyed(Object*);
 void reclaim_device_locked(uint32_t);
 Error device_ref_locked(uint64_t,uint64_t,bool,DeviceRef*&);
 Error issue_device_ref_locked(uint32_t,uint32_t,bool,uint64_t&);
	GDExtensionDeviceSessionResultV1 bind_device_session_in_frame(const GDExtensionDeviceExitAbi *, uint64_t, uint64_t, ViewDomain, Object *, GDExtensionDeviceRegistryContextRef *, GDExtensionOriginalDeviceSessionRef *);
	// Caller holds Registry::mutex and authenticates the original level action.
	// Both initialization and scene-ready scheduling share this atomic pair path.
	Error schedule_object_cleanup_pair_locked(Record &p_record, uint64_t p_action, uint64_t p_object_id);
	Error validate_object_cleanup_pair_locked(const Record &p_record, uint64_t p_object_id) const;
public:
 GDExtensionDeviceSessionResultV1 bind_device_session(const GDExtensionDeviceExitAbi*,InitializationLevelContextRef,Object*,GDExtensionDeviceRegistryContextRef*,GDExtensionOriginalDeviceSessionRef*);
 GDExtensionDeviceSessionResultV1 bind_scene_ready_device_session(const GDExtensionDeviceExitAbi *,GDExtensionDeviceExitSceneReadyContextRef,Object *,GDExtensionDeviceRegistryContextRef *,GDExtensionOriginalDeviceSessionRef *);
 GDExtensionDeviceSessionResultV1 bind_main_scene_ready_device_session(const GDExtensionDeviceExitAbi *, GDExtensionDeviceExitSceneReadyContextRef, GDExtensionDeviceRegistryContextRef *, GDExtensionOriginalDeviceSessionRef *, uint64_t *);
 GDExtensionDeviceSessionResultV1 copy_device_session(GDExtensionDeviceRegistryContextRef,GDExtensionOriginalDeviceSessionRef,bool,GDExtensionDeviceSessionViewV1*);
 GDExtensionDeviceSessionResultV1 resolve_device_session(GDExtensionDeviceRegistryContextRef,uint64_t,uint64_t,GDExtensionOriginalDeviceSessionRef*);
 GDExtensionDeviceSessionResultV1 release_device_reference(uint64_t,uint64_t,bool);


 GDExtensionDeviceSessionResultV1 qualification_open(const GDExtensionDeviceExitAbi *, GDExtensionDeviceExitSceneReadyContextRef, GDExtensionDeviceRegistryContextRef, GDExtensionOriginalDeviceSessionRef, GDExtensionDeviceExitLibraryCodeHoldRef, GDExtensionQualificationBridgeRefV1 *);
 GDExtensionDeviceSessionResultV1 qualification_backing(GDExtensionQualificationBridgeRefV1, uint64_t, GDExtensionQualificationBackingV1 *);
 GDExtensionDeviceSessionResultV1 qualification_record(GDExtensionQualificationBridgeRefV1,const GDExtensionQualificationRecordV1 *,GDExtensionQualificationTicketRefV1 *,GDExtensionQualificationReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_observe(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1,const GDExtensionQualificationBudgetV1 *,GDExtensionQualificationReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_facts(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1,uint64_t,uint32_t,uint32_t,GDExtensionQualificationFactV1 *,uint32_t *);
 GDExtensionDeviceSessionResultV1 qualification_copy(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1,uint64_t,void *,uint32_t,uint32_t *);
 GDExtensionDeviceSessionResultV1 qualification_release(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1);
 GDExtensionDeviceSessionResultV1 qualification_prepare_uploads(GDExtensionQualificationBridgeRefV1,const GDExtensionQualificationUploadPrepareV1 *,const GDExtensionQualificationUploadPlanV1 *,uint32_t,GDExtensionQualificationTicketRefV1 *,GDExtensionQualificationReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_upload(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1,const GDExtensionQualificationUploadRequestV1 *,const void *,uint32_t,GDExtensionQualificationUploadSegmentV1 *,uint32_t,GDExtensionQualificationUploadReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_record_prepared(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1,const GDExtensionQualificationRecordV1 *,GDExtensionQualificationReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_cancel(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationTicketRefV1);
 GDExtensionDeviceSessionResultV1 qualification_rt_configure(GDExtensionQualificationBridgeRefV1,const GDExtensionQualificationRtConfigV1 *);
 GDExtensionDeviceSessionResultV1 qualification_rt_reserve(GDExtensionQualificationBridgeRefV1,const GDExtensionQualificationRtRequestV1 *,const void *,const void *,GDExtensionQualificationRtRefV1 *);
 GDExtensionDeviceSessionResultV1 qualification_rt_activate(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationRtRefV1);
 GDExtensionDeviceSessionResultV1 qualification_rt_observe(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationRtRefV1,GDExtensionQualificationRtObservationV1 *,void *,uint32_t,GDExtensionQualificationUploadSegmentV1 *,uint32_t);
 GDExtensionDeviceSessionResultV1 qualification_rt_release(GDExtensionQualificationBridgeRefV1,GDExtensionQualificationRtRefV1);
 void qualification_rt_dispatch(uint64_t,uint64_t);

 GDExtensionDeviceSessionResultV1 qualification_close(GDExtensionQualificationBridgeRefV1);
 GDExtensionDeviceSessionResultV1 qualification_storage_create(GDExtensionQualificationBridgeRefV1, const GDExtensionStorageCreateRequestV1 *, GDExtensionStorageBackingV1 *);
 GDExtensionDeviceSessionResultV1 qualification_native_unit_create(GDExtensionQualificationBridgeRefV1, const GDExtensionNativeUnitCreateRequestV1 *, const void *, uint64_t, GDExtensionNativeUnitBackingV1 *);
 GDExtensionDeviceSessionResultV1 qualification_storage_retire(GDExtensionQualificationBridgeRefV1,const GDExtensionStorageRetireRequestV1 *,GDExtensionStorageRetireRefV1 *,GDExtensionStorageRetireReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_storage_retire_observe(GDExtensionQualificationBridgeRefV1,GDExtensionStorageRetireRefV1,const GDExtensionQualificationBudgetV1 *,GDExtensionStorageRetireReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_storage_retire_release(GDExtensionQualificationBridgeRefV1,GDExtensionStorageRetireRefV1);
 GDExtensionDeviceSessionResultV1 qualification_native_unit_retire(GDExtensionQualificationBridgeRefV1,const GDExtensionNativeUnitRetireRequestV1 *,GDExtensionNativeUnitRetireRefV1 *,GDExtensionNativeUnitRetireReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_native_unit_retire_observe(GDExtensionQualificationBridgeRefV1,GDExtensionNativeUnitRetireRefV1,const GDExtensionQualificationBudgetV1 *,GDExtensionNativeUnitRetireReportV1 *);
 GDExtensionDeviceSessionResultV1 qualification_native_unit_retire_release(GDExtensionQualificationBridgeRefV1,GDExtensionNativeUnitRetireRefV1);
 void qualification_service(uint64_t, uint64_t, uint32_t, uint32_t, uint32_t);

	static Registry &get_singleton();
	// Called only by the core Object registration path with an original owner
	// declaration. No public handle or inferred constructor phase is accepted.
	Error schedule_object_cleanup(const GDExtensionDeviceExitAbi *p_abi, InitializationLevelContextRef p_context, RegisteredCleanupRef p_terminal, uint64_t p_object_id);
	Error register_scene_ready(const GDExtensionDeviceExitAbi *p_abi, InitializationLevelContextRef p_context, GDExtensionDeviceExitSceneReadyCallbackV1 p_callback, void *p_original_userdata);
 Error register_scene_ready_v2(const GDExtensionSceneInstallRequirementsV2 *,InitializationLevelContextRef,GDExtensionDeviceExitSceneReadyCallbackV2,void *);
	Error schedule_scene_ready_object_cleanup(const GDExtensionDeviceExitAbi *p_abi, GDExtensionDeviceExitSceneReadyContextRef p_context, uint32_t p_phase, uint64_t p_object_id);
	Error scene_ready_abort_unscheduled_pair(const GDExtensionDeviceExitAbi *, GDExtensionDeviceExitSceneReadyContextRef, int32_t, uint32_t, uint64_t);
	static void register_interfaces();
	// Core service owners call these at actual start and before first teardown.
	void service_started(uint32_t p_service);
	Error service_stopping(uint32_t p_service);
	Error begin_cycle(uint64_t &r_cycle);
	bool begin_finalizing(uint64_t p_cycle);
	void finish_engine_cycle(uint64_t p_cycle);
	Error reserve_open(uint32_t p_kind, RecordRef &r_wrapper, RecordRef &r_scope, GDExtension *p_owner = nullptr);
	void attach(const RecordRef &p_ref);
	// Unknown tokens remain legacy; registered closed owners must never fall back.
	Error retain_binding_owner(void *p_token, RecordRef &r_ref);
	Error retain_callable_owner(void *p_token, RecordRef &r_ref);
	Error register_callable_cleanup(const RecordRef &p_ref, uint64_t p_identity, CleanupAction &r_action, uint64_t &r_serial);
	void forget_binding_owner(GDExtension *p_owner);
	void observe_initialize(const RecordRef &p_ref, Error p_error);
	void detach(const RecordRef &p_ref);
	// Internal G1 substrate. Sealing persists on Busy; accepted frames may
	// return, but no normal admission/publication may reopen this generation.
	// Full guarded qualification additionally requires finite cleanup/preflight.
	Error begin_quiesce(const RecordRef &p_ref);
	// Object-facing enrollment requires the current exact normal/bootstrap entry.
	Error authenticate_registration(const RecordRef &p_ref);
	Error check_entry_classes(const RecordRef &p_ref, uint64_t p_classes);
	Error check_current_entry_classes(uint64_t p_classes);
	// Trusted core primitive; never expose it directly through the extension ABI.
	Error register_cleanup_action(const RecordRef &p_ref, uint64_t p_object_id, uint64_t p_thread_id, uint64_t p_action_id, CleanupAction &r_action, CleanupPhase p_phase = CleanupPhase::UNSCHEDULED);
	Error mark_core_object_action(const RecordRef &p_ref, uint64_t p_object_id, CleanupAction &r_action);
	Error begin_close(const RecordRef &p_ref, CloseRoute p_route = CloseRoute::EXPLICIT);
	void observe_close(const RecordRef &p_ref, Error p_error);
	Error resolve_core_close_return(const RecordRef &p_ref, CoreCloseReturnFact &r_fact);
	Error retain_private(const RecordRef &p_ref, RecordRef &r_ref);

	// Internal implementation seam. No guarded version is exposed to loaders
	// until reciprocal ABI/census adoption; production state machine is tested
	// through actual entry/level callbacks, without test-issued authority.
	Error register_guard(GDExtensionClassLibraryPtr p_library, const GuardRegistrationV1 *p_registration, GuardToken *r_token);
	Error borrow_guarded_library(const GDExtensionDeviceExitAbi *, GDExtensionClassLibraryPtr, GuardedLibraryRef *);
	Error borrow_initialize_level(const GDExtensionDeviceExitAbi *, GDExtensionClassLibraryPtr, uint32_t, InitializationLevelContextRef *);
	Error borrow_deinit_level(const GDExtensionDeviceExitAbi *, GDExtensionClassLibraryPtr, uint32_t, DeinitLevelContextRef *);
	Error borrow_level_cleanup(const GDExtensionDeviceExitAbi *, InitializationLevelContextRef, uint32_t, RegisteredCleanupRef *);
	Error after_binding_initialize(const GDExtensionDeviceExitAbi *, InitializationLevelContextRef);
	Error begin_user_terminator(const GDExtensionDeviceExitAbi *, GuardToken, DeinitLevelContextRef);
	Error after_user_terminator(const GDExtensionDeviceExitAbi *, DeinitLevelContextRef);
	Error after_binding_deinit(const GDExtensionDeviceExitAbi *, DeinitLevelContextRef);
	Error publish_exit_result(const GDExtensionDeviceExitAbi *, GuardToken, DeinitPreflightContextRef, const ExitDiagnosticV1 *);
	Error acquire_code_hold(const GDExtensionDeviceExitAbi *, GuardedLibraryRef, RegisteredCleanupRef, LibraryCodeHoldRef *);
	Error clone_code_hold(const GDExtensionDeviceExitAbi *, LibraryCodeHoldRef, LibraryCodeHoldRef *);
	Error release_code_hold(const GDExtensionDeviceExitAbi *, LibraryCodeHoldRef);
	[[noreturn]] void terminate_initialize_blocked(InitializationLevelContextRef, int32_t);
	[[noreturn]] void terminate_blocked(DeinitLevelContextRef, int32_t);

	int32_t bootstrap(const GDExtensionDeviceExitAbi *p_request, GDExtensionClassLibraryPtr p_library, GDExtensionDeviceExitLibraryCapability *r_library);
	int32_t retain(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryCapability p_library, GDExtensionDeviceExitLibraryViewRef *r_view);
	int32_t copy(GDExtensionDeviceExitLibraryViewRef p_view, GDExtensionDeviceExitLibrarySnapshot *r_snapshot);
	int32_t release(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryViewRef p_view);
	int32_t clone(const GDExtensionDeviceExitAbi *p_request, GDExtensionDeviceExitLibraryViewRef p_source, GDExtensionDeviceExitLibraryViewRef *r_view);
};
} // namespace GDExtensionDeviceExit
