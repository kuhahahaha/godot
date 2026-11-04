/**************************************************************************/
/*  gdextension_production_resource_use.h                                 */
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

#include "core/error/error_list.h"
#include "core/extension/gdextension_qualification_physical.h"

class TestProductionResourceUseAccess;
namespace GDExtensionDeviceExit {
class ProductionRequest;

// Preallocated reverse edge within the original production request arena.
// Only the original Registry installs/removes it, under Registry + RD locks.
// This retains physical metadata; it neither admits work nor proves completion.
class ProductionResourceUse {
	friend class Registry;
	friend class ProductionRequest;
#ifdef TESTS_ENABLED
	friend class ::TestProductionResourceUseAccess;
#endif
	QualificationPhysicalReturn *physical = nullptr;
	ProductionRequest *request = nullptr;
	uint64_t request_serial = 0;
	uint64_t bridge = 0;
	uint64_t child = 0;
	uint64_t rid = 0;
	uint64_t serial = 0;
	ProductionResourceUse *previous = nullptr;
	ProductionResourceUse *next = nullptr;

	bool same_physical() const {
		return physical && physical->bridge == bridge && physical->child == child && physical->rid == rid && physical->serial == serial;
	}

	bool linked() const {
		return physical && (previous ? previous->physical == physical && previous->next == this : physical->production_uses == this) &&
				(!next || (next->physical == physical && next->previous == this));
	}

	Error attach(QualificationPhysicalReturn &p_physical, ProductionRequest &p_request, uint64_t p_request_serial) {
		if (!p_request_serial) {
			return ERR_INVALID_PARAMETER;
		}
		if (physical) {
			return physical == &p_physical && request == &p_request && request_serial == p_request_serial && same_physical() && linked() ? OK : ERR_INVALID_DATA;
		}
		if (!p_physical.bridge || !p_physical.child || !p_physical.rid || !p_physical.serial || p_physical.use_closed.load() || p_physical.enqueued || p_physical.reserved_event || p_physical.free_event) {
			return ERR_UNAVAILABLE;
		}
		physical = &p_physical;
		request = &p_request;
		request_serial = p_request_serial;
		bridge = p_physical.bridge;
		child = p_physical.child;
		rid = p_physical.rid;
		serial = p_physical.serial;
		next = p_physical.production_uses;
		if (next) {
			next->previous = this;
		}
		p_physical.production_uses = this;
		return OK;
	}

	// The request owner must establish all its real native/readback returns
	// before invoking this. No destructor or timeout unlinks a live edge.
	Error detach(ProductionRequest &p_request, uint64_t p_request_serial) {
		if (!physical || request != &p_request || request_serial != p_request_serial || !same_physical()) {
			return ERR_INVALID_DATA;
		}
		if (!linked()) {
			return ERR_INVALID_DATA;
		}
		if (previous) {
			previous->next = next;
		} else {
			physical->production_uses = next;
		}
		if (next) {
			next->previous = previous;
		}
		physical = nullptr;
		request = nullptr;
		request_serial = bridge = child = rid = serial = 0;
		previous = next = nullptr;
		return OK;
	}

public:
	ProductionResourceUse() = default;
	ProductionResourceUse(const ProductionResourceUse &) = delete;
	ProductionResourceUse &operator=(const ProductionResourceUse &) = delete;
};
} // namespace GDExtensionDeviceExit
