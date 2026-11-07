/**************************************************************************/
/*  native_depth_allocation_projection.h                                  */
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

#include "servers/rendering/native_disposal_witness.h"

// Original RD-created physical allocation snapshot. Retains original disposal
// witnesses, not the RD, resources or GPU last-use. The native owner must keep
// its original RD/render thread alive through observe/retirement. No raw RD or
// driver pointer survives here; a different device cannot read this snapshot.
class NativeDepthAllocationProjection {
	friend class RenderingDevice;

public:
	enum class ByteKnowledge : uint8_t { UnknownNotExposed,
		Known };
	struct Bytes {
		ByteKnowledge knowledge = ByteKnowledge::UnknownNotExposed;
		// Read only when Known; default zero never asserts a zero native cost.
		uint64_t bytes = 0;
	};
	struct Report {
		NativeDisposalWitness::Report depth;
		NativeDisposalWitness::Report framebuffer;
		bool framebuffer_created = false;
		// This edge shares depth_driver_backing; it is not another allocation.
		NativeDisposalWitness::Report shared_attachment_depth;
		Bytes depth_driver_backing;
		Bytes framebuffer_driver_backing;
		Bytes framebuffer_object;
		Bytes attachment_reference;
		Bytes opaque_driver_overhead;
		// Physical disposal only: no producer/reader Complete or refund grant.
		bool all_created_children_returned = false;
	};

private:
	uint64_t device = 0;
	NativeDisposalWitness depth;
	NativeDisposalWitness framebuffer;
	Bytes depth_driver_backing;
};

// A native owner reservation quote, not a total-heap/opaque-overhead bound.
// Immutable copies retain provenance only. C holds the original E05 reservation
// through actual verification, child retirement and its sole closed_record.
class NativeDepthAllocationQuote {
	friend class RenderingDevice;

public:
	enum class CreateOutcome : uint8_t { NotCreated,
		CreatedVerified,
		CreatedOverReservation,
		CreatedActualUnknown };
	enum class Policy : uint8_t { Unavailable,
		MetalHeapPool,
		MetalHeapDedicated };
	struct Report {
		uint32_t width = 0, height = 0;
		Policy policy = Policy::Unavailable;
		NativeDepthAllocationProjection::Bytes texture_native_requirement;
		NativeDepthAllocationProjection::Bytes texture_owner_reservation;
		// Explicit admission policy: the framebuffer reference has no separate
		// Metal heap reservation credit. Known zero here is not physical FB cost;
		// framebuffer backing/object and opaque actuals remain unexposed below.
		NativeDepthAllocationProjection::Bytes framebuffer_owner_reservation;
		NativeDepthAllocationProjection::Bytes framebuffer_driver_backing;
		NativeDepthAllocationProjection::Bytes texture_object;
		NativeDepthAllocationProjection::Bytes framebuffer_object;
		NativeDepthAllocationProjection::Bytes attachment_reference;
		NativeDepthAllocationProjection::Bytes opaque_driver_overhead;
		uint64_t alignment = 0;
		// Native resource children, not newly allocated shared heap blocks.
		uint32_t allocations = 0;
		uint32_t parts = 0; // One E05 composite part holds depth and framebuffer.
		uint32_t shared_attachment_references = 0;
	};

private:
	uint64_t device = 0;
	Report report;
};
