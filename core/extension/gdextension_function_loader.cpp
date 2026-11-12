/**************************************************************************/
/*  gdextension_function_loader.cpp                                       */
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

#include "gdextension_function_loader.h"

#include "core/extension/gdextension.h"

Error GDExtensionFunctionLoader::open_library(const String &p_path) {
	if (is_library_open()) {
		return ERR_BUSY;
	}
	if (!prepared_library_path.is_empty() && prepared_library_path != p_path) {
		abort_prepared_open();
		return ERR_INVALID_PARAMETER;
	}
	if (prepared_library_path.is_empty()) {
		Error err = prepare_open(p_path);
		if (err != OK) {
			return err;
		}
	}
	if (prepared_guard_version > 1) {
		abort_prepared_open();
		return ERR_INVALID_PARAMETER;
	}

	library_path = prepared_library_path;
	initialization_function = prepared_initialization_function;
	abort_prepared_open();

	return OK;
}

Error GDExtensionFunctionLoader::prepare_open(const String &p_path) {
	if (is_library_open() || !prepared_library_path.is_empty()) {
		return ERR_BUSY;
	}
	if (!p_path.begins_with("libgodot://")) {
		return ERR_FILE_NOT_FOUND;
	}
	if (!initialization_function) {
		return ERR_DOES_NOT_EXIST;
	}
	if (device_exit_guard_version > 1) {
		return ERR_INVALID_PARAMETER;
	}
	prepared_library_path = p_path;
	prepared_initialization_function = initialization_function;
	prepared_guard_version = device_exit_guard_version;
	return OK;
}

void GDExtensionFunctionLoader::abort_prepared_open() {
	prepared_library_path.clear();
	prepared_initialization_function = nullptr;
	prepared_guard_version = 0;
}

Error GDExtensionFunctionLoader::initialize(GDExtensionInterfaceGetProcAddress p_get_proc_address, const Ref<GDExtension> &p_extension, GDExtensionInitialization *r_initialization) {
	ERR_FAIL_COND_V_MSG(!initialization_function, ERR_DOES_NOT_EXIST, "Initialization function is required for function based GDExtensions.");
	GDExtensionBool ret = initialization_function(p_get_proc_address, p_extension.ptr(), r_initialization);

	if (ret) {
		return OK;
	} else {
		ERR_FAIL_V_MSG(FAILED, "GDExtension initialization function for '" + library_path + "' returned an error.");
	}
}

Error GDExtensionFunctionLoader::close_library() {
	initialization_function = nullptr;
	library_path.clear();
	abort_prepared_open();
	return OK;
}

bool GDExtensionFunctionLoader::is_library_open() const {
	return !library_path.is_empty();
}

bool GDExtensionFunctionLoader::has_library_changed() const {
	return false;
}

bool GDExtensionFunctionLoader::library_exists() const {
	return true;
}

void GDExtensionFunctionLoader::set_initialization_function(GDExtensionInitializationFunction p_initialization_function) {
	initialization_function = p_initialization_function;
}
