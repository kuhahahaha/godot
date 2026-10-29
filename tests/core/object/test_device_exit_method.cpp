/**************************************************************************/
/*  test_device_exit_method.cpp                                           */
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

#include "core/extension/gdextension.h"
#include "core/extension/gdextension_function_loader.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/object/class_db.h"
#include "core/object/gdvirtual.gen.h"
#include "core/object/method_bind.h"
#include "core/os/os.h"
#include "core/variant/method_ptrcall.h"
#include "tests/test_macros.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

TEST_FORCE_LINK(test_device_exit_method)

namespace ObjectAssociationTest {
extern void (*before_direct_publication)();
}

namespace TestDeviceExitEntry {
Error register_authenticated_lifetime(Object *object, const GDExtensionDeviceExit::RecordRef &record, uint64_t thread, uint64_t action);
void set_resident_method_gate_closed(GDExtension &extension, bool closed);
void set_resident_method_quiescing(GDExtension &extension, bool closed);
uint32_t exhaust_method_refs(GDExtension &extension, uint32_t reserve);
void restore_method_refs(GDExtension &extension, uint32_t limit);
} //namespace TestDeviceExitEntry

namespace TestDeviceExitMethod {
static GDExtension *owner = nullptr;
static Error close_result = OK;
static int calls = 0;
static bool close_in_body = false;
static GDExtensionVariantType return_type = GDEXTENSION_VARIANT_TYPE_INT;
static GDExtensionClassMethodArgumentMetadata return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64;
static StringName return_class;
static bool returns_value = true;
static bool missing_return = false;
static bool skip_method_registration = false;
static GDExtensionInterfaceGetProcAddress fixture_get = nullptr;
static GDExtensionInterfaceClassdbUnregisterExtensionClass unregister_class = nullptr;
class CountingFunctionLoader : public GDExtensionFunctionLoader {
public:
	uint32_t close_calls = 0;
	Error close_library() override {
		close_calls++;
		return GDExtensionFunctionLoader::close_library();
	}
};
static void body() {
	calls++;
	if (close_in_body) {
		close_result = owner->close_library();
	}
}
static void call(void *, GDExtensionClassInstancePtr, const GDExtensionConstVariantPtr *, GDExtensionInt, GDExtensionVariantPtr ret, GDExtensionCallError *error) {
	body();
	*static_cast<Variant *>(ret) = int64_t(73);
	error->error = GDEXTENSION_CALL_OK;
}
static void ptrcall(void *, GDExtensionClassInstancePtr, const GDExtensionConstTypePtr *, GDExtensionTypePtr ret) {
	body();
	*static_cast<int64_t *>(ret) = 73;
}
static GDExtensionBool object_set(GDExtensionClassInstancePtr, GDExtensionConstStringNamePtr, GDExtensionConstVariantPtr) {
	body();
	return true;
}
static GDExtensionBool object_get(GDExtensionClassInstancePtr, GDExtensionConstStringNamePtr, GDExtensionVariantPtr ret) {
	body();
	*static_cast<Variant *>(ret) = int64_t(73);
	return true;
}
static const GDExtensionPropertyInfo *listed_property = nullptr;
static int list_frees = 0;
static const GDExtensionPropertyInfo *object_list(GDExtensionClassInstancePtr, uint32_t *count) {
	body();
	*count = 1;
	return listed_property;
}
static void object_list_free(GDExtensionClassInstancePtr, const GDExtensionPropertyInfo *list, uint32_t count) {
	CHECK_EQ(list, listed_property);
	CHECK_EQ(count, 1);
	body();
	list_frees++;
}
static GDExtensionBool object_validate(GDExtensionClassInstancePtr, GDExtensionPropertyInfo *info) {
	body();
	info->usage = PROPERTY_USAGE_READ_ONLY;
	return true;
}
static GDExtensionBool object_can_revert(GDExtensionClassInstancePtr, GDExtensionConstStringNamePtr) {
	body();
	return true;
}
static void object_string(GDExtensionClassInstancePtr, GDExtensionBool *valid, GDExtensionStringPtr ret) {
	body();
	*valid = true;
	*static_cast<String *>(ret) = "K03 extension string";
}
static void object_notification(GDExtensionClassInstancePtr, int32_t, GDExtensionBool) {
	body();
}
static bool virtual_data_mode = false;
static bool constructor_mode = false;
static GDExtensionObjectPtr object_create(void *, GDExtensionBool) {
	body();
	return memnew(Object);
}
static GDExtensionClassInstancePtr object_recreate(void *, GDExtensionObjectPtr object) {
	body();
	return object;
}
static void virtual_call(GDExtensionClassInstancePtr, const GDExtensionConstTypePtr *, GDExtensionTypePtr ret) {
	body();
	*static_cast<int64_t *>(ret) = 73;
}
static GDExtensionClassCallVirtual virtual_discover(void *, GDExtensionConstStringNamePtr, uint32_t) {
	body();
	return virtual_call;
}
static void *virtual_discover_data(void *, GDExtensionConstStringNamePtr, uint32_t) {
	body();
	return &virtual_data_mode;
}
static void virtual_call_data(GDExtensionClassInstancePtr instance, GDExtensionConstStringNamePtr, void *data, const GDExtensionConstTypePtr *args, GDExtensionTypePtr ret) {
	CHECK_EQ(data, &virtual_data_mode);
	virtual_call(instance, args, ret);
}
class VirtualProbe : public Object {
public:
	ObjectGDExtension *descriptor() const { return const_cast<ObjectGDExtension *>(_get_extension()); }
	GDVIRTUAL0R(int64_t, _k03_probe)
	bool invoke(int64_t &ret) { return GDVIRTUAL_CALL(_k03_probe, ret); }
	bool overridden() const { return GDVIRTUAL_IS_OVERRIDDEN(_k03_probe); }
};
static void initialize(void *, GDExtensionInitializationLevel) {}
static void deinitialize(void *, GDExtensionInitializationLevel) {}
static void register_probe(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr library) {
	const auto register_method = reinterpret_cast<GDExtensionInterfaceClassdbRegisterExtensionClassMethod>(get("classdb_register_extension_class_method"));
	StringName name("DeviceExitMethodFixture"), method("probe");
	StringName empty_name;
	String empty_hint;
	GDExtensionPropertyInfo abi_return = { return_type, &empty_name, &return_class, PROPERTY_HINT_NONE, &empty_hint, PROPERTY_USAGE_DEFAULT };
	GDExtensionClassMethodInfo info = {};
	info.name = &method;
	info.call_func = call;
	info.ptrcall_func = ptrcall;
	info.method_flags = GDEXTENSION_METHOD_FLAG_STATIC;
	info.has_return_value = returns_value;
	info.return_value_info = missing_return ? nullptr : &abi_return;
	info.return_value_metadata = return_metadata;
	register_method(library, &name, &info);
}
static GDExtensionBool entry(GDExtensionInterfaceGetProcAddress get, GDExtensionClassLibraryPtr library, GDExtensionInitialization *result) {
	owner = static_cast<GDExtension *>(library);
	*result = {};
	result->initialize = initialize;
	result->deinitialize = deinitialize;
	const auto register_class = reinterpret_cast<GDExtensionInterfaceClassdbRegisterExtensionClass6>(get("classdb_register_extension_class6"));
	unregister_class = reinterpret_cast<GDExtensionInterfaceClassdbUnregisterExtensionClass>(get("classdb_unregister_extension_class"));
	StringName name("DeviceExitMethodFixture"), parent("Object"), method("probe");
	GDExtensionClassCreationInfo6 class_info = {};
	class_info.is_abstract = true;
	if (constructor_mode) {
		class_info.is_abstract = false;
		class_info.is_exposed = true;
		class_info.create_instance_func = object_create;
	}
	class_info.set_func = object_set;
	class_info.get_func = object_get;
	class_info.get_property_list_func = object_list;
	class_info.free_property_list_func = object_list_free;
	class_info.validate_property_func = object_validate;
	class_info.property_can_revert_func = object_can_revert;
	class_info.property_get_revert_func = object_get;
	class_info.to_string_func = object_string;
	class_info.notification_func = object_notification;
	class_info.recreate_instance_func = object_recreate;
	if (virtual_data_mode) {
		class_info.get_virtual_call_data_func = virtual_discover_data;
		class_info.call_virtual_with_data_func = virtual_call_data;
	} else {
		class_info.get_virtual_func = virtual_discover;
	}
	register_class(library, &name, &parent, &class_info);
	fixture_get = get;
	if (!skip_method_registration) {
		register_probe(get, library);
	}
	return true;
}

// Isolated child: reentrant publication must stop before recreating/storing.
static Object *association_target = nullptr;
static ObjectGDExtension *association_descriptor = nullptr;
static void direct_association_child() {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("libgodot://direct-association", loader) != OK) {
		std::_Exit(91);
	}
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *source = memnew(VirtualProbe);
	ClassDB::set_object_extension_instance(source, name, source);
	association_descriptor = source->descriptor();
	Ref<RefCounted> target = memnew(RefCounted);
	association_target = target.ptr();
	const bool recreate = OS::get_singleton()->get_cmdline_args().find("--direct-recreate") != nullptr;
	const GDExtensionInstanceBindingCallbacks callbacks = {
		[](void *, void *instance) -> void * { return instance; }, nullptr,
		[](void *, void *, GDExtensionBool) -> GDExtensionBool {
			if (OS::get_singleton()->get_cmdline_args().find("--direct-recreate") != nullptr) {
#ifdef TOOLS_ENABLED
				association_target->reset_internal_extension(association_descriptor);
#endif
			} else {
				ClassDB::set_object_extension_instance(association_target, "DeviceExitMethodFixture", association_target);
			}
			return true;
		}
	};
	if (target->get_instance_binding(extension.ptr(), &callbacks) != target.ptr()) {
		std::_Exit(92);
	}
	if (!target->reference()) {
		std::_Exit(93);
	}
	// Missing reservation reaches this sentinel; no unsafe destruction follows.
	std::fprintf(stderr, "DIRECT_ASSOCIATION_UNGUARDED recreate=%d\n", int(recreate));
	std::_Exit(0);
}
REGISTER_TEST_COMMAND("--device-exit-direct-association", direct_association_child);

static void direct_repeat_closed_child() {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("libgodot://direct-repeat-closed", loader) != OK) {
		std::_Exit(91);
	}
	VirtualProbe *object = memnew(VirtualProbe);
	ClassDB::set_object_extension_instance(object, "DeviceExitMethodFixture", object);
	if (extension->close_library() != ERR_BUSY) {
		std::_Exit(92);
	}
	ClassDB::set_object_extension_instance(object, "DeviceExitMethodFixture", object);
	std::fprintf(stderr, "UNEXPECTED_CLOSED_REPEAT\n");
	std::_Exit(93);
}
REGISTER_TEST_COMMAND("--device-exit-direct-repeat-closed", direct_repeat_closed_child);

static GDExtensionDeviceExit::RecordRef *adoption_record = nullptr;
static void adoption_quiesce_child() {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library("libgodot://adoption-quiesce", loader) != OK) {
		std::_Exit(91);
	}
	GDExtensionDeviceExit::RecordRef record;
	if (GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record) != OK) {
		std::_Exit(92);
	}
	adoption_record = &record;
	ObjectAssociationTest::before_direct_publication = []() {
		if (GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(*adoption_record) != ERR_BUSY) {
			std::_Exit(93);
		}
	};
	VirtualProbe *object = memnew(VirtualProbe);
	ClassDB::set_object_extension_instance(object, "DeviceExitMethodFixture", object);
	// Safe missing-gate sentinel; do not destruct an unqualified association.
	std::fprintf(stderr, "ADOPTION_PUBLICATION_UNGUARDED\n");
	std::_Exit(0);
}
REGISTER_TEST_COMMAND("--device-exit-adoption-quiesce", adoption_quiesce_child);

static int direct_holder_frees = 0;
TEST_CASE("device_exit::object/registered-direct-predelete-and-free-after-quiesce") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://registered-direct-cleanup", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *object = memnew(VirtualProbe);
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 45), OK);
	ClassDB::set_object_extension_instance(object, name, object);
	ObjectGDExtension *descriptor = object->descriptor();
	const auto original_free = descriptor->free_instance;
	descriptor->free_instance = [](void *, void *) {
		direct_holder_frees++;
		CHECK_EQ(owner->close_library(), ERR_BUSY);
	};
	const auto original_notification = descriptor->notification2;
	static int cleanup_notifications = 0;
	cleanup_notifications = direct_holder_frees = 0;
	descriptor->notification2 = [](void *instance, int32_t what, GDExtensionBool) {
		if (what == Object::NOTIFICATION_PREDELETE || what == Object::NOTIFICATION_PREDELETE_CLEANUP) {
			cleanup_notifications++;
			CHECK_EQ(owner->close_library(), ERR_BUSY);
			static bool reentering = false;
			if (!reentering) {
				reentering = true;
				static_cast<Object *>(instance)->notification(what, true);
				reentering = false;
			}
		}
	};
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record), ERR_BUSY);
	memdelete(object);
	CHECK_EQ(cleanup_notifications, 2);
	CHECK_EQ(direct_holder_frees, 1);
	descriptor->notification2 = original_notification;
	descriptor->free_instance = original_free;
	CHECK_EQ(extension->close_library(), OK);
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
}
#ifdef TOOLS_ENABLED
TEST_CASE("device_exit::object/registered-direct-clear-after-quiesce") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://registered-direct-clear", loader), OK);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *object = memnew(VirtualProbe);
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 46), OK);
	ClassDB::set_object_extension_instance(object, name, object);
	ObjectGDExtension *descriptor = object->descriptor();
	const auto original_free = descriptor->free_instance;
	direct_holder_frees = 0;
	descriptor->free_instance = [](void *, void *) {
		direct_holder_frees++;
		CHECK_EQ(owner->close_library(), ERR_BUSY);
	};
	CHECK_EQ(GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(record), ERR_BUSY);
	object->clear_internal_extension();
	CHECK_EQ(direct_holder_frees, 1);
	descriptor->free_instance = original_free;
	CHECK_EQ(extension->close_library(), OK);
	memdelete(object);
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
}

static GDExtension *clear_binding_owner = nullptr;
static int clear_callback_order = 0;
static bool clear_missing_owner = false;
static GDExtensionBool empty_entry(GDExtensionInterfaceGetProcAddress, GDExtensionClassLibraryPtr, GDExtensionInitialization *result) {
	*result = {};
	result->initialize = initialize;
	result->deinitialize = deinitialize;
	return true;
}
static bool clear_all_targets(bool same_generation, bool missing_owner) {
	Ref<GDExtensionFunctionLoader> direct_loader;
	direct_loader.instantiate();
	direct_loader->set_initialization_function(entry);
	Ref<GDExtension> direct;
	direct.instantiate();
	if (direct->open_library("libgodot://clear-direct-owner", direct_loader) != OK) {
		return false;
	}
	Ref<GDExtensionFunctionLoader> binding_loader;
	binding_loader.instantiate();
	binding_loader->set_initialization_function(empty_entry);
	Ref<GDExtension> binding;
	binding.instantiate();
	if (binding->open_library("libgodot://clear-binding-owner", binding_loader) != OK) {
		return false;
	}
	clear_binding_owner = same_generation ? direct.ptr() : binding.ptr();
	GDExtensionDeviceExit::RecordRef direct_record, binding_record;
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	if (registry.retain_binding_owner(direct.ptr(), direct_record) != OK ||
			registry.retain_binding_owner(clear_binding_owner, binding_record) != OK) {
		return false;
	}
	VirtualProbe *object = memnew(VirtualProbe);
	bool good = TestDeviceExitEntry::register_authenticated_lifetime(object, direct_record, Thread::get_caller_id(), 61) == OK;
	if (!same_generation && !missing_owner) {
		good = TestDeviceExitEntry::register_authenticated_lifetime(object, binding_record, Thread::get_caller_id(), 62) == OK && good;
	}
	StringName name("DeviceExitMethodFixture");
	ClassDB::set_object_extension_instance(object, name, object);
	ObjectGDExtension *descriptor = object->descriptor();
	const auto original_free = descriptor->free_instance;
	clear_callback_order = 0;
	clear_missing_owner = missing_owner;
	descriptor->free_instance = [](void *, void *) {
		if (clear_missing_owner) {
			std::fputs("CLEAR_BODY_BEFORE_ALL_TARGET_ADMISSION\n", stderr);
			std::fflush(stderr);
			std::_Exit(98);
		}
		clear_callback_order = clear_callback_order * 10 + 1;
		CHECK_EQ(owner->close_library(), ERR_BUSY);
		CHECK_EQ(clear_binding_owner->close_library(), ERR_BUSY);
	};
	const GDExtensionInstanceBindingCallbacks callbacks = {
		nullptr,
		[](void *, void *, void *) {
			clear_callback_order = clear_callback_order * 10 + 2;
			CHECK_EQ(owner->close_library(), ERR_BUSY);
			CHECK_EQ(clear_binding_owner->close_library(), ERR_BUSY);
		},
		nullptr
	};
	object->set_instance_binding(clear_binding_owner, object, &callbacks);
	good = registry.begin_quiesce(direct_record) == ERR_BUSY && good;
	good = registry.begin_quiesce(binding_record) == ERR_BUSY && good;
	object->clear_internal_extension();
	good = clear_callback_order == 12 && good;
	descriptor->free_instance = original_free;
	good = direct->close_library() == OK && good;
	good = binding->close_library() == OK && good;
	memdelete(object);
	unregister_class(direct.ptr(), &name);
	owner = clear_binding_owner = nullptr;
	return good;
}
TEST_CASE("device_exit::object/direct-clear-preflights-all-callback-generations") {
	CHECK(clear_all_targets(true, false));
	CHECK(clear_all_targets(false, false));
}
static void clear_missing_owner_child() {
	(void)clear_all_targets(false, true);
	std::_Exit(99);
}
REGISTER_TEST_COMMAND("--device-exit-clear-missing-owner", clear_missing_owner_child);
#endif // TOOLS_ENABLED

TEST_CASE("device_exit::object/direct-instance-holder-survives-to-real-free-return") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://direct-holder", loader), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *object = memnew(VirtualProbe);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 801), OK);
	ClassDB::set_object_extension_instance(object, name, object);
	ObjectGDExtension *descriptor = object->descriptor();
	const auto original_free = descriptor->free_instance;
	descriptor->free_instance = [](void *, void *) {
		direct_holder_frees++;
		close_result = owner->close_library();
	};
	direct_holder_frees = 0;
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	// A resident RED close can logically detach; this callback must still be
	// observed, never treated as native unload safety evidence.
	close_result = OK;
	memdelete(object);
	CHECK_EQ(direct_holder_frees, 1);
	CHECK_EQ(close_result, ERR_BUSY);
	descriptor->free_instance = original_free;
	if (extension->is_library_open()) {
		CHECK_EQ(extension->close_library(), OK);
	}
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
}

TEST_CASE("device_exit::object/direct-association-auto-enrolls-cleanup") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://direct-auto-enroll", loader), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *object = memnew(VirtualProbe);
	ClassDB::set_object_extension_instance(object, name, object);
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
}

TEST_CASE("device_exit::object/direct-association-repeat-is-idempotent") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://direct-repeat", loader), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *object = memnew(VirtualProbe);
	ClassDB::set_object_extension_instance(object, name, object);
	// Repeating the exact public association is an idempotent operation.  It
	// must not retire the live holder without the real disposal callback.
	ClassDB::set_object_extension_instance(object, name, object);
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
}

TEST_CASE("device_exit::object/get-set-frame-and-closed-refusal") {
	for (bool getter : { false, true }) {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://object-properties", loader), OK);
		Object *object = memnew(Object);
		StringName name("DeviceExitMethodFixture"), property("k03_probe");
		GDExtensionDeviceExit::RecordRef record;
		REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
		REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 802 + getter), OK);
		ClassDB::set_object_extension_instance(object, name, object);
		close_in_body = true;
		calls = 0;
		close_result = OK;
		bool valid = false;
		if (getter) {
			CHECK_EQ(int64_t(object->get(property, &valid)), 73);
		} else {
			object->set(property, 91, &valid);
		}
		CHECK(valid);
		CHECK_EQ(calls, 1);
		CHECK_EQ(close_result, ERR_BUSY);
		close_in_body = false;
		if (extension->is_library_open()) {
			CHECK_EQ(extension->close_library(), ERR_BUSY);
		}
		calls = 0;
		valid = true;
		if (getter) {
			CHECK_EQ(object->get(property, &valid).get_type(), Variant::NIL);
		} else {
			object->set(property, 91, &valid);
		}
		CHECK_FALSE(valid);
		CHECK_EQ(calls, 0);
		memdelete(object);
		CHECK_EQ(extension->close_library(), OK);
		unregister_class(extension.ptr(), &name);
		owner = nullptr;
	}
}

TEST_CASE("device_exit::object/property-cohorts-and-notifications") {
	for (int route = 0; route < 7; route++) {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://object-cohorts", loader), OK);
		Object *object = memnew(Object);
		StringName name("DeviceExitMethodFixture"), property("k03_listed"), empty;
		GDExtensionDeviceExit::RecordRef record;
		REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
		REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 810 + route), OK);
		String hint;
		GDExtensionPropertyInfo info = { GDEXTENSION_VARIANT_TYPE_INT, &property, &empty, PROPERTY_HINT_NONE, &hint, PROPERTY_USAGE_DEFAULT };
		listed_property = &info;
		ClassDB::set_object_extension_instance(object, name, object);
		for (bool closed : { false, true }) {
			if (closed && extension->is_library_open()) {
				CHECK_EQ(extension->close_library(), ERR_BUSY);
			}
			close_in_body = !closed;
			close_result = OK;
			calls = 0;
			list_frees = 0;
			if (route == 0) {
				List<PropertyInfo> properties;
				object->get_property_list(&properties);
				int found = 0;
				for (const PropertyInfo &pi : properties) {
					if (pi.name == property) {
						found++;
						CHECK_EQ(pi.type, Variant::INT);
					}
				}
				CHECK_EQ(found, closed ? 0 : 1);
				CHECK_EQ(list_frees, closed ? 0 : 1);
			} else if (route == 1) {
				PropertyInfo pi(Variant::INT, property);
				const auto original_usage = pi.usage;
				object->validate_property(pi);
				CHECK_EQ(pi.usage, closed ? original_usage : decltype(original_usage)(PROPERTY_USAGE_READ_ONLY));
			} else if (route == 2) {
				CHECK_EQ(object->property_can_revert(property), !closed);
			} else if (route == 3) {
				Variant ret = object->property_get_revert(property);
				if (closed) {
					CHECK_EQ(ret.get_type(), Variant::NIL);
				} else {
					CHECK_EQ(int64_t(ret), 73);
				}
			} else if (route == 4) {
				String result = object->to_string();
				CHECK_EQ(result == "K03 extension string", !closed);
				if (closed) {
					CHECK(result.contains(itos(object->get_instance_id())));
				}
			} else {
				object->notification(4242, route == 6);
			}
			CHECK_EQ(calls, closed ? 0 : route == 0 ? 2
													: 1);
			if (!closed) {
				CHECK_EQ(close_result, ERR_BUSY);
			}
		}
		close_in_body = false;
		memdelete(object);
		CHECK_EQ(extension->close_library(), OK);
		unregister_class(extension.ptr(), &name);
		listed_property = nullptr;
		owner = nullptr;
	}
}
#ifdef TOOLS_ENABLED
TEST_CASE("device_exit::object/native-placeholder-keeps-core-property-access") {
	ObjectGDExtension *placeholder = ClassDB::get_placeholder_extension("Node3D");
	REQUIRE(placeholder != nullptr);
	Object *object = static_cast<Object *>(placeholder->create_instance3(placeholder->class_userdata, false));
	REQUIRE(object != nullptr);
	bool valid = false;
	object->set("metadata/k03_probe", 91, &valid);
	CHECK(valid);
	CHECK_EQ(int64_t(object->get("metadata/k03_probe", &valid)), 91);
	CHECK(valid);
	memdelete(object);
}
#endif

TEST_CASE("device_exit::object/virtual-discovery-and-generated-cached-invocation") {
	for (bool with_data : { false, true }) {
		for (bool warm : { false, true }) {
			virtual_data_mode = with_data;
			Ref<GDExtensionFunctionLoader> loader;
			loader.instantiate();
			loader->set_initialization_function(entry);
			Ref<GDExtension> extension;
			extension.instantiate();
			REQUIRE_EQ(extension->open_library("libgodot://object-virtual", loader), OK);
			VirtualProbe *object = memnew(VirtualProbe);
			StringName name("DeviceExitMethodFixture");
			GDExtensionDeviceExit::RecordRef record;
			REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
			REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(object, record, Thread::get_caller_id(), 820 + (with_data ? 2 : 0) + (warm ? 1 : 0)), OK);
			ClassDB::set_object_extension_instance(object, name, object);
			if (warm) {
				close_in_body = false;
				calls = 0;
				int64_t ret = 99;
				CHECK(object->invoke(ret));
				CHECK_EQ(ret, 73);
				CHECK_EQ(calls, 2);
				calls = 0;
				CHECK(object->overridden());
				CHECK(object->invoke(ret));
				CHECK_EQ(ret, 73);
				CHECK_EQ(calls, 1);
				close_in_body = true;
				calls = 0;
				// This admitted callback seals the generation, then returns.
				CHECK(object->invoke(ret));
				CHECK_EQ(ret, 73);
				CHECK_EQ(calls, 1);
				CHECK_EQ(close_result, ERR_BUSY);
				calls = 0;
				// The next invocation must fail before entering the cached body.
				CHECK_FALSE(object->overridden());
				CHECK_FALSE(object->invoke(ret));
				CHECK_EQ(ret, 0);
				CHECK_EQ(calls, 0);
			}
			close_in_body = false;
			if (extension->is_library_open()) {
				CHECK_EQ(extension->close_library(), ERR_BUSY);
			}
			calls = 0;
			int64_t ret = 99;
			CHECK_FALSE(object->invoke(ret));
			CHECK_EQ(ret, 0);
			CHECK_FALSE(object->overridden());
			CHECK_EQ(calls, 0);
			memdelete(object);
			CHECK_EQ(extension->close_library(), OK);
			unregister_class(extension.ptr(), &name);
			owner = nullptr;
		}
	}
	virtual_data_mode = false;
}

TEST_CASE("device_exit::object/virtual-quota-refusal-keeps-cold-cache-retryable") {
	for (bool with_data : { false, true }) {
		for (uint32_t reserve : { 0u, 1u }) {
			virtual_data_mode = with_data;
			Ref<GDExtensionFunctionLoader> loader;
			loader.instantiate();
			loader->set_initialization_function(entry);
			Ref<GDExtension> extension;
			extension.instantiate();
			REQUIRE_EQ(extension->open_library("libgodot://virtual-quota", loader), OK);
			VirtualProbe *object = memnew(VirtualProbe);
			StringName name("DeviceExitMethodFixture");
			ClassDB::set_object_extension_instance(object, name, object);
			const uint32_t limit = TestDeviceExitEntry::exhaust_method_refs(*extension.ptr(), reserve);
			calls = 0;
			int64_t ret = 99;
			CHECK_FALSE(object->invoke(ret));
			CHECK_EQ(ret, 0);
			CHECK_FALSE(object->overridden());
			CHECK_EQ(calls, 0);
			TestDeviceExitEntry::restore_method_refs(*extension.ptr(), limit);
			CHECK(object->invoke(ret));
			CHECK_EQ(ret, 73);
			CHECK_EQ(calls, 2);
			CHECK(object->overridden());
			memdelete(object);
			unregister_class(extension.ptr(), &name);
			CHECK_EQ(extension->close_library(), OK);
			owner = nullptr;
		}
	}
	virtual_data_mode = false;
}

TEST_CASE("device_exit::object/classdb-constructor-frame-and-closed-refusal") {
	constructor_mode = true;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://constructor", loader), OK);
	StringName name("DeviceExitMethodFixture");
	calls = 0;
	close_in_body = true;
	Object *object = ClassDB::instantiate_without_postinitialization(name);
	REQUIRE(object != nullptr);
	CHECK_EQ(calls, 1);
	CHECK_EQ(close_result, ERR_BUSY);
	memdelete(object);
	close_in_body = false;
	if (extension->is_library_open()) {
		REQUIRE_EQ(extension->close_library(), OK);
	}
	calls = 0;
	object = ClassDB::instantiate_without_postinitialization(name);
	CHECK(object == nullptr);
	CHECK_EQ(calls, 0);
	if (object) {
		memdelete(object);
	}
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
	constructor_mode = false;
}

#ifdef TOOLS_ENABLED
static int reload_primary_frees = 0;
static int reload_secondary_frees = 0;
TEST_CASE("device_exit::object/reload-primary-slot-preserves-secondary-binding") {
	for (int route : { 0, 1, 2 }) {
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://binding-reload-slot", loader), OK);
		StringName name("DeviceExitMethodFixture");
		VirtualProbe *object = memnew(VirtualProbe);
		ClassDB::set_object_extension_instance(object, name, object);
		int primary = 1, secondary = 2, replacement = 3;
		const GDExtensionInstanceBindingCallbacks callbacks = {
			[](void *, void *instance) -> void * { return instance; },
			[](void *, void *instance, void *binding) {
				if (instance == binding) {
					reload_secondary_frees++;
				} else {
					reload_primary_frees++;
				}
			},
			nullptr
		};
		reload_primary_frees = reload_secondary_frees = 0;
		if (route != 2) {
			object->set_instance_binding(&primary, &primary, &callbacks);
		}
		REQUIRE_EQ(object->get_instance_binding(&secondary, &callbacks), object);
		if (route == 1) {
			object->free_instance_binding(&primary);
			CHECK_EQ(reload_primary_frees, 1);
		}
		object->clear_internal_extension();
		CHECK_EQ(reload_primary_frees, route == 2 ? 0 : 1);
		CHECK_EQ(reload_secondary_frees, 0);
		ERR_PRINT_OFF;
		object->set_instance_binding(&primary, &replacement, &callbacks);
		ERR_PRINT_ON;
		CHECK_EQ(object->get_instance_binding(&primary, nullptr), &replacement);
		CHECK_EQ(object->get_instance_binding(&secondary, nullptr), object);
		memdelete(object);
		CHECK_EQ(reload_secondary_frees, 1);
		unregister_class(extension.ptr(), &name);
		REQUIRE_EQ(extension->close_library(), OK);
		owner = nullptr;
	}
}

static int discarded_recreates = 0;
static Error recreate_quiesce_result = OK;
TEST_CASE("device_exit::object/recreate-quiesce-discards-before-publication") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://recreate-quiesce", loader), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *source = memnew(VirtualProbe);
	ClassDB::set_object_extension_instance(source, name, source);
	ObjectGDExtension *descriptor = source->descriptor();
	source->clear_internal_extension();
	memdelete(source);
	GDExtensionDeviceExit::RecordRef record;
	REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), record), OK);
	const auto original_recreate = descriptor->recreate_instance;
	const auto original_free = descriptor->free_instance;
	void *original_userdata = descriptor->class_userdata;
	descriptor->class_userdata = &record;
	descriptor->recreate_instance = [](void *userdata, GDExtensionObjectPtr object) -> GDExtensionClassInstancePtr {
		recreate_quiesce_result = GDExtensionDeviceExit::Registry::get_singleton().begin_quiesce(*static_cast<GDExtensionDeviceExit::RecordRef *>(userdata));
		return object;
	};
	descriptor->free_instance = [](void *, void *) { discarded_recreates++; };
	discarded_recreates = 0;
	VirtualProbe *object = memnew(VirtualProbe);
	object->reset_internal_extension(descriptor);
	CHECK_EQ(recreate_quiesce_result, ERR_BUSY);
	CHECK(object->descriptor() == nullptr);
	CHECK_EQ(discarded_recreates, 1);
	// Resident-only restoration permits cleanup of an old failing fixture;
	// this is not a production reopen route or a native unload proof.
	TestDeviceExitEntry::set_resident_method_quiescing(*extension.ptr(), false);
	memdelete(object);
	descriptor->recreate_instance = original_recreate;
	descriptor->free_instance = original_free;
	descriptor->class_userdata = original_userdata;
	unregister_class(extension.ptr(), &name);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}

TEST_CASE("device_exit::object/recreate-success-auto-enrolls-cleanup") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://recreate-cleanup", loader), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *object = memnew(VirtualProbe);
	ClassDB::set_object_extension_instance(object, name, object);
	ObjectGDExtension *descriptor = object->descriptor();
	object->clear_internal_extension();
	close_in_body = false;
	object->reset_internal_extension(descriptor);
	CHECK_EQ(object->descriptor(), descriptor);
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	memdelete(object);
	CHECK_EQ(extension->close_library(), OK);
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
}

TEST_CASE("device_exit::object/recreate-frame-and-closed-refusal") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://recreate", loader), OK);
	StringName name("DeviceExitMethodFixture");
	VirtualProbe *source = memnew(VirtualProbe);
	GDExtensionDeviceExit::RecordRef source_record;
	REQUIRE_EQ(GDExtensionDeviceExit::Registry::get_singleton().retain_binding_owner(extension.ptr(), source_record), OK);
	REQUIRE_EQ(TestDeviceExitEntry::register_authenticated_lifetime(source, source_record, Thread::get_caller_id(), 830), OK);
	ClassDB::set_object_extension_instance(source, name, source);
	ObjectGDExtension *descriptor = source->descriptor();
	REQUIRE(descriptor != nullptr);
	const auto original_free = descriptor->free_instance;
	// Recreated instances require a free callback even when publication fails.
	// This resident fixture uses the native Object itself as instance data.
	descriptor->free_instance = [](void *, void *) {
		discarded_recreates++;
		CHECK_EQ(owner->close_library(), ERR_BUSY);
	};
	discarded_recreates = 0;
	VirtualProbe *object = memnew(VirtualProbe);
	calls = 0;
	close_in_body = true;
	object->reset_internal_extension(descriptor);
	CHECK_EQ(calls, 1);
	CHECK_EQ(close_result, ERR_BUSY);
	CHECK(object->descriptor() == nullptr);
	CHECK_EQ(discarded_recreates, 1);
	CHECK_EQ(String(GDExtensionDeviceExit::get_binding_entry_diagnostic()), "ENTRY_CLOSED");
	close_in_body = false;
	memdelete(object);
	CHECK_EQ(discarded_recreates, 1);
	CHECK_EQ(extension->close_library(), ERR_BUSY);
	object = memnew(VirtualProbe);
	calls = 0;
	object->reset_internal_extension(descriptor);
	CHECK_EQ(calls, 0);
	CHECK(object->descriptor() == nullptr);
	CHECK_EQ(discarded_recreates, 1);
	memdelete(object);
	memdelete(source);
	CHECK_EQ(discarded_recreates, 2);
	descriptor->free_instance = original_free;
	unregister_class(extension.ptr(), &name);
	CHECK_EQ(extension->close_library(), OK);
	owner = nullptr;
}
#endif

TEST_CASE("device_exit::method/every-cached-static-dispatch-keeps-frame-until-return") {
	for (int route = 0; route < 3; route++) {
		Ref<CountingFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://method-entry", loader), OK);
		MethodBind *method = ClassDB::get_method("DeviceExitMethodFixture", "probe");
		REQUIRE(method != nullptr);
		close_in_body = true;
		close_result = OK;
		calls = 0;
		if (route == 0) {
			Callable::CallError error;
			Variant ret = method->call(nullptr, nullptr, 0, error);
			CHECK_EQ(error.error, Callable::CallError::CALL_OK);
			CHECK_EQ(int64_t(ret), 73);
		} else if (route == 1) {
			Variant ret;
			method->validated_call(nullptr, nullptr, &ret);
			CHECK_EQ(int64_t(ret), 73);
		} else {
			int64_t ret = -1;
			method->ptrcall(nullptr, nullptr, &ret);
			CHECK_EQ(ret, 73);
		}
		CHECK_EQ(calls, 1);
		CHECK_EQ(close_result, ERR_BUSY);
		CHECK(extension->is_library_open());
		// A failed close attempt must seal normal admission before the callback
		// frame returns; the retry remains possible only after the frame retires.
		CHECK_EQ(extension->initialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), ERR_BUSY);
		close_in_body = false;
		StringName name("DeviceExitMethodFixture");
		unregister_class(extension.ptr(), &name);
		if (extension->is_library_open()) {
			CHECK_EQ(extension->close_library(), OK);
		}
		CHECK_EQ(loader->close_calls, 1);
		owner = nullptr;
	}
}
TEST_CASE("device_exit::method/closed-cached-dispatch-never-enters-body-and-encodes-int-default") {
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://method-closed", loader), OK);
	MethodBind *method = ClassDB::get_method("DeviceExitMethodFixture", "probe");
	REQUIRE(method != nullptr);
	close_in_body = false;
	calls = 0;
	REQUIRE_EQ(extension->close_library(), OK);
	Callable::CallError error;
	Variant ret = method->call(nullptr, nullptr, 0, error);
	CHECK_EQ(ret.get_type(), Variant::NIL);
	CHECK_EQ(error.error, Callable::CallError::CALL_ERROR_INVALID_METHOD);
	ret = int64_t(99);
	ERR_PRINT_OFF;
	method->validated_call(nullptr, nullptr, &ret);
	int64_t raw = 99;
	method->ptrcall(nullptr, nullptr, &raw);
	ERR_PRINT_ON;
	CHECK_EQ(ret.get_type(), Variant::INT);
	CHECK_EQ(int64_t(ret), 0);
	CHECK_EQ(raw, 0);
	CHECK_EQ(calls, 0);
	StringName name("DeviceExitMethodFixture");
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
}
template <typename T>
static void check_default(Variant::Type type, const T &seed, GDExtensionClassMethodArgumentMetadata metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE) {
	REQUIRE(Variant(seed) != Variant(T{}));
	return_type = GDExtensionVariantType(type);
	return_class = type == Variant::OBJECT ? StringName("Object") : StringName();
	return_metadata = metadata;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://method-default", loader), OK);
	MethodBind *method = ClassDB::get_method("DeviceExitMethodFixture", "probe");
	REQUIRE(method != nullptr);
	REQUIRE_EQ(extension->close_library(), OK);
	typename PtrToArg<T>::EncodeT raw{};
	PtrToArg<T>::encode(seed, &raw);
	Variant ret = int64_t(99);
	calls = 0;
	ERR_PRINT_OFF;
	method->ptrcall(nullptr, nullptr, &raw);
	method->validated_call(nullptr, nullptr, &ret);
	ERR_PRINT_ON;
	CHECK(Variant(PtrToArg<T>::convert(&raw)) == Variant(T{}));
	CHECK(ret == Variant(T{}));
	CHECK_EQ(calls, 0);
	StringName name("DeviceExitMethodFixture");
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
	return_type = GDEXTENSION_VARIANT_TYPE_INT;
	return_class = StringName();
	return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64;
}
TEST_CASE("device_exit::method/core-default-encoders-cover-every-builtin-return") {
	check_default<Variant>(Variant::NIL, Variant(99));
	check_default<bool>(Variant::BOOL, true);
	check_default<int64_t>(Variant::INT, 99);
	for (int metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT8; metadata <= GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_UINT64; metadata++) {
		check_default<int64_t>(Variant::INT, 99, GDExtensionClassMethodArgumentMetadata(metadata));
	}
	check_default<int64_t>(Variant::INT, 99, GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_CHAR16);
	check_default<int64_t>(Variant::INT, 99, GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_CHAR32);
	check_default<double>(Variant::FLOAT, 9.5);
	check_default<double>(Variant::FLOAT, 9.5, GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_FLOAT);
	check_default<double>(Variant::FLOAT, 9.5, GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_DOUBLE);
	check_default<String>(Variant::STRING, "seed");
	check_default<StringName>(Variant::STRING_NAME, StringName("seed"));
	check_default<NodePath>(Variant::NODE_PATH, NodePath("seed"));
	check_default<Vector2>(Variant::VECTOR2, Vector2(2, 3));
	check_default<Vector2i>(Variant::VECTOR2I, Vector2i(2, 3));
	check_default<Rect2>(Variant::RECT2, Rect2(2, 3, 4, 5));
	check_default<Rect2i>(Variant::RECT2I, Rect2i(2, 3, 4, 5));
	check_default<Vector3>(Variant::VECTOR3, Vector3(2, 3, 4));
	check_default<Vector3i>(Variant::VECTOR3I, Vector3i(2, 3, 4));
	check_default<Transform2D>(Variant::TRANSFORM2D, Transform2D(0.5, Vector2(2, 3)));
	check_default<Vector4>(Variant::VECTOR4, Vector4(2, 3, 4, 5));
	check_default<Vector4i>(Variant::VECTOR4I, Vector4i(2, 3, 4, 5));
	check_default<Plane>(Variant::PLANE, Plane(Vector3(1, 0, 0), 3));
	check_default<Quaternion>(Variant::QUATERNION, Quaternion(1, 2, 3, 4));
	check_default<AABB>(Variant::AABB, AABB(Vector3(2, 3, 4), Vector3(5, 6, 7)));
	check_default<Basis>(Variant::BASIS, Basis(Vector3(2, 0, 0), Vector3(0, 3, 0), Vector3(0, 0, 4)));
	check_default<Transform3D>(Variant::TRANSFORM3D, Transform3D(Basis(), Vector3(2, 3, 4)));
	check_default<Projection>(Variant::PROJECTION, Projection(Transform3D(Basis(), Vector3(2, 3, 4))));
	check_default<Color>(Variant::COLOR, Color(0.2, 0.3, 0.4, 0.5));
	check_default<RID>(Variant::RID, RID::from_uint64(73));
	Object *sentinel = memnew(Object);
	check_default<Callable>(Variant::CALLABLE, Callable(sentinel, "probe"));
	check_default<Signal>(Variant::SIGNAL, Signal(sentinel, "probe"));
	Dictionary dictionary;
	dictionary["seed"] = 73;
	check_default<Dictionary>(Variant::DICTIONARY, dictionary);
	Array array;
	array.push_back(73);
	check_default<Array>(Variant::ARRAY, array);
	check_default<PackedByteArray>(Variant::PACKED_BYTE_ARRAY, PackedByteArray{ 73 });
	check_default<PackedInt32Array>(Variant::PACKED_INT32_ARRAY, PackedInt32Array{ 73 });
	check_default<PackedInt64Array>(Variant::PACKED_INT64_ARRAY, PackedInt64Array{ 73 });
	check_default<PackedFloat32Array>(Variant::PACKED_FLOAT32_ARRAY, PackedFloat32Array{ 7.5 });
	check_default<PackedFloat64Array>(Variant::PACKED_FLOAT64_ARRAY, PackedFloat64Array{ 7.5 });
	check_default<PackedStringArray>(Variant::PACKED_STRING_ARRAY, PackedStringArray{ "seed" });
	check_default<PackedVector2Array>(Variant::PACKED_VECTOR2_ARRAY, PackedVector2Array{ Vector2(2, 3) });
	check_default<PackedVector3Array>(Variant::PACKED_VECTOR3_ARRAY, PackedVector3Array{ Vector3(2, 3, 4) });
	check_default<PackedColorArray>(Variant::PACKED_COLOR_ARRAY, PackedColorArray{ Color(0.2, 0.3, 0.4) });
	check_default<PackedVector4Array>(Variant::PACKED_VECTOR4_ARRAY, PackedVector4Array{ Vector4(2, 3, 4, 5) });
	check_default<Object *>(Variant::OBJECT, sentinel);
	check_default<Object *>(Variant::OBJECT, sentinel, GDEXTENSION_METHOD_ARGUMENT_METADATA_OBJECT_IS_REQUIRED);
	memdelete(sentinel);
}
TEST_CASE("device_exit::method/closed-ref-return-releases-initialized-storage") {
	return_type = GDEXTENSION_VARIANT_TYPE_OBJECT;
	return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE;
	return_class = "RefCounted";
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://method-ref-default", loader), OK);
	MethodBind *method = ClassDB::get_method("DeviceExitMethodFixture", "probe");
	REQUIRE(method != nullptr);
	REQUIRE_EQ(extension->close_library(), OK);
	Ref<RefCounted> seed;
	seed.instantiate();
	Ref<RefCounted> raw = seed;
	REQUIRE_EQ(seed->get_reference_count(), 2);
	ERR_PRINT_OFF;
	method->ptrcall(nullptr, nullptr, &raw);
	ERR_PRINT_ON;
	CHECK(raw.is_null());
	CHECK_EQ(seed->get_reference_count(), 1);
	// A broken raw pointer overwrite loses this reference; repair only the test
	// fixture's extra count so the semantic RED does not leak into teardown.
	if (seed->get_reference_count() == 2) {
		seed->unreference();
	}
	StringName name("DeviceExitMethodFixture");
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
	return_class = StringName();
	return_type = GDEXTENSION_VARIANT_TYPE_INT;
	return_class = StringName();
	return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64;
}
TEST_CASE("device_exit::method/registration-rejects-incompatible-return-metadata") {
	// 14 is unnamed but within this enum's C++ value range [0, 15]. An
	// out-of-range value such as 99 makes the fixture itself undefined before
	// registration can inspect it, hiding the intended unknown-metadata check.
	for (const auto metadata : { GDEXTENSION_METHOD_ARGUMENT_METADATA_REAL_IS_FLOAT, GDEXTENSION_METHOD_ARGUMENT_METADATA_OBJECT_IS_REQUIRED, GDExtensionClassMethodArgumentMetadata(14) }) {
		return_metadata = metadata;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		ERR_PRINT_OFF;
		const Error opened = extension->open_library("libgodot://method-invalid-metadata", loader);
		ERR_PRINT_ON;
		REQUIRE_EQ(opened, OK);
		CHECK(ClassDB::get_method("DeviceExitMethodFixture", "probe") == nullptr);
		StringName name("DeviceExitMethodFixture");
		unregister_class(extension.ptr(), &name);
		CHECK_EQ(extension->close_library(), OK);
		owner = nullptr;
	}
	return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64;
}
TEST_CASE("device_exit::method/registration-rejects-missing-type-range-and-unknown-object-class") {
	for (int route = 0; route < 4; route++) {
		missing_return = route == 0;
		// Unknown value within the C++ enum range, so the rejection fixture
		// does not invoke undefined behavior before the ABI validation.
		return_type = route == 1 ? GDExtensionVariantType(63) : route == 2 ? GDEXTENSION_VARIANT_TYPE_VARIANT_MAX
																		   : GDEXTENSION_VARIANT_TYPE_OBJECT;
		return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE;
		return_class = route == 3 ? "DeviceExitUnknownReturnClass" : "Object";
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		ERR_PRINT_OFF;
		const Error opened = extension->open_library("libgodot://method-invalid-descriptor", loader);
		ERR_PRINT_ON;
		REQUIRE_EQ(opened, OK);
		CHECK(ClassDB::get_method("DeviceExitMethodFixture", "probe") == nullptr);
		StringName name("DeviceExitMethodFixture");
		unregister_class(extension.ptr(), &name);
		CHECK_EQ(extension->close_library(), OK);
		owner = nullptr;
	}
	missing_return = false;
	return_class = StringName();
	return_type = GDEXTENSION_VARIANT_TYPE_INT;
	return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64;
}
TEST_CASE("device_exit::method/quota-refusal-does-not-publish-and-cached-dispatch-retries") {
	for (uint32_t reserve : { 0u, 1u }) {
		skip_method_registration = true;
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://method-quota", loader), OK);
		const uint32_t limit = TestDeviceExitEntry::exhaust_method_refs(*extension.ptr(), reserve);
		ERR_PRINT_OFF;
		register_probe(fixture_get, extension.ptr());
		ERR_PRINT_ON;
		TestDeviceExitEntry::restore_method_refs(*extension.ptr(), limit);
		CHECK(ClassDB::get_method("DeviceExitMethodFixture", "probe") == nullptr);
		register_probe(fixture_get, extension.ptr());
		MethodBind *method = ClassDB::get_method("DeviceExitMethodFixture", "probe");
		REQUIRE(method != nullptr);
		const uint32_t call_limit = TestDeviceExitEntry::exhaust_method_refs(*extension.ptr(), 0);
		calls = 0;
		Callable::CallError error;
		Variant result = method->call(nullptr, nullptr, 0, error);
		Variant validated = 99;
		int64_t raw = 99;
		ERR_PRINT_OFF;
		method->validated_call(nullptr, nullptr, &validated);
		method->ptrcall(nullptr, nullptr, &raw);
		ERR_PRINT_ON;
		TestDeviceExitEntry::restore_method_refs(*extension.ptr(), call_limit);
		CHECK_EQ(result.get_type(), Variant::NIL);
		CHECK_EQ(error.error, Callable::CallError::CALL_ERROR_INVALID_METHOD);
		CHECK_EQ(int64_t(validated), 0);
		CHECK_EQ(raw, 0);
		CHECK_EQ(calls, 0);
		CHECK_EQ(int64_t(method->call(nullptr, nullptr, 0, error)), 73);
		CHECK_EQ(error.error, Callable::CallError::CALL_OK);
		method->validated_call(nullptr, nullptr, &validated);
		method->ptrcall(nullptr, nullptr, &raw);
		CHECK_EQ(int64_t(validated), 73);
		CHECK_EQ(raw, 73);
		CHECK_EQ(calls, 3);
		StringName name("DeviceExitMethodFixture");
		unregister_class(extension.ptr(), &name);
		CHECK_EQ(extension->close_library(), OK);
		owner = nullptr;
	}
	skip_method_registration = false;
}
TEST_CASE("device_exit::method/closed-void-does-not-write-return-storage") {
	returns_value = false;
	Ref<GDExtensionFunctionLoader> loader;
	loader.instantiate();
	loader->set_initialization_function(entry);
	Ref<GDExtension> extension;
	extension.instantiate();
	REQUIRE_EQ(extension->open_library("libgodot://method-void", loader), OK);
	MethodBind *method = ClassDB::get_method("DeviceExitMethodFixture", "probe");
	REQUIRE(method != nullptr);
	REQUIRE_EQ(extension->close_library(), OK);
	Variant variant = 99;
	uint64_t raw[3] = { 17, 23, 41 };
	calls = 0;
	ERR_PRINT_OFF;
	method->validated_call(nullptr, nullptr, &variant);
	method->ptrcall(nullptr, nullptr, &raw[1]);
	ERR_PRINT_ON;
	CHECK_EQ(int64_t(variant), 99);
	CHECK_EQ(raw[0], 17);
	CHECK_EQ(raw[1], 23);
	CHECK_EQ(raw[2], 41);
	CHECK_EQ(calls, 0);
	StringName name("DeviceExitMethodFixture");
	unregister_class(extension.ptr(), &name);
	owner = nullptr;
	returns_value = true;
}
#ifdef TOOLS_ENABLED
struct Access {
	static void prepare(GDExtension &extension) { extension.prepare_reload(); }
	static void finish(GDExtension &extension) { extension.finish_reload(); }
};
TEST_CASE("device_exit::method/reload-preserves-old-cached-return-abi-in-both-directions") {
	for (bool old_ref : { false, true }) {
		return_type = GDEXTENSION_VARIANT_TYPE_OBJECT;
		return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_NONE;
		return_class = old_ref ? "RefCounted" : "Object";
		Ref<GDExtensionFunctionLoader> loader;
		loader.instantiate();
		loader->set_initialization_function(entry);
		Ref<GDExtension> extension;
		extension.instantiate();
		REQUIRE_EQ(extension->open_library("libgodot://method-reload", loader), OK);
		MethodBind *old_method = ClassDB::get_method("DeviceExitMethodFixture", "probe");
		REQUIRE(old_method != nullptr);
		StringName name("DeviceExitMethodFixture");
		Access::prepare(*extension.ptr());
		unregister_class(extension.ptr(), &name);
		REQUIRE_EQ(extension->close_library(), OK);
		loader->set_initialization_function(entry);
		return_class = old_ref ? "Object" : "RefCounted";
		REQUIRE_EQ(extension->open_library("libgodot://method-reload", loader), OK);
		MethodBind *new_method = ClassDB::get_method(name, "probe");
		CHECK(new_method != old_method);
		CHECK_FALSE(old_method->is_valid());
		Access::finish(*extension.ptr());
		REQUIRE_EQ(extension->close_library(), OK);
		// Only execute the stale ABI after identity split succeeds: a regression
		// otherwise would reinterpret Object storage as Ref and corrupt lifetime.
		if (new_method != old_method) {
			Ref<RefCounted> seed;
			seed.instantiate();
			if (old_ref) {
				Ref<RefCounted> raw = seed;
				ERR_PRINT_OFF;
				old_method->ptrcall(nullptr, nullptr, &raw);
				ERR_PRINT_ON;
				CHECK(raw.is_null());
				CHECK_EQ(seed->get_reference_count(), 1);
			} else {
				Object *raw = seed.ptr();
				ERR_PRINT_OFF;
				old_method->ptrcall(nullptr, nullptr, &raw);
				ERR_PRINT_ON;
				CHECK(raw == nullptr);
				CHECK_EQ(seed->get_reference_count(), 1);
			}
		}
		unregister_class(extension.ptr(), &name);
		owner = nullptr;
	}
	return_class = StringName();
	return_type = GDEXTENSION_VARIANT_TYPE_INT;
	return_metadata = GDEXTENSION_METHOD_ARGUMENT_METADATA_INT_IS_INT64;
}

static bool native_probe_active = false;
static int native_probes = 0;
static int native_early_closes = 0;
static GDExtensionInterfaceGetProcAddress native_get = nullptr;
static void native_probe() {
	native_probe_active = true;
	Error error = owner->close_library();
	native_probe_active = false;
	native_probes++;
	if (error != ERR_BUSY || native_early_closes != 0) {
		std::_Exit(96);
	}
}
static GDExtensionStringNamePtr native_name(int index) {
	static StringName names[] = { "DeviceExitNativeMethod", "Object", "probe", "" };
	if (index < 0 || index > 3) {
		std::_Exit(97);
	}
	return &names[index];
}
static GDExtensionStringPtr native_hint() {
	static String hint;
	return &hint;
}
static GDExtensionInstanceBindingCallbacks native_binding_callbacks = {};
static void native_publish_binding(const GDExtensionInstanceBindingCallbacks *callbacks) {
	native_binding_callbacks = *callbacks;
}
static GDExtensionInterfaceFunctionPtr native_intercept(const char *name) {
	if (std::strcmp(name, "fixture_core_method_probe") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(native_probe);
	}
	if (std::strcmp(name, "fixture_core_method_name") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(native_name);
	}
	if (std::strcmp(name, "fixture_core_method_hint") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(native_hint);
	}
	if (std::strcmp(name, "fixture_core_binding_callbacks") == 0) {
		return reinterpret_cast<GDExtensionInterfaceFunctionPtr>(native_publish_binding);
	}
	return native_get(name);
}
class NativeMethodLoader : public GDExtensionLibraryLoader {
public:
	uint64_t generation = 0;
	Error initialize(GDExtensionInterfaceGetProcAddress get_proc_address, const Ref<GDExtension> &extension, GDExtensionInitialization *result) override {
		owner = extension.ptr();
		native_get = get_proc_address;
		auto bootstrap = reinterpret_cast<GDExtensionInterfaceDeviceExitBootstrapLibraryV1>(get_proc_address("device_exit_bootstrap_library_v1"));
		auto retain = reinterpret_cast<GDExtensionInterfaceDeviceExitRetainLibraryViewV1>(get_proc_address("device_exit_retain_library_view_v1"));
		auto copy = reinterpret_cast<GDExtensionInterfaceDeviceExitCopyLibrarySnapshotV1>(get_proc_address("device_exit_copy_library_snapshot_v1"));
		auto release = reinterpret_cast<GDExtensionInterfaceDeviceExitReleaseLibraryViewV1>(get_proc_address("device_exit_release_library_view_v1"));
		const GDExtensionDeviceExitAbi abi = { 1, 16, 0 };
		GDExtensionDeviceExitLibraryCapability authority = {};
		GDExtensionDeviceExitLibraryViewRef view = {};
		GDExtensionDeviceExitLibrarySnapshot snapshot = { { 1, 48, 0 }, 0, 0, 0, 0, 0, 0, 0 };
		if (!bootstrap || !retain || !copy || !release || bootstrap(&abi, extension.ptr(), &authority) != OK || retain(&abi, authority, &view) != OK) {
			std::_Exit(98);
		}
		const int32_t error = copy(view, &snapshot);
		if (release(&abi, view) != OK) {
			std::_Exit(98);
		}
		if (error != OK || snapshot.library_generation == 0) {
			std::_Exit(98);
		}
		generation = snapshot.library_generation;
		return GDExtensionLibraryLoader::initialize(native_intercept, extension, result);
	}
	Error close_library() override {
		if (native_probe_active) {
			native_early_closes++;
			return ERR_BUSY;
		}
		return GDExtensionLibraryLoader::close_library();
	}
};
static void native_method_child() {
	String path;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--device-exit-path=")) {
			path = arg.trim_prefix("--device-exit-path=");
		}
	}
	if (path.is_empty()) {
		std::_Exit(91);
	}
	OS::get_singleton()->unset_environment("DEVICE_EXIT_METHOD_FLOAT");
	Ref<NativeMethodLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library(path, loader) != OK) {
		std::_Exit(92);
	}
	const uint64_t old_generation = loader->generation;
	MethodBind *old_method = ClassDB::get_method("DeviceExitNativeMethod", "probe");
	if (!old_method) {
		std::_Exit(93);
	}
	Callable::CallError error;
	if (int64_t(old_method->call(nullptr, nullptr, 0, error)) != 73 || error.error != Callable::CallError::CALL_OK) {
		std::_Exit(94);
	}
	Variant variant;
	old_method->validated_call(nullptr, nullptr, &variant);
	int64_t raw = -1;
	old_method->ptrcall(nullptr, nullptr, &raw);
	if (int64_t(variant) != 73 || raw != 73) {
		std::_Exit(94);
	}
	Access::prepare(*extension.ptr());
	auto unregister = reinterpret_cast<GDExtensionInterfaceClassdbUnregisterExtensionClass>(native_get("classdb_unregister_extension_class"));
	StringName name("DeviceExitNativeMethod");
	unregister(extension.ptr(), &name);
	if (extension->close_library() != OK || loader->is_library_open()) {
		std::_Exit(95);
	}
	// The new ABI forces a distinct bind, preserving the old generation cache.
	OS::get_singleton()->set_environment("DEVICE_EXIT_METHOD_FLOAT", "1");
	if (extension->open_library(path, loader) != OK || loader->generation == old_generation) {
		std::_Exit(95);
	}
	MethodBind *new_method = ClassDB::get_method(name, "probe");
	if (!new_method || new_method == old_method || old_method->is_valid()) {
		std::_Exit(95);
	}
	Access::finish(*extension.ptr());
	variant = old_method->call(nullptr, nullptr, 0, error);
	if (variant.get_type() != Variant::NIL || error.error != Callable::CallError::CALL_ERROR_INVALID_METHOD) {
		std::_Exit(94);
	}
	variant = 99;
	raw = 99;
	ERR_PRINT_OFF;
	old_method->validated_call(nullptr, nullptr, &variant);
	old_method->ptrcall(nullptr, nullptr, &raw);
	ERR_PRINT_ON;
	if (variant.get_type() != Variant::INT || int64_t(variant) != 0 || raw != 0 || native_probes != 3) {
		std::_Exit(94);
	}
	if (double(new_method->call(nullptr, nullptr, 0, error)) != 73.5 || error.error != Callable::CallError::CALL_OK) {
		std::_Exit(94);
	}
	new_method->validated_call(nullptr, nullptr, &variant);
	double real = -1;
	new_method->ptrcall(nullptr, nullptr, &real);
	if (double(variant) != 73.5 || real != 73.5 || native_probes != 6) {
		std::_Exit(94);
	}
	unregister(extension.ptr(), &name);
	if (extension->close_library() != OK || loader->is_library_open()) {
		std::_Exit(95);
	}
	OS::get_singleton()->unset_environment("DEVICE_EXIT_METHOD_FLOAT");
	owner = nullptr;
	std::fprintf(stderr, "METHOD_NATIVE good=1 probes=6 early_closes=0 distinct_generation=1\n");
}
REGISTER_TEST_COMMAND("--device-exit-method-native", native_method_child);
static void native_binding_child() {
	String path;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--device-exit-path=")) {
			path = arg.trim_prefix("--device-exit-path=");
		}
	}
	if (path.is_empty()) {
		std::_Exit(91);
	}
	OS::get_singleton()->set_environment("DEVICE_EXIT_BINDING_NATIVE", "1");
	Ref<NativeMethodLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library(path, loader) != OK || !native_binding_callbacks.create_callback || !native_binding_callbacks.free_callback) {
		std::_Exit(92);
	}
	for (bool destroy : { false, true }) {
		Object *object = memnew(Object);
		if (object->get_instance_binding(extension.ptr(), &native_binding_callbacks) != object ||
				object->get_instance_binding(extension.ptr(), nullptr) != object ||
				extension->close_library() != ERR_BUSY || !loader->is_library_open()) {
			std::_Exit(94);
		}
		if (!destroy) {
			object->free_instance_binding(extension.ptr());
			if (object->has_instance_binding(extension.ptr())) {
				std::_Exit(94);
			}
		}
		memdelete(object);
	}
	if (native_probes != 4 || native_early_closes != 0 || extension->close_library() != OK || loader->is_library_open()) {
		std::_Exit(95);
	}
	// Host-owned callback metadata intentionally retains stale image addresses.
	Object *closed = memnew(Object);
	if (closed->get_instance_binding(extension.ptr(), &native_binding_callbacks) || closed->has_instance_binding(extension.ptr()) || native_probes != 4) {
		std::_Exit(94);
	}
	memdelete(closed);
	owner = nullptr;
	std::fprintf(stderr, "BINDING_NATIVE good=1 callbacks=4 early_closes=0\n");
}
REGISTER_TEST_COMMAND("--device-exit-binding-native", native_binding_child);
static void native_object_child() {
	String path;
	for (const String &arg : OS::get_singleton()->get_cmdline_args()) {
		if (arg.begins_with("--device-exit-path=")) {
			path = arg.trim_prefix("--device-exit-path=");
		}
	}
	if (path.is_empty()) {
		std::_Exit(91);
	}
	OS::get_singleton()->unset_environment("DEVICE_EXIT_METHOD_FLOAT");
	OS::get_singleton()->set_environment("DEVICE_EXIT_OBJECT_NATIVE", "1");
	Ref<NativeMethodLoader> loader;
	loader.instantiate();
	Ref<GDExtension> extension;
	extension.instantiate();
	if (extension->open_library(path, loader) != OK) {
		std::_Exit(92);
	}
	VirtualProbe *object = memnew(VirtualProbe);
	StringName name("DeviceExitNativeMethod"), property("probe");
	ClassDB::set_object_extension_instance(object, name, object);
	bool valid = false;
	if (int64_t(object->get(property, &valid)) != 73 || !valid) {
		std::_Exit(94);
	}
	List<PropertyInfo> list;
	object->get_property_list(&list);
	int matches = 0;
	for (const PropertyInfo &info : list) {
		if (info.name == property) {
			matches++;
		}
	}
	int64_t ret = 99;
	if (matches != 1 || !object->invoke(ret) || ret != 73 || !object->overridden() || !object->invoke(ret) || ret != 73 || native_probes != 6) {
		std::_Exit(94);
	}
	if (extension->close_library() != ERR_BUSY || !loader->is_library_open()) {
		std::_Exit(95);
	}
	// The real reload clear retires the direct holder and invalidates the
	// generated virtual cache before actual OS close. Never remove a holder
	// while keeping a live direct association merely to exercise stale code.
	object->clear_internal_extension();
	if (extension->close_library() != OK || loader->is_library_open()) {
		std::_Exit(95);
	}
	ret = 99;
	Variant closed = object->get(property, &valid);
	if (valid || closed.get_type() != Variant::NIL || object->invoke(ret) || ret != 99 || object->overridden()) {
		std::_Exit(94);
	}
	list.clear();
	object->get_property_list(&list);
	for (const PropertyInfo &info : list) {
		if (info.name == property) {
			std::_Exit(94);
		}
	}
	if (native_probes != 6) {
		std::_Exit(94);
	}
	memdelete(object);
	auto unregister = reinterpret_cast<GDExtensionInterfaceClassdbUnregisterExtensionClass>(native_get("classdb_unregister_extension_class"));
	unregister(extension.ptr(), &name);
	owner = nullptr;
	std::fprintf(stderr, "OBJECT_NATIVE good=1 probes=6 early_closes=0 holder_block=1 cleared=1\n");
}
REGISTER_TEST_COMMAND("--device-exit-object-native", native_object_child);
#endif
} // namespace TestDeviceExitMethod
