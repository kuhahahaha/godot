/**************************************************************************/
/*  gdextension_library_loader.h                                          */
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

#include "core/extension/gdextension_loader.h"
#include "core/io/config_file.h"
#include "core/os/shared_object.h"

#include <functional>

#ifdef TESTS_ENABLED
namespace TestDeviceExitLibraryClose {
struct Access;
}
#endif

class GDExtensionLibraryLoader : public GDExtensionLoader {
#ifdef TESTS_ENABLED
	friend struct TestDeviceExitLibraryClose::Access;
	Error test_close_error = OK;
	uint32_t test_close_attempts = 0;
#endif
	GDSOFTCLASS(GDExtensionLibraryLoader, GDExtensionLoader);

	friend class GDExtensionManager;
	friend class GDExtension;

private:
	struct Configuration {
		String resource_path;
		String library_path;
		String entry_symbol;
		Vector<SharedObject> library_dependencies;
		HashMap<String, String> class_icon_paths;
		uint32_t guard_version = 0;
#ifdef TOOLS_ENABLED
		bool is_reloadable = false;
		uint64_t resource_last_modified_time = 0;
		uint64_t library_last_modified_time = 0;
#endif
	};
	Configuration configuration;
	Configuration prepared_configuration;
	void *library = nullptr;
	bool preparing = false;
	bool prepared = false;
	bool opening = false;
	bool closing = false;
	Error close_error = OK;

	Error _parse_gdextension_file(const String &p_path, Configuration &r_configuration);

public:
	static String find_extension_library(const String &p_path, Ref<ConfigFile> p_config, std::function<bool(String)> p_has_feature, PackedStringArray *r_tags = nullptr);
	static Vector<SharedObject> find_extension_dependencies(const String &p_path, Ref<ConfigFile> p_config, std::function<bool(String)> p_has_feature);

	virtual Error prepare_open(const String &p_path) override;
	virtual void abort_prepared_open() override;
	virtual uint32_t get_device_exit_guard_version() const override { return prepared ? prepared_configuration.guard_version : configuration.guard_version; }
	virtual LoaderKind get_loader_kind() const override { return LoaderKind::DYNAMIC; }
	virtual Error open_library(const String &p_path) override;
	virtual Error initialize(GDExtensionInterfaceGetProcAddress p_get_proc_address, const Ref<GDExtension> &p_extension, GDExtensionInitialization *r_initialization) override;
	virtual Error close_library() override;
	virtual bool is_library_open() const override;
	virtual bool has_library_changed() const override;
	virtual bool library_exists() const override;

	Error parse_gdextension_file(const String &p_path) { return prepare_open(p_path); }
};
