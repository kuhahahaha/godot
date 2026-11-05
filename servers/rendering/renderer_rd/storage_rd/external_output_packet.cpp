/**************************************************************************/
/*  external_output_packet.cpp                                            */
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

#include "external_output_packet.h"

#include "servers/rendering/renderer_rd/storage_rd/external_output_owner.h"

namespace RendererRD {
ExternalSurfaceSelection::ExternalSurfaceSelection(ExternalSurfaceSelection &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->surface_move(*this, p_other);
	}
}
ExternalSurfaceSelection &ExternalSurfaceSelection::operator=(ExternalSurfaceSelection &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->surface_move(*this, p_other);
		}
	}
	return *this;
}
ExternalSurfaceSelection::~ExternalSurfaceSelection() {
	CRASH_COND(owner);
}

ExternalPacketViewLease::ExternalPacketViewLease(ExternalPacketViewLease &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->view_move(*this, p_other);
	}
}
ExternalPacketViewLease &ExternalPacketViewLease::operator=(ExternalPacketViewLease &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->view_move(*this, p_other);
		}
	}
	return *this;
}
ExternalPacketViewLease::~ExternalPacketViewLease() {
	CRASH_COND(owner);
}
Error ExternalPacketViewLease::release_checked() {
	return owner ? owner->view_return(*this) : ERR_DOES_NOT_EXIST;
}
ExternalPacketBinding::ExternalPacketBinding(ExternalPacketBinding &&p_other) noexcept {
	if (p_other.owner) {
		p_other.owner->read_move(*this, p_other);
	}
}
ExternalPacketBinding &ExternalPacketBinding::operator=(ExternalPacketBinding &&p_other) noexcept {
	if (this != &p_other) {
		CRASH_COND(owner);
		if (p_other.owner) {
			p_other.owner->read_move(*this, p_other);
		}
	}
	return *this;
}
ExternalPacketBinding::~ExternalPacketBinding() {
	CRASH_COND(owner);
}
Error ExternalPacketBinding::cancel_before_draw() {
	return owner ? owner->read_return(*this, false) : ERR_DOES_NOT_EXIST;
}
Error ExternalPacketBinding::record_checked(RenderingDevice::DrawListID) {
	// The original typed read acquisition and checked native draw join are
	// absent. Refuse before touching RD; caller still explicitly returns borrow.
	return ERR_UNAVAILABLE;
}
} // namespace RendererRD
