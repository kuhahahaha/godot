/**************************************************************************/
/*  gdextension_manager.cpp                                               */
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

#include "gdextension_manager.h"

#include "core/config/engine.h"
#include "core/extension/gdextension_function_loader.h"
#include "core/extension/gdextension_library_loader.h"
#include "core/extension/gdextension_special_compat_hashes.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/object/callable_mp.h"
#include "core/object/class_db.h"
#include "core/object/script_language.h"
#include "core/os/os.h"

GDExtensionManager::LoadStatus GDExtensionManager::_load_extension_internal(const Ref<GDExtension> &p_extension, bool p_first_load) {
	if (level >= 0) { // Already initialized up to some level.
		const bool guarded = p_extension->loader->get_device_exit_guard_version() == 1;
		int32_t minimum_level = guarded ? p_extension->get_minimum_library_initialization_level() : 0;
		if (!p_first_load) {
			minimum_level = p_extension->get_minimum_library_initialization_level();
			if (minimum_level < MIN(level, GDExtension::INITIALIZATION_LEVEL_SCENE)) {
				return LOAD_STATUS_NEEDS_RESTART;
			}
		}
		if (guarded && p_first_load && minimum_level < MIN(level, GDExtension::INITIALIZATION_LEVEL_SCENE)) {
			return LOAD_STATUS_NEEDS_RESTART;
		}
		// Initialize up to current level.
		for (int32_t i = minimum_level; i <= level; i++) {
			const Error error = p_extension->initialize_library(GDExtension::InitializationLevel(i));
			if (guarded && error != OK && p_extension->scene_ready_load_error == error) {
				p_extension->abort_scene_ready_load(error);
				return LOAD_STATUS_FAILED;
			}
			GDExtensionDeviceExit::require_level_dispatch_success(error, i, true);
		}
	}

	for (const KeyValue<String, String> &kv : p_extension->class_icon_paths) {
		gdextension_class_icon_paths[kv.key] = kv.value;
	}

	return LOAD_STATUS_OK;
}

GDExtensionManager::LoadStatus GDExtensionManager::_finish_guarded_scene_ready_or_abort(const Ref<GDExtension> &extension, bool reloading) {
	(void)reloading; // The caller retains/removes its existing map entry.
	if (!scene_materialization_seam_reached || extension->loader->get_device_exit_guard_version() != 1) { return LOAD_STATUS_OK; }
	Error error = scene_materialization_error;
	if (error != OK) {
		auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
		if (registry.scene_ready_pending(extension->library_exit_ref)) {
			GDExtensionDeviceExit::require_close_success(registry.cancel_pending_scene_ready(extension->library_exit_ref, GDExtensionDeviceExit::CloseRoute::SHUTDOWN), GDExtensionDeviceExit::CloseRoute::SHUTDOWN);
		}
	} else {
		error = extension->dispatch_scene_ready_if_materialized();
	}
	if (error == OK) { return LOAD_STATUS_OK; }
	for (const KeyValue<String, String> &icon : extension->class_icon_paths) {
		const auto *current = gdextension_class_icon_paths.getptr(icon.key);
		if (current && *current == icon.value) { gdextension_class_icon_paths.erase(icon.key); }
	}
	extension->abort_scene_ready_load(error); // The local Ref stays live through the exact INIT_ABORT close.
	return LOAD_STATUS_FAILED;
}

void GDExtensionManager::_finish_load_extension(const Ref<GDExtension> &p_extension) {
#ifdef TOOLS_ENABLED
	// Signals that a new extension is loaded so GDScript can register new class names.
	emit_signal("extension_loaded", p_extension);
#endif

	if (startup_callback_called) {
		// Extension is loading after the startup callback has already been called,
		// so we call it now for this extension to make sure it doesn't miss it.
		p_extension->_call_main_loop_callback(GDExtension::MainLoopCallback::STARTUP);
	}
}

GDExtensionManager::LoadStatus GDExtensionManager::_unload_extension_internal(const Ref<GDExtension> &p_extension) {
#ifdef TOOLS_ENABLED
	// Signals that a new extension is unloading so GDScript can unregister class names.
	emit_signal("extension_unloading", p_extension);
#endif

	if (!shutdown_callback_called) {
		// Extension is unloading before the shutdown callback has been called,
		// which means the engine hasn't shutdown yet but we want to make sure
		// to call the shutdown callback so it doesn't miss it.
		p_extension->_call_main_loop_callback(GDExtension::MainLoopCallback::SHUTDOWN);
	}

	if (level >= 0) { // Already initialized up to some level.
		// Deinitialize down from current level.
		const int32_t minimum_level = p_extension->loader->get_device_exit_guard_version() == 1 ?
				p_extension->get_minimum_library_initialization_level() : GDExtension::INITIALIZATION_LEVEL_CORE;
		for (int32_t i = level; i >= minimum_level; i--) {
			GDExtensionDeviceExit::require_level_dispatch_success(p_extension->deinitialize_library(GDExtension::InitializationLevel(i)), i, false);
		}
	}

	for (const KeyValue<String, String> &kv : p_extension->class_icon_paths) {
		gdextension_class_icon_paths.erase(kv.key);
	}

	// Clear main loop callbacks.
	p_extension->startup_callback = nullptr;
	p_extension->shutdown_callback = nullptr;
	p_extension->frame_callback = nullptr;
	p_extension->main_loop_callback_mask = 0;

	return LOAD_STATUS_OK;
}

GDExtensionManager::LoadStatus GDExtensionManager::load_extension(const String &p_path) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return LOAD_STATUS_FAILED;
	}

	Ref<GDExtensionLibraryLoader> loader;
	loader.instantiate();
	return load_extension_with_loader(p_path, loader);
}

GDExtensionManager::LoadStatus GDExtensionManager::load_extension_from_function(const String &p_path, GDExtensionConstPtr<const GDExtensionInitializationFunction> p_init_func) {
	Ref<GDExtensionFunctionLoader> func_loader;
	func_loader.instantiate();
	func_loader->set_initialization_function((GDExtensionInitializationFunction)*p_init_func.data);
	return load_extension_with_loader(p_path, func_loader);
}

GDExtensionManager::LoadStatus GDExtensionManager::load_extension_with_loader(const String &p_path, const Ref<GDExtensionLoader> &p_loader) {
	DEV_ASSERT(p_loader.is_valid());

	if (gdextension_map.has(p_path)) {
		return LOAD_STATUS_ALREADY_LOADED;
	}

	Ref<GDExtension> extension;
	extension.instantiate();
	Error err = extension->open_library(p_path, p_loader);
	if (err != OK) {
		return LOAD_STATUS_FAILED;
	}

	LoadStatus status = _load_extension_internal(extension, true);
	if (status != LOAD_STATUS_OK) {
		return status;
	}
	status = _finish_guarded_scene_ready_or_abort(extension, false);
	if (status != LOAD_STATUS_OK) { return status; }

	_finish_load_extension(extension);

	extension->set_path(p_path);
	gdextension_map[p_path] = extension;
	return LOAD_STATUS_OK;
}

GDExtensionManager::LoadStatus GDExtensionManager::reload_extension(const String &p_path) {
#ifndef TOOLS_ENABLED
	ERR_FAIL_V_MSG(LOAD_STATUS_FAILED, "GDExtensions can only be reloaded in an editor build.");
#else
	ERR_FAIL_COND_V_MSG(!Engine::get_singleton()->is_extension_reloading_enabled(), LOAD_STATUS_FAILED, "GDExtension reloading is disabled.");

	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return LOAD_STATUS_FAILED;
	}

	if (!gdextension_map.has(p_path)) {
		return LOAD_STATUS_NOT_LOADED;
	}

	Ref<GDExtension> extension = gdextension_map[p_path];
	ERR_FAIL_COND_V_MSG(!extension->is_reloadable(), LOAD_STATUS_FAILED, vformat("This GDExtension is not marked as 'reloadable' or doesn't support reloading: %s.", p_path));

	LoadStatus status;

	if (extension->is_library_open() && extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::RELOAD) != OK) {
		return LOAD_STATUS_FAILED;
	}
	extension->prepare_reload();

	// Unload library if it's open. It may not be open if the developer made a
	// change that broke loading in a previous hot-reload attempt.
	if (extension->is_library_open()) {
		status = _unload_extension_internal(extension);
		if (status != LOAD_STATUS_OK) {
			// We need to clear these no matter what.
			extension->clear_instance_bindings();
			return status;
		}

		extension->clear_instance_bindings();
		GDExtensionDeviceExit::require_close_success(extension->_close_library(GDExtensionDeviceExit::CloseRoute::RELOAD), GDExtensionDeviceExit::CloseRoute::RELOAD);
	}

	Error err = extension->open_library(p_path, extension->loader);
	if (err != OK) {
		return LOAD_STATUS_FAILED;
	}

	status = _load_extension_internal(extension, false);
	if (status != LOAD_STATUS_OK) {
		return status;
	}
	status = _finish_guarded_scene_ready_or_abort(extension, true);
	if (status != LOAD_STATUS_OK) { return status; }

	extension->finish_reload();

	// Needs to come after reload is fully finished, so all objects using
	// extension classes are in a consistent state.
	_finish_load_extension(extension);

	return LOAD_STATUS_OK;
#endif
}

GDExtensionManager::LoadStatus GDExtensionManager::unload_extension(const String &p_path) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return LOAD_STATUS_FAILED;
	}

	if (!gdextension_map.has(p_path)) {
		return LOAD_STATUS_NOT_LOADED;
	}

	Ref<GDExtension> extension = gdextension_map[p_path];
	if (!shutdown_callback_called && !extension->is_shutdown_callback_dispatched()) {
		extension->_call_main_loop_callback(GDExtension::MainLoopCallback::SHUTDOWN);
	}
	if (extension->preflight_exit(GDExtensionDeviceExit::CloseRoute::LAST_REF) != OK) {
		return LOAD_STATUS_FAILED;
	}
	LoadStatus status = _unload_extension_internal(extension);
	if (status != LOAD_STATUS_OK) {
		return status;
	}

#ifdef TOOLS_ENABLED
	extension->clear_instance_bindings();
#endif
	GDExtensionDeviceExit::require_close_success(extension->_close_library(GDExtensionDeviceExit::CloseRoute::LAST_REF), GDExtensionDeviceExit::CloseRoute::LAST_REF);

	gdextension_map.erase(p_path);
	return LOAD_STATUS_OK;
}

bool GDExtensionManager::is_extension_loaded(const String &p_path) const {
	return gdextension_map.has(p_path);
}

Vector<String> GDExtensionManager::get_loaded_extensions() const {
	Vector<String> ret;
	for (const KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		ret.push_back(E.key);
	}
	return ret;
}
Ref<GDExtension> GDExtensionManager::get_extension(const String &p_path) {
	HashMap<String, Ref<GDExtension>>::Iterator E = gdextension_map.find(p_path);
	ERR_FAIL_COND_V(!E, Ref<GDExtension>());
	return E->value;
}

bool GDExtensionManager::class_has_icon_path(const String &p_class) const {
	// TODO: Check that the icon belongs to a registered class somehow.
	return gdextension_class_icon_paths.has(p_class);
}

String GDExtensionManager::class_get_icon_path(const String &p_class) const {
	// TODO: Check that the icon belongs to a registered class somehow.
	if (gdextension_class_icon_paths.has(p_class)) {
		return gdextension_class_icon_paths[p_class];
	}
	return "";
}

Error GDExtensionManager::initialize_extensions(GDExtension::InitializationLevel p_level) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return OK;
	}

	ERR_FAIL_COND_V(int32_t(p_level) - 1 != level, ERR_INVALID_PARAMETER);
	level = p_level; // Partial startup unwinds each extension's actual initialized level.
	for (KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		if (E.value->loader->get_device_exit_guard_version() == 1 && p_level < E.value->get_minimum_library_initialization_level()) { continue; }
		const Error error = E.value->initialize_library(p_level);
		if (error != OK && E.value->loader->get_device_exit_guard_version() == 1 && E.value->scene_ready_load_error == error) {
			E.value->abort_scene_ready_load(error);
			return error;
		}
		GDExtensionDeviceExit::require_level_dispatch_success(error, p_level, true);

		if (p_level == GDExtension::INITIALIZATION_LEVEL_EDITOR) {
			for (const KeyValue<String, String> &kv : E.value->class_icon_paths) {
				gdextension_class_icon_paths[kv.key] = kv.value;
			}
		}
	}
	return OK;
}

Error GDExtensionManager::scene_materialized() {
	if (!Thread::is_main_thread()) { return ERR_UNAUTHORIZED; }
	if (scene_materialization_dispatch_active) { return ERR_BUSY; }
	// Record actual Main order before prechecks or any extension entry.
	scene_materialization_seam_reached = true;
	if (scene_materialization_error != OK) { return scene_materialization_error; }
	scene_materialization_dispatch_active = true;
	struct DispatchReturn {
		bool &active;
		~DispatchReturn() { active = false; }
	} dispatch_return{ scene_materialization_dispatch_active };
	struct PendingGeneration {
		GDExtension *extension;
		uint64_t generation;
	};
	Vector<PendingGeneration> pending;
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	for (const KeyValue<String, Ref<GDExtension>> &entry : gdextension_map) {
		if (registry.scene_ready_pending(entry.value->library_exit_ref)) {
			// Borrow the original map-owned wrapper/RecordRef, without a RefCounted
			// reference callback. Pending/InFrame G1 Busy prevents its removal or
			// generation replacement until this item is consumed. Done items are
			// never dereferenced again, including during snapshot destruction.
			pending.push_back({ entry.value.ptr(), entry.value->library_exit_ref.get_generation() });
		}
	}
	for (const auto &item : pending) {
		if (item.extension->library_exit_ref.get_generation() != item.generation) {
			GDExtensionDeviceExit::require_close_success(ERR_INVALID_DATA, GDExtensionDeviceExit::CloseRoute::SHUTDOWN);
		}
		const Error error = item.extension->dispatch_scene_ready_if_materialized();
		if (error == OK) { continue; }
		scene_materialization_error = error;
		// Include generations added by an earlier callback; never hold a map
		// iterator across DSO entry. Cancellation enters no extension code.
		for (const KeyValue<String, Ref<GDExtension>> &entry : gdextension_map) {
			if (!registry.scene_ready_pending(entry.value->library_exit_ref)) { continue; }
			GDExtensionDeviceExit::require_close_success(
					registry.cancel_pending_scene_ready(entry.value->library_exit_ref, GDExtensionDeviceExit::CloseRoute::SHUTDOWN),
					GDExtensionDeviceExit::CloseRoute::SHUTDOWN);
		}
		break;
	}
	return scene_materialization_error;
}

Error GDExtensionManager::cancel_pending_scene_ready_for_start_abort(GDExtensionDeviceExit::CloseRoute route) {
	if (!Thread::is_main_thread()) { return ERR_UNAUTHORIZED; }
	if (route != GDExtensionDeviceExit::CloseRoute::SETUP_ABORT && route != GDExtensionDeviceExit::CloseRoute::SHUTDOWN) { return ERR_INVALID_DATA; }
	if (scene_materialization_seam_reached || scene_materialization_dispatch_active) { return ERR_UNAUTHORIZED; }
	auto &registry = GDExtensionDeviceExit::Registry::get_singleton();
	for (const KeyValue<String, Ref<GDExtension>> &entry : gdextension_map) {
		if (!registry.scene_ready_pending(entry.value->library_exit_ref)) { continue; }
		const Error error = registry.cancel_pending_scene_ready(entry.value->library_exit_ref, route);
		if (error != OK) { return error; }
	}
	return OK;
}

Error GDExtensionManager::preflight_exit(GDExtensionDeviceExit::CloseRoute p_route) {
	for (const KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		if (E.value->loader->get_device_exit_guard_version() != 1 || !E.value->is_library_open()) { continue; }
		const Error result = E.value->preflight_exit(p_route);
		if (result != OK) { return result; }
	}
	exit_route = p_route;
	return OK;
}

Error GDExtensionManager::deinitialize_guarded_extensions_before_main_loop() {
	const Error preflight = preflight_exit(exit_route);
	if (preflight != OK) { return preflight; }
	if (guarded_levels_deinitialized_before_main_loop) { return OK; }
	// A guarded SCENE binding may belong to the main loop itself. Retire its
	// exact BINDING action while the SceneTree is still alive; legacy libraries
	// retain the established module and ScriptServer teardown order.
	for (int32_t current_level = GDExtension::INITIALIZATION_LEVEL_EDITOR; current_level >= GDExtension::INITIALIZATION_LEVEL_SCENE; current_level--) {
		for (KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
			if (E.value->loader->get_device_exit_guard_version() != 1 || !E.value->is_library_open() || E.value->level_initialized != current_level ||
					current_level < E.value->get_minimum_library_initialization_level()) { continue; }
			GDExtensionDeviceExit::require_level_dispatch_success(
					E.value->deinitialize_library(GDExtension::InitializationLevel(current_level)), current_level, false);
		}
	}
	guarded_levels_deinitialized_before_main_loop = true;
	return OK;
}

Error GDExtensionManager::deinitialize_guarded_core_for_setup_abort() {
	const Error error = preflight_exit(GDExtensionDeviceExit::CloseRoute::SETUP_ABORT);
	if (error != OK) { return error; }
	// Main must first unwind native services interleaved with their owning
	// extension levels. In particular Navigation precedes SERVERS teardown.
	ERR_FAIL_COND_V(level > GDExtension::INITIALIZATION_LEVEL_CORE, ERR_INVALID_DATA);
	// Guarded CORE terminals also run while globals/core services remain live;
	// legacy CORE retains its established unregister_core_extensions boundary.
	for (const KeyValue<String, Ref<GDExtension>> &entry : gdextension_map) {
		if (entry.value->loader->get_device_exit_guard_version() != 1 || !entry.value->is_library_open() || entry.value->level_initialized != GDExtension::INITIALIZATION_LEVEL_CORE) { continue; }
		GDExtensionDeviceExit::require_level_dispatch_success(entry.value->deinitialize_library(GDExtension::INITIALIZATION_LEVEL_CORE), GDExtension::INITIALIZATION_LEVEL_CORE, false);
	}
	return OK;
}

Error GDExtensionManager::deinitialize_extensions(GDExtension::InitializationLevel p_level) {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return OK;
	}

	ERR_FAIL_COND_V(int32_t(p_level) != level, ERR_INVALID_PARAMETER);
	const Error preflight = preflight_exit(exit_route);
	if (preflight != OK) { return preflight; }
	for (KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		if (!E.value->is_library_open() || E.value->level_initialized < p_level) { continue; }
		if (E.value->loader->get_device_exit_guard_version() == 1 && p_level < E.value->get_minimum_library_initialization_level()) { continue; }
		if (guarded_levels_deinitialized_before_main_loop &&
				E.value->loader->get_device_exit_guard_version() == 1 && p_level >= GDExtension::INITIALIZATION_LEVEL_SCENE) { continue; }
		GDExtensionDeviceExit::require_level_dispatch_success(E.value->deinitialize_library(p_level), p_level, false);
	}
	level = int32_t(p_level) - 1;
	return OK;
}

#ifdef TOOLS_ENABLED
void GDExtensionManager::track_instance_binding(void *p_token, Object *p_object) {
	for (KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		if (E.value.ptr() == p_token) {
			if (E.value->is_reloadable()) {
				E.value->track_instance_binding(p_object);
				return;
			}
		}
	}
}

void GDExtensionManager::untrack_instance_binding(void *p_token, Object *p_object) {
	for (KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		if (E.value.ptr() == p_token) {
			if (E.value->is_reloadable()) {
				E.value->untrack_instance_binding(p_object);
				return;
			}
		}
	}
}

void GDExtensionManager::_reload_all_scripts() {
	for (int i = 0; i < ScriptServer::get_language_count(); i++) {
		ScriptServer::get_language(i)->reload_all_scripts();
	}
}
#endif // TOOLS_ENABLED

void GDExtensionManager::load_extensions() {
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return;
	}

	Ref<FileAccess> f = FileAccess::open(GDExtension::get_extension_list_config_file(), FileAccess::READ);
	while (f.is_valid() && !f->eof_reached()) {
		String s = f->get_line().strip_edges();
		if (!s.is_empty()) {
			LoadStatus err = load_extension(s);
			ERR_CONTINUE_MSG(err == LOAD_STATUS_FAILED, vformat("Error loading extension: '%s'.", s));
		}
	}

	OS::get_singleton()->load_platform_gdextensions();
}

void GDExtensionManager::reload_extensions() {
#ifdef TOOLS_ENABLED
	if (Engine::get_singleton()->is_recovery_mode_hint()) {
		return;
	}
	bool reloaded = false;
	for (const KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		if (!E.value->is_reloadable()) {
			continue;
		}

		if (E.value->has_library_changed()) {
			reloaded = true;
			reload_extension(E.value->get_path());
		}
	}

	if (reloaded) {
		emit_signal("extensions_reloaded");

		// Reload all scripts to clear out old references.
		callable_mp_static(&GDExtensionManager::_reload_all_scripts).call_deferred();
	}
#endif
}

bool GDExtensionManager::ensure_extensions_loaded(const HashSet<String> &p_extensions) {
	Vector<String> extensions_added;
	Vector<String> extensions_removed;

	for (const String &E : p_extensions) {
		if (!is_extension_loaded(E)) {
			extensions_added.push_back(E);
		}
	}

	Vector<String> loaded_extensions = get_loaded_extensions();
	for (const String &loaded_extension : loaded_extensions) {
		if (!p_extensions.has(loaded_extension)) {
			// The extension may not have a .gdextension file.
			const Ref<GDExtension> extension = GDExtensionManager::get_singleton()->get_extension(loaded_extension);
			if (!extension->get_loader()->library_exists()) {
				extensions_removed.push_back(loaded_extension);
			}
		}
	}

	String extension_list_config_file = GDExtension::get_extension_list_config_file();
	if (p_extensions.size()) {
		if (extensions_added.size() || extensions_removed.size()) {
			// Extensions were added or removed.
			Ref<FileAccess> f = FileAccess::open(extension_list_config_file, FileAccess::WRITE);
			for (const String &E : p_extensions) {
				f->store_line(E);
			}
		}
	} else {
		if (loaded_extensions.size() || FileAccess::exists(extension_list_config_file)) {
			// Extensions were removed.
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_RESOURCES);
			da->remove(extension_list_config_file);
		}
	}

	bool needs_restart = false;
	for (const String &extension : extensions_added) {
		GDExtensionManager::LoadStatus st = GDExtensionManager::get_singleton()->load_extension(extension);
		if (st == GDExtensionManager::LOAD_STATUS_NEEDS_RESTART) {
			needs_restart = true;
		}
	}

	for (const String &extension : extensions_removed) {
		GDExtensionManager::LoadStatus st = GDExtensionManager::get_singleton()->unload_extension(extension);
		if (st == GDExtensionManager::LOAD_STATUS_NEEDS_RESTART) {
			needs_restart = true;
		}
	}

#ifdef TOOLS_ENABLED
	if (extensions_added.size() || extensions_removed.size()) {
		// Emitting extensions_reloaded so EditorNode can reload Inspector and regenerate documentation.
		emit_signal("extensions_reloaded");

		// Reload all scripts to clear out old references.
		callable_mp_static(&GDExtensionManager::_reload_all_scripts).call_deferred();
	}
#endif

	return needs_restart;
}

void GDExtensionManager::startup() {
	startup_callback_called = true;

	for (const KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		const Ref<GDExtension> &extension = E.value;
		extension->_call_main_loop_callback(GDExtension::MainLoopCallback::STARTUP);
	}
}

Error GDExtensionManager::shutdown() {
	const Error preflight = preflight_exit(GDExtensionDeviceExit::CloseRoute::SHUTDOWN);
	if (preflight != OK) { return preflight; }
	shutdown_callback_called = true;

	for (const KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		const Ref<GDExtension> &extension = E.value;
		extension->_call_main_loop_callback(GDExtension::MainLoopCallback::SHUTDOWN);
	}
	return OK;
}

void GDExtensionManager::frame() {
	for (const KeyValue<String, Ref<GDExtension>> &E : gdextension_map) {
		const Ref<GDExtension> &extension = E.value;
		extension->_call_main_loop_callback(GDExtension::MainLoopCallback::FRAME);
	}
}

GDExtensionManager *GDExtensionManager::get_singleton() {
	return singleton;
}

void GDExtensionManager::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_extension", "path"), &GDExtensionManager::load_extension);
	ClassDB::bind_method(D_METHOD("load_extension_from_function", "path", "init_func"), &GDExtensionManager::load_extension_from_function);
	ClassDB::bind_method(D_METHOD("reload_extension", "path"), &GDExtensionManager::reload_extension);
	ClassDB::bind_method(D_METHOD("unload_extension", "path"), &GDExtensionManager::unload_extension);
	ClassDB::bind_method(D_METHOD("is_extension_loaded", "path"), &GDExtensionManager::is_extension_loaded);

	ClassDB::bind_method(D_METHOD("get_loaded_extensions"), &GDExtensionManager::get_loaded_extensions);
	ClassDB::bind_method(D_METHOD("get_extension", "path"), &GDExtensionManager::get_extension);

	BIND_ENUM_CONSTANT(LOAD_STATUS_OK);
	BIND_ENUM_CONSTANT(LOAD_STATUS_FAILED);
	BIND_ENUM_CONSTANT(LOAD_STATUS_ALREADY_LOADED);
	BIND_ENUM_CONSTANT(LOAD_STATUS_NOT_LOADED);
	BIND_ENUM_CONSTANT(LOAD_STATUS_NEEDS_RESTART);

	ADD_SIGNAL(MethodInfo("extensions_reloaded"));
	ADD_SIGNAL(MethodInfo("extension_loaded", PropertyInfo(Variant::OBJECT, "extension", PROPERTY_HINT_RESOURCE_TYPE, GDExtension::get_class_static())));
	ADD_SIGNAL(MethodInfo("extension_unloading", PropertyInfo(Variant::OBJECT, "extension", PROPERTY_HINT_RESOURCE_TYPE, GDExtension::get_class_static())));
}

GDExtensionManager::GDExtensionManager() {
	ERR_FAIL_COND(singleton != nullptr);
	singleton = this;

#ifndef DISABLE_DEPRECATED
	GDExtensionSpecialCompatHashes::initialize();
#endif
}

GDExtensionManager::~GDExtensionManager() {
	if (singleton == this) {
		singleton = nullptr;
	}
#ifndef DISABLE_DEPRECATED
	GDExtensionSpecialCompatHashes::finalize();
#endif
}
