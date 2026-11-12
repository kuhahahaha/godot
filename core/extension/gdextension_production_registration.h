/**************************************************************************/
/*  gdextension_production_registration.h                                 */
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

#include "core/extension/gdextension_library_exit_guard.h"

namespace GDExtensionDeviceExit {

// Export identity only. Copying this POD does not retain or authenticate a
// producer, issue an observation role, or authorize an original operation.
struct ProducerRegistrationIdentity {
	char label[96] = {};
	uint64_t registry_instance = 0;
	uint64_t library_generation = 0;
};

// Retains original Registry metadata. It intentionally contains no DSO pointer
// to invoke, callback/deleter, operation key, code hold, or native permission.
// The original CodeHolder/accepted-frame gates separately protect executable
// code. Historical identity may remain readable after those gates close.
class ProducerRegistrationRef {
	friend class Registry;
	RecordRef reference;
	uint64_t registration_serial = 0;

public:
	ProducerRegistrationRef() = default;
	ProducerRegistrationRef(const ProducerRegistrationRef &) = delete;
	ProducerRegistrationRef &operator=(const ProducerRegistrationRef &) = delete;
	ProducerRegistrationRef(ProducerRegistrationRef &&) noexcept = default;
	ProducerRegistrationRef &operator=(ProducerRegistrationRef &&) noexcept = default;
	bool is_valid() const { return reference.is_valid(); }
	void reset() {
		reference.reset();
		registration_serial = 0;
	}
};

} // namespace GDExtensionDeviceExit
