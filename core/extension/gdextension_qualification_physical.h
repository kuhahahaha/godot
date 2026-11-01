/**************************************************************************/
/*  gdextension_qualification_physical.h                                  */
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

#include "core/os/mutex.h"
#include "core/templates/local_vector.h"

#include <atomic>
#include <cstdint>

class RenderingDevice;
class TestQualificationPhysicalAccess;
class TestQualificationAckAccess;
namespace GDExtensionDeviceExit {
class Registry;
struct QualificationTicket;
struct QualificationResource;
// One exact object return cell within an original Registry child. This is not
// a completion source: only the original RD queue and driver-free return write
// it. Its parent retains it through the owner's later acknowledgement.
class QualificationPhysicalReturn {
 friend class Registry;
 friend struct QualificationTicket;
 struct QualificationResource;
 friend class ::RenderingDevice;
#ifdef TESTS_ENABLED
	friend class ::TestQualificationPhysicalAccess;
	friend class ::TestQualificationAckAccess;
#endif
	BinaryMutex mutex;
	uint64_t bridge = 0;
	uint64_t child = 0;
	uint64_t rid = 0;
	uint64_t serial = 0;
	uint64_t frame_epoch = 0;
	uint64_t reserved_event = 0;
	uint64_t free_event = 0;
	uint32_t frame_slot = UINT32_MAX;
	bool event_capacity_reserved = false;
	bool enqueued = false;
	std::atomic<bool> use_closed{ false };
	std::atomic<uint32_t> dependent_children{ 0 };
	LocalVector<QualificationTicket *> uses;
};
} //namespace GDExtensionDeviceExit
