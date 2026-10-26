/**************************************************************************/
/*  gdextension_manager.h                                                 */
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

#include "core/extension/gdextension.h"
#include "core/variant/native_ptr.h"

GDVIRTUAL_NATIVE_PTR(GDExtensionInitializationFunction)

class GDExtensionManager : public Object {
	GDCLASS(GDExtensionManager, Object);
#ifdef TESTS_ENABLED
	friend struct TestDeviceExitEntry::Access;
#endif

	int32_t level = -1;
	HashMap<String, Ref<GDExtension>> gdextension_map;
	HashMap<String, String> gdextension_class_icon_paths;

	bool startup_callback_called = false;
	bool shutdown_callback_called = false;
	bool guarded_levels_deinitialized_before_main_loop = false;
	bool scene_materialization_seam_reached = false;
	bool scene_materialization_dispatch_active = false;
	Error scene_materialization_error = OK;
	GDExtensionDeviceExit::CloseRoute exit_route = GDExtensionDeviceExit::CloseRoute::LEVEL_DEINIT;

	static void _bind_methods();

	static inline GDExtensionManager *singleton = nullptr;

public:
	enum LoadStatus {
		LOAD_STATUS_OK,
		LOAD_STATUS_FAILED,
		LOAD_STATUS_ALREADY_LOADED,
		LOAD_STATUS_NOT_LOADED,
		LOAD_STATUS_NEEDS_RESTART,
	};

private:
	LoadStatus _load_extension_internal(const Ref<GDExtension> &p_extension, bool p_first_load);
	LoadStatus _finish_guarded_scene_ready_or_abort(const Ref<GDExtension> &p_extension, bool p_reloading);
	void _finish_load_extension(const Ref<GDExtension> &p_extension);
	LoadStatus _unload_extension_internal(const Ref<GDExtension> &p_extension);

#ifdef TOOLS_ENABLED
	static void _reload_all_scripts();
#endif

public:
	LoadStatus load_extension(const String &p_path);
	LoadStatus load_extension_from_function(const String &p_path, GDExtensionConstPtr<const GDExtensionInitializationFunction> p_init_func);
	LoadStatus load_extension_with_loader(const String &p_path, const Ref<GDExtensionLoader> &p_loader);
	LoadStatus reload_extension(const String &p_path);
	LoadStatus unload_extension(const String &p_path);
	bool is_extension_loaded(const String &p_path) const;
	Vector<String> get_loaded_extensions() const;
	Ref<GDExtension> get_extension(const String &p_path);

	bool class_has_icon_path(const String &p_class) const;
	String class_get_icon_path(const String &p_class) const;

	Error initialize_extensions(GDExtension::InitializationLevel p_level);
	Error preflight_exit(GDExtensionDeviceExit::CloseRoute p_route);
	Error scene_materialized();
	bool scene_materialization_seam_has_been_reached() const { return scene_materialization_seam_reached; }
	Error cancel_pending_scene_ready_for_start_abort(GDExtensionDeviceExit::CloseRoute p_route);
	Error deinitialize_guarded_core_for_setup_abort();
	Error deinitialize_guarded_extensions_before_main_loop();
	Error deinitialize_extensions(GDExtension::InitializationLevel p_level);

#ifdef TOOLS_ENABLED
	void track_instance_binding(void *p_token, Object *p_object);
	void untrack_instance_binding(void *p_token, Object *p_object);
#endif

	static GDExtensionManager *get_singleton();

	void load_extensions();
	void reload_extensions();
	bool ensure_extensions_loaded(const HashSet<String> &p_extensions);

	void startup();
	Error shutdown();
	void frame();

	GDExtensionManager();
	~GDExtensionManager();
};

VARIANT_ENUM_CAST(GDExtensionManager::LoadStatus)
