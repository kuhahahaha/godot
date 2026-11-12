/**************************************************************************/
/*  particles_external_output_test_fixture.h                              */
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

#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"

namespace RendererRD {
class ExternalOutputStorage;
}

// Fixed tests-only owned prefix, completed within one original RT callback.
// A retained raw pointer is intentional on failure; destruction never guesses
// driver return or dereferences a device during shutdown. This is no K03 lease.
class ParticlesExternalOutputTestFixture : public RefCounted {
	GDCLASS(ParticlesExternalOutputTestFixture, RefCounted)

	mutable Mutex result_mutex;
	RendererRD::ExternalOutputStorage *storage = nullptr;
	bool started = false;
	bool complete = false;
	bool physical_prefix_returned = false;
	bool retained_on_failure = false;
	Error outcome = ERR_UNCONFIGURED;
	Array observations;
	Array failures;
	Dictionary device_info;
	Dictionary counts;
	uint32_t maintenance_calls = 0;
	uint64_t elapsed_usec = 0;

	bool check(bool p_ok, const String &p_label);
	Error finish(Error p_error);

protected:
	static void _bind_methods();

public:
	Error run();
	Dictionary result() const;
};
