/**************************************************************************/
/*  gdextension_qualification_typed.cpp                                   */
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

#include "core/crypto/crypto_core.h"
#include "core/extension/gdextension.h"
#include "core/extension/gdextension_qualification_depth.h"
#include "core/extension/gdextension_qualification_internal.h"

#include <type_traits>

namespace GDExtensionDeviceExit {
namespace {
GDExtensionDeviceSessionResultV1 result(Error e, uint32_t reason = 0) {
	return { int32_t(e), reason, 0, 0 };
}
template <class T>
bool aligned(const T *p) {
	return p && uintptr_t(p) % alignof(T) == 0;
}
bool separate(const void *a, size_t as, const void *b, size_t bs) {
	const auto x = uintptr_t(a), y = uintptr_t(b);
	return x <= y ? as <= y - x : bs <= x - y;
}
bool abi(const GDExtensionDeviceExitAbi &a) {
	return a.abi_version == 1 && a.struct_size == sizeof(a) && !a.reserved;
}
bool source(const GDExtensionQualificationSourceV1 &s) {
	return s.session_serial && s.generation;
}
bool same_source(const GDExtensionQualificationSourceV1 &a, const GDExtensionQualificationSourceV1 &b) {
	return a.session_serial == b.session_serial && a.generation == b.generation && memcmp(a.digest, b.digest, sizeof(a.digest)) == 0;
}
struct RdUnlock {
	Mutex *mutex = nullptr;
	~RdUnlock() {
		if (mutex) {
			mutex->unlock();
		}
	}
};

} //namespace
Error Registry::qualification_resource_locked(QualificationBridge *b, GDExtensionQualificationResourceRefV1 ref, uint32_t kind, QualificationResource *&out) {
	out = nullptr;
	if (ref.registry_instance != instance || !ref.handle_serial) {
		return ERR_INVALID_DATA;
	}
	auto match = [&](auto *child) {if(child && child->serial==ref.handle_serial && !child->acknowledged && (!kind||child->kind==kind)){out=child;return true;}return false; };
	for (auto *child : b->textures) {
		if (match(child)) {
			return OK;
		}
	}
	for (auto *child : b->samplers) {
		if (match(child)) {
			return OK;
		}
	}
	for (auto *child : b->sets) {
		if (match(child)) {
			return OK;
		}
	}
	return ERR_DOES_NOT_EXIST;
}
Error Registry::qualification_resource_reserve_locked(QualificationBridge *b, RenderingDevice *rd, QualificationResource *c, uint32_t kind, const GDExtensionQualificationSourceV1 &s) {
	const uint32_t refs = b->cleanup_refs_reserved ? 1 : 3;
	if (private_limit < refs || records[b->library].private_refs > private_limit - refs || !qualification_serial_capacity(4) || !rd->_qualification_free_event_available(false) || !rd->next_qualification_object_serial || rd->next_qualification_object_serial == UINT64_MAX) {
		return ERR_PARAMETER_RANGE_ERROR;
	}
	qualification_serials.take(c->serial);
	qualification_serials.take(c->retire_serial);
	qualification_serials.take(c->dependency_serial);
	++qualification_cleanup_serial_reservations;
	c->cleanup_serial_reserved = true;
	c->kind = kind;
	c->source = s;
	c->object_serial = rd->next_qualification_object_serial++;
	c->physical.bridge = b->serial;
	c->physical.child = c->serial;
	c->physical.serial = c->object_serial;
	if (!b->cleanup_refs_reserved) {
		records[b->library].private_refs += 2;
		b->cleanup_refs_reserved = true;
	}
	++b->children;
	++records[b->library].private_refs;
	return OK;
}
GDExtensionDeviceSessionResultV1 Registry::qualification_texture_create(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationTextureCreateV1 *q, GDExtensionQualificationResourceBackingV1 *out) {
	if (!aligned(q) || !aligned(out) || !separate(q, sizeof(*q), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (validate_qualification_texture_shape(*q) != OK) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *b = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationBridge::Texture *c = nullptr;
	uint32_t slot = 0;
	RdUnlock guard;
	{
		Lock lock(*this);
		Error e = qualification_bridge_locked(ref, true, true, b, rd, 12);
		if (e != OK) {
			return result(e, 16);
		}
		if (q->library_generation != records[b->library].snapshot.library_generation) {
			return result(ERR_INVALID_DATA, 17);
		}
		for (auto *old : b->textures) {
			if (old && old->origin.create_attempt == q->create_attempt && same_source(old->source, q->source)) {
				if (memcmp(&old->origin, q, sizeof(*q))) {
					return result(ERR_INVALID_DATA, 16);
				}
				if (old->acknowledged) {
					return result(ERR_DOES_NOT_EXIST, 17);
				}
				if (old->creating) {
					return result(ERR_BUSY, 19);
				}
				if (old->retiring || old->physical.use_closed.load()) {
					return result(ERR_BUSY, 16);
				}
				*out = old->backing;
				return result(OK);
			}
		}
		for (; slot < QualificationBridge::CHILD_CAPACITY && b->textures[slot]; ++slot) {
		}
		if (slot == QualificationBridge::CHILD_CAPACITY) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		if (!rd->_thread_safe_.try_lock()) {
			return result(ERR_BUSY, 19);
		}
		guard.mutex = &rd->_thread_safe_;
		if (rd->_recording_error() != OK) {
			return result(rd->_recording_error(), 16);
		}
		c = memnew(QualificationBridge::Texture);
		e = qualification_resource_reserve_locked(b, rd, c, 1, q->source);
		if (e != OK) {
			memdelete(c);
			return result(e, 19);
		}
		c->origin = *q;
		b->textures[slot] = c;
	}
	RenderingDevice::TextureFormat f;
	f.texture_type = q->texture_type == 1 ? RenderingDevice::TEXTURE_TYPE_1D : q->texture_type == 2 ? RenderingDevice::TEXTURE_TYPE_2D
																									: RenderingDevice::TEXTURE_TYPE_3D;
	f.width = q->width;
	f.height = q->height;
	f.depth = q->depth;
	f.array_layers = f.mipmaps = 1;
	const bool depth = q->format == GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1;
	f.format = depth ? RenderingDevice::DATA_FORMAT_D32_SFLOAT : q->format == 1 ? RenderingDevice::DATA_FORMAT_R32G32B32A32_SFLOAT
																				: RenderingDevice::DATA_FORMAT_R8G8B8A8_UNORM;
	f.usage_bits = depth ? 0 : RenderingDevice::TEXTURE_USAGE_CAN_UPDATE_BIT;
	if (q->usage & 1) {
		f.usage_bits |= RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT;
	}
	if (q->usage & 2) {
		f.usage_bits |= RenderingDevice::TEXTURE_USAGE_STORAGE_BIT;
	}
	if (depth) {
		f.usage_bits |= RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	}
	RID rid = rd->_qualification_texture_create(f, RenderingDevice::TextureView(), &c->physical);
	Lock lock(*this);
	if (rid.is_null()) {
		CRASH_COND(!c->cleanup_serial_reserved || !qualification_cleanup_serial_reservations);
		--qualification_cleanup_serial_reservations;
		b->textures[slot] = nullptr;
		--b->children;
		--records[b->library].private_refs;
		memdelete(c);
		qualification_trim_cleanup_ref_credits_locked(b);
		return result(FAILED, 16);
	}
	c->rid = rid.get_id();
	c->creating = false;
	c->backing = { { 1, 16, 0 }, { instance, c->serial }, c->source, c->rid, c->object_serial, uint64_t(q->width) * q->height * q->depth * (q->format == 1 ? 16 : 4), 1, 1 };
	*out = c->backing;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_sampler_create(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationSamplerCreateV1 *q, GDExtensionQualificationResourceBackingV1 *out) {
	if (!aligned(q) || !aligned(out) || !separate(q, sizeof(*q), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(q->abi) || !source(q->source) || !q->library_generation || !q->create_attempt || q->mag_filter > 1 || q->min_filter > 1 || q->mip_filter > 1 || q->repeat_u >= 5 || q->repeat_v >= 5 || q->repeat_w >= 5) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *b = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationBridge::Sampler *c = nullptr;
	uint32_t slot = 0;
	RdUnlock guard;
	{
		Lock lock(*this);
		Error e = qualification_bridge_locked(ref, true, true, b, rd, 13);
		if (e != OK) {
			return result(e, 16);
		}
		if (q->library_generation != records[b->library].snapshot.library_generation) {
			return result(ERR_INVALID_DATA, 17);
		}
		for (auto *old : b->samplers) {
			if (old && old->origin.create_attempt == q->create_attempt && same_source(old->source, q->source)) {
				if (memcmp(&old->origin, q, sizeof(*q))) {
					return result(ERR_INVALID_DATA, 16);
				}
				if (old->acknowledged) {
					return result(ERR_DOES_NOT_EXIST, 17);
				}
				if (old->creating) {
					return result(ERR_BUSY, 19);
				}
				if (old->retiring || old->physical.use_closed.load()) {
					return result(ERR_BUSY, 16);
				}
				*out = old->backing;
				return result(OK);
			}
		}
		for (; slot < QualificationBridge::CHILD_CAPACITY && b->samplers[slot]; ++slot) {
		}
		if (slot == QualificationBridge::CHILD_CAPACITY) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		if (!rd->_thread_safe_.try_lock()) {
			return result(ERR_BUSY, 19);
		}
		guard.mutex = &rd->_thread_safe_;
		if (rd->_recording_error() != OK) {
			return result(rd->_recording_error(), 16);
		}
		c = memnew(QualificationBridge::Sampler);
		e = qualification_resource_reserve_locked(b, rd, c, 2, q->source);
		if (e != OK) {
			memdelete(c);
			return result(e, 19);
		}
		c->origin = *q;
		b->samplers[slot] = c;
	}
	RenderingDevice::SamplerState state;
	state.mag_filter = RenderingDevice::SamplerFilter(q->mag_filter);
	state.min_filter = RenderingDevice::SamplerFilter(q->min_filter);
	state.mip_filter = RenderingDevice::SamplerFilter(q->mip_filter);
	state.repeat_u = RenderingDevice::SamplerRepeatMode(q->repeat_u);
	state.repeat_v = RenderingDevice::SamplerRepeatMode(q->repeat_v);
	state.repeat_w = RenderingDevice::SamplerRepeatMode(q->repeat_w);
	RID rid = rd->_qualification_sampler_create(state, &c->physical);
	Lock lock(*this);
	if (rid.is_null()) {
		CRASH_COND(!c->cleanup_serial_reserved || !qualification_cleanup_serial_reservations);
		--qualification_cleanup_serial_reservations;
		b->samplers[slot] = nullptr;
		--b->children;
		--records[b->library].private_refs;
		memdelete(c);
		qualification_trim_cleanup_ref_credits_locked(b);
		return result(FAILED, 16);
	}
	c->rid = rid.get_id();
	c->creating = false;
	c->backing = { { 1, 16, 0 }, { instance, c->serial }, c->source, c->rid, c->object_serial, 0, 2, 1 };
	*out = c->backing;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_set_create(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationSetCreateV1 *q, const GDExtensionQualificationUniformBindingV1 *bindings, uint32_t count, GDExtensionQualificationResourceBackingV1 *out) {
	if (!aligned(q) || !aligned(out) || !aligned(bindings) || !count || count > 128 || !separate(q, sizeof(*q), out, sizeof(*out)) || !separate(bindings, count * sizeof(*bindings), out, sizeof(*out)) || !separate(q, sizeof(*q), bindings, count * sizeof(*bindings))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(q->abi) || !source(q->source) || !q->library_generation || !q->create_attempt || q->binding_count != count || q->shader_set > 1) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *b = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationTicket *ticket = nullptr;
	QualificationBridge::UniformSet *c = nullptr;
	uint32_t slot = 0;
	RdUnlock guard;
	Vector<RenderingDevice::Uniform> uniforms;
	LocalVector<QualificationPhysicalReturn *> children;
	{
		Lock lock(*this);
		Error e = qualification_bridge_locked(ref, true, true, b, rd, 15);
		if (e != OK) {
			return result(e, 16);
		}
		e = qualification_ticket_locked(b, q->ticket, ticket);
		if (e != OK) {
			return result(e, 17);
		}
		if (!ticket->prepared || !ticket->multi_readback || ticket->admission_closed || ticket->recorded || ticket->recording || q->source.session_serial != ticket->preparation.qualification_session_serial || q->source.generation != ticket->preparation.record_generation || q->library_generation != ticket->preparation.library_generation) {
			return result(ERR_INVALID_DATA, 16);
		}
		for (auto *old : b->sets) {
			if (old && old->origin.create_attempt == q->create_attempt && old->origin.ticket.handle_serial == q->ticket.handle_serial && old->origin.unit.handle_serial == q->unit.handle_serial && old->origin.shader_set == q->shader_set) {
				if (memcmp(&old->origin, q, sizeof(*q)) || old->bindings.size() != count || memcmp(old->bindings.ptr(), bindings, count * sizeof(*bindings))) {
					return result(ERR_INVALID_DATA, 16);
				}
				if (old->acknowledged) {
					return result(ERR_DOES_NOT_EXIST, 17);
				}
				if (old->creating) {
					return result(ERR_BUSY, 19);
				}
				if (old->retiring || old->physical.use_closed.load()) {
					return result(ERR_BUSY, 16);
				}
				*out = old->backing;
				return result(OK);
			}
		}
		QualificationBridge::NativeUnit *unit = nullptr;
		if (q->unit.registry_instance == instance) {
			for (auto *u : b->units) {
				if (u && u->serial == q->unit.handle_serial && !u->creating && !u->retiring && !u->acknowledged && u->backing.unit.registry_instance == instance && u->backing.unit.handle_serial == u->serial && u->backing.state == 1 && u->backing.known_mask == 3 && !u->backing.possible_mask && u->backing.shader_rid == q->shader_rid && u->backing.shader_serial == q->shader_serial) {
					unit = u;
					break;
				}
			}
		}
		if (!unit) {
			return result(ERR_UNAUTHORIZED, 16);
		}
		if (!rd->_thread_safe_.try_lock()) {
			return result(ERR_BUSY, 19);
		}
		guard.mutex = &rd->_thread_safe_;
		auto *shader = rd->shader_owner.get_or_null(RID::from_uint64(q->shader_rid));
		if (!shader || shader->qualification_return != &unit->shader || unit->shader.use_closed.load()) {
			return result(ERR_UNAUTHORIZED, 16);
		}
		children.push_back(&unit->shader);
		if (q->shader_set >= uint32_t(shader->uniform_sets.size()) || uint32_t(shader->uniform_sets[q->shader_set].size()) != count) {
			return result(ERR_INVALID_DATA, 2);
		}
		auto resource = [&](GDExtensionQualificationResourceRefV1 r, uint64_t rid, uint64_t serial, uint32_t kind) -> bool {
			QualificationResource *child = nullptr;
			if (qualification_resource_locked(b, r, kind, child) != OK || child->creating || child->retiring || child->rid != rid || child->object_serial != serial || child->source.session_serial != q->source.session_serial || child->source.generation != q->source.generation || memcmp(child->source.digest, q->source.digest, 32) || child->physical.use_closed.load()) {
				return false;
			}
			// Descriptor creation may precede upload; record admission checks image readiness.
			if (kind == 1 && child->upload_ticket && child->upload_ticket != ticket->serial) {
				return false;
			}
			if (!children.has(&child->physical)) {
				children.push_back(&child->physical);
			}
			return true;
		};
		for (uint32_t i = 0; i < count; ++i) {
			const auto &x = bindings[i];
			for (uint32_t j = 0; j < i; ++j) {
				if (bindings[j].binding == x.binding) {
					return result(ERR_INVALID_DATA, 2);
				}
			}
			if (x.type < 1 || x.type > 5 || !x.rid || !x.object_serial) {
				return result(ERR_INVALID_PARAMETER, 2);
			}
			const GDExtensionQualificationResourceRefV1 zero = {};
			if (x.type != 4 && (memcmp(&x.secondary, &zero, sizeof(zero)) || x.secondary_rid || x.secondary_serial)) {
				return result(ERR_INVALID_PARAMETER, 2);
			}
			RenderingDevice::Uniform u;
			u.binding = x.binding;
			if (x.type == 1) {
				QualificationBridge::Storage *stored = nullptr;
				if (x.resource.registry_instance == instance) {
					for (auto *a : b->storage) {
						if (a && a->serial == x.resource.handle_serial && !a->creating && !a->retiring && !a->acknowledged && a->backing.rid == x.rid && a->backing.backing_serial == x.object_serial) {
							stored = a;
							break;
						}
					}
				}
				auto *buffer = rd->storage_buffer_owner.get_or_null(RID::from_uint64(x.rid));
				if (!stored || !buffer || buffer->qualification_return != &stored->physical || stored->physical.use_closed.load()) {
					return result(ERR_UNAUTHORIZED, 16);
				}
				if (!children.has(&stored->physical)) {
					children.push_back(&stored->physical);
				}
				u.uniform_type = RenderingDevice::UNIFORM_TYPE_STORAGE_BUFFER;
			} else if (x.type == 2 || x.type == 5) {
				if (!resource(x.resource, x.rid, x.object_serial, 1)) {
					return result(ERR_UNAUTHORIZED, 16);
				}
				u.uniform_type = x.type == 2 ? RenderingDevice::UNIFORM_TYPE_TEXTURE : RenderingDevice::UNIFORM_TYPE_IMAGE;
			} else {
				if (!resource(x.resource, x.rid, x.object_serial, 2)) {
					return result(ERR_UNAUTHORIZED, 16);
				}
				u.uniform_type = x.type == 3 ? RenderingDevice::UNIFORM_TYPE_SAMPLER : RenderingDevice::UNIFORM_TYPE_SAMPLER_WITH_TEXTURE;
			}
			u.append_id(RID::from_uint64(x.rid));
			if (x.type == 4) {
				if (!resource(x.secondary, x.secondary_rid, x.secondary_serial, 1)) {
					return result(ERR_UNAUTHORIZED, 16);
				}
				u.append_id(RID::from_uint64(x.secondary_rid));
			}
			bool exact = false;
			for (const auto &expected : shader->uniform_sets[q->shader_set]) {
				if (expected.binding == x.binding && expected.type == u.uniform_type && (x.type == 1 || expected.length == 1)) {
					exact = true;
					break;
				}
			}
			if (!exact) {
				return result(ERR_INVALID_DATA, 2);
			}
			uniforms.push_back(u);
		}
		if (children.size() > 128) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		for (; slot < QualificationBridge::CHILD_CAPACITY && b->sets[slot]; ++slot) {
		}
		if (slot == QualificationBridge::CHILD_CAPACITY) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		c = memnew(QualificationBridge::UniformSet);
		e = qualification_resource_reserve_locked(b, rd, c, 3, q->source);
		if (e != OK) {
			memdelete(c);
			return result(e, 19);
		}
		c->origin = *q;
		c->bindings.resize(count);
		memcpy(c->bindings.ptr(), bindings, count * sizeof(*bindings));
		b->sets[slot] = c;
	}
	RID rid = rd->_qualification_uniform_set_create(uniforms, RID::from_uint64(q->shader_rid), q->shader_set, &c->physical, children);
	Lock lock(*this);
	if (rid.is_null()) {
		CRASH_COND(!c->cleanup_serial_reserved || !qualification_cleanup_serial_reservations);
		--qualification_cleanup_serial_reservations;
		b->sets[slot] = nullptr;
		--b->children;
		--records[b->library].private_refs;
		memdelete(c);
		qualification_trim_cleanup_ref_credits_locked(b);
		return result(FAILED, 16);
	}
	c->rid = rid.get_id();
	c->creating = false;
	c->backing = { { 1, 16, 0 }, { instance, c->serial }, c->source, c->rid, c->object_serial, 0, 3, 1 };
	*out = c->backing;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_texture_upload(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationTextureUploadV1 *q, const void *pixels, uint32_t bytes, GDExtensionQualificationUploadSegmentV1 *segments, uint32_t capacity, GDExtensionQualificationUploadReportV1 *out) {
	if (!aligned(q) || !aligned(out) || !aligned(segments) || !pixels || !bytes || !capacity || capacity > 4096 || !separate(q, sizeof(*q), out, sizeof(*out)) || !separate(pixels, bytes, out, sizeof(*out)) || !separate(segments, capacity * sizeof(*segments), out, sizeof(*out)) || !separate(q, sizeof(*q), pixels, bytes) || !separate(q, sizeof(*q), segments, capacity * sizeof(*segments)) || !separate(pixels, bytes, segments, capacity * sizeof(*segments))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	memset(segments, 0, capacity * sizeof(*segments));
	if (!abi(q->abi) || q->byte_size != bytes || !q->expected_revision) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *b = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationTicket *ticket = nullptr;
	QualificationResource *c = nullptr;
	Vector<uint8_t> copied;
	RdUnlock guard;
	auto snapshot = [&]() {*out=c->upload_report;out->out_written=c->upload.segments.size();for(uint32_t i=0;i<c->upload.segments.size();++i){const auto &x=c->upload.segments[i];auto &s=segments[i];s.abi={1,16,0};s.upload_index=UINT32_MAX;s.segment_index=i;s.state=x.handle.is_valid()?1:2;s.byte_begin=x.begin;s.byte_end=x.end;s.graph_instance=x.handle.graph_instance;s.graph_frame=x.handle.graph_frame;s.graph_kind=x.handle.kind;s.graph_index=x.handle.index;s.graph_offset=x.handle.offset;s.backing_serial=c->object_serial;} };
	{
		Lock lock(*this);
		Error e = qualification_bridge_locked(ref, true, true, b, rd, 14);
		if (e != OK) {
			return result(e, 16);
		}
		e = qualification_ticket_locked(b, q->ticket, ticket);
		if (e != OK) {
			return result(e, 17);
		}
		e = qualification_resource_locked(b, q->texture, 1, c);
		if (e != OK) {
			return result(e, 17);
		}
		if (!ticket->prepared || !ticket->multi_readback || ticket->recorded || ticket->recording || ticket->admission_closed || ticket->mapping_failed || ticket->append_failed || c->creating || c->retiring || c->physical.use_closed.load()) {
			return result(ERR_BUSY, 16);
		}
		if (q->record_generation != ticket->preparation.record_generation || q->parent_serial != ticket->preparation.parent_serial || q->object_serial != c->object_serial || c->source.session_serial != ticket->preparation.qualification_session_serial || c->source.generation != q->record_generation || q->byte_size != c->backing.byte_size) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (static_cast<QualificationBridge::Texture *>(c)->origin.format == GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1) {
			return result(ERR_UNAUTHORIZED, 16); // Depth content comes only from its accepted raster producer.
		}
		const auto *texture = rd->texture_owner.get_or_null(RID::from_uint64(c->rid));
		if (!texture || texture->qualification_return != &c->physical) {
			return result(ERR_UNAUTHORIZED, 16);
		}
		const uint32_t rows = texture->height * texture->depth;
		if (capacity < rows) {
			return result(ERR_PARAMETER_RANGE_ERROR, 19);
		}
		auto normalized = *q;
		normalized.expected_revision = 0;
		if (c->upload_attempted) {
			auto old = c->upload_origin;
			old.expected_revision = 0;
			if (memcmp(&old, &normalized, sizeof(old)) || c->upload_ticket != ticket->serial) {
				return result(ERR_INVALID_DATA, 16);
			}
		}
		e = copied.resize(bytes);
		if (e != OK) {
			return result(e, 19);
		}
		memcpy(copied.ptrw(), pixels, bytes);
		uint8_t digest[32];
		e = CryptoCore::sha256(copied.ptr(), bytes, digest);
		if (e != OK || memcmp(digest, q->payload_digest, 32)) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (c->upload_attempted && q->expected_revision == c->last_upload_revision) {
			snapshot();
			return result(OK);
		}
		if (q->expected_revision != ticket->report.revision) {
			return result(ERR_INVALID_DATA, 17);
		}
		if (c->upload_failed) {
			return result(ERR_BUSY, 16);
		}
		if (c->upload_attempted && c->upload.consumed == bytes) {
			snapshot();
			return result(OK);
		}
		if (!rd->_thread_safe_.try_lock()) {
			return result(ERR_BUSY, 19);
		}
		guard.mutex = &rd->_thread_safe_;
		if (rd->_recording_error() != OK || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->frame_wait_draining || rd->draw_graph.has_driver_callbacks()) {
			return result(ERR_BUSY, 19);
		}
		if (!c->upload_attempted) {
			e = qualification_typed_upload_capacity(ticket, rd, rows);
			if (e != OK) {
				return result(e, 19);
			}
			if (ticket->texture_uploads.size() >= 128) {
				return result(ERR_PARAMETER_RANGE_ERROR, 19);
			}
			c->upload.max_segments = rows;
			c->upload.segments.reserve(rows);
		}
		LocalVector<RID> resources;
		resources.push_back(RID::from_uint64(c->rid));
		e = rd->_qualification_pin_resources(ticket, resources);
		if (e != OK) {
			return result(e, 16);
		}
		if (!c->upload_attempted) {
			ticket->texture_uploads.push_back(c);
			c->upload_origin = *q;
			c->upload_ticket = ticket->serial;
		}
  e=rd->_qualification_admit_frame_ticket(ticket);
  if(e!=OK)return result(e,19);
		c->upload_attempted = true;
		c->last_upload_revision = q->expected_revision;
		ticket->recording = true;
	}
	const uint32_t previous = c->upload.segments.size();
	rd->qualification_recording = true;
	const Error error = rd->_qualification_texture_upload(RID::from_uint64(c->rid), copied.ptr(), bytes, c->upload);
	rd->qualification_recording = false;
	Lock lock(*this);
	ticket->recording = false;
	ticket->continuation_pending = error == ERR_BUSY;
	for (uint32_t i = previous; i < c->upload.segments.size(); ++i) {
		const auto &x = c->upload.segments[i];
		qualification_add_upload_graph(ticket, rd, x.handle.graph_instance, x.handle.graph_frame, x.handle.kind, x.handle.index, x.handle.offset, c->object_serial, x.begin, x.end - x.begin);
	}
	if (error != OK && error != ERR_BUSY) {
		c->upload_failed = true;
		ticket->append_failed = true;
		if (ticket->report.native_error == OK) {
			ticket->report.native_error = error;
		}
	}
	ticket->observation_dirty = true;
	qualification_ticket_publish(ticket, rd);
	auto &r = c->upload_report;
	r = {};
	r.abi = { 1, 16, 0 };
	r.revision = ticket->report.revision;
	r.record_generation = q->record_generation;
	r.parent_serial = q->parent_serial;
	r.upload_index = UINT32_MAX;
	r.native_error = error == ERR_BUSY ? OK : error;
	r.state = ticket->mapping_failed ? 3 : error == ERR_BUSY ? 4
			: error != OK									 ? (c->upload.segments.is_empty() ? 0 : 2)
															 : 1;
	r.known_count = c->upload.segments.size();
	r.appended_end = c->upload.consumed;
	r.unappended_begin = c->upload.consumed;
	r.unappended_end = bytes;
	snapshot();
	return result(OK);
}

Error Registry::qualification_depth_target_locked(QualificationBridge *b, RenderingDevice *rd, const QualificationResource *resource) {
	// Authenticate the actual pointer before reading caller-selected metadata.
	const QualificationBridge::Texture *c = nullptr;
	for (auto *candidate : b->textures) {
		if (candidate && candidate == resource) {
			c = candidate;
			break;
		}
	}
	if (!c || c->kind != 1 || c->creating || c->retiring || c->acknowledged || c->maintenance_error != OK || !c->serial || !c->rid || !c->object_serial || c->physical.bridge != b->serial || c->physical.child != c->serial || c->physical.rid != c->rid || c->physical.serial != c->object_serial || c->physical.use_closed.load() || !abi(c->backing.abi) || c->backing.kind != 1 || c->backing.state != 1 || c->backing.resource.registry_instance != instance || c->backing.resource.handle_serial != c->serial || c->backing.rid != c->rid || c->backing.object_serial != c->object_serial || !same_source(c->source, c->backing.source) || !same_source(c->source, c->origin.source) || validate_qualification_texture_shape(c->origin) != OK || c->origin.format != GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1) {
		return ERR_UNAUTHORIZED;
	}
	const auto *texture = rd->texture_owner.get_or_null(RID::from_uint64(c->rid));
	if (!texture || !texture->driver_id || texture->qualification_return != &c->physical || texture->type != RenderingDevice::TEXTURE_TYPE_2D || texture->format != RenderingDevice::DATA_FORMAT_D32_SFLOAT || texture->samples != RenderingDevice::TEXTURE_SAMPLES_1 || texture->width != c->origin.width || texture->height != c->origin.height || texture->depth != 1 || texture->layers != 1 || texture->mipmaps != 1 || texture->usage_flags != (RenderingDevice::TEXTURE_USAGE_SAMPLING_BIT | RenderingDevice::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) || texture->owner.is_valid() || texture->shared_fallback || texture->transfer_worker_index >= 0) {
		return ERR_UNAVAILABLE;
	}
	if (c->upload_attempted || c->upload_failed || c->upload_ticket || c->upload.consumed) {
		return ERR_UNAUTHORIZED;
	}
	return OK;
}

Error Registry::qualification_depth_content_locked(QualificationBridge *b, RenderingDevice *rd, QualificationTicket *ticket, const GDExtensionQualificationRecordV2 &q, const QualificationResource *resource, bool p_pending) {
	Error error = qualification_depth_target_locked(b, rd, resource);
	if (error != OK) {
		return error;
	}
	const auto *c = static_cast<const QualificationBridge::Texture *>(resource);
	if (!ticket || !ticket->serial || ticket->bridge_serial != b->serial || c->origin.library_generation != q.library_generation || c->source.session_serial != q.qualification_session_serial || c->source.generation != q.record_generation || memcmp(c->source.digest, q.body_map_digest, sizeof(c->source.digest)) || ticket->texture_uploads.has(const_cast<QualificationBridge::Texture *>(c))) {
		return ERR_UNAUTHORIZED;
	}
	if (p_pending) {
		// Caller already authenticated source/raster/original operation. This is
		// only a pending descriptor exception, never produced-content authority.
		return OK;
	}
	const auto &depth = ticket->depth_report;
	const auto *texture = rd->texture_owner.get_or_null(RID::from_uint64(c->rid));
	if (!ticket->operation_bound || !valid_original_operation_identity(ticket->operation) || ticket->operation.owner_request_nonce != q.request_nonce || ticket->operation.library_generation != q.library_generation || ticket->operation.qualification_session_serial != q.qualification_session_serial || ticket->operation.record_generation != q.record_generation || !same_original_operation_identity(c->produced_operation, ticket->operation) || !ticket->depth_bound || !depth.content_revision || !c->content_revision || c->content_revision != depth.content_revision || c->produced_ticket != ticket->serial || !c->produced_raster || depth.raster.registry_instance != instance || c->produced_raster != depth.raster.handle_serial || depth.target.registry_instance != instance || depth.target.handle_serial != c->serial || depth.target_object_serial != c->object_serial || !same_source(depth.source, c->source) || texture->pending_clear) {
		return ERR_UNAUTHORIZED;
	}
	return OK;
}

Error Registry::qualification_typed_dependencies(QualificationBridge *b, RenderingDevice *rd, QualificationTicket *ticket, const GDExtensionQualificationRecordV2 &q, const LocalVector<RID> &resources, const QualificationResource *p_pending_depth) {
	if (p_pending_depth) {
		Error error = qualification_depth_content_locked(b, rd, ticket, q, p_pending_depth, true);
		if (error != OK) {
			return error;
		}
	}
	for (RID rid : resources) {
		for (auto *c : b->textures) {
			if (c && c->rid == rid.get_id()) {
				if (c->origin.format == GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1) {
					Error error = qualification_depth_content_locked(b, rd, ticket, q, c, c == p_pending_depth);
					if (error != OK) {
						return error;
					}
					continue;
				}
				if (c->creating || c->retiring || c->source.session_serial != q.qualification_session_serial || c->source.generation != q.record_generation || memcmp(c->source.digest, q.body_map_digest, 32) || !c->upload_attempted || c->upload_failed || c->upload_ticket != ticket->serial || c->upload.consumed != c->backing.byte_size) {
					return ERR_UNAUTHORIZED;
				}
			}
		}
		for (auto *c : b->samplers) {
			if (c && c->rid == rid.get_id()) {
				if (c->creating || c->retiring || c->source.session_serial != q.qualification_session_serial || c->source.generation != q.record_generation || memcmp(c->source.digest, q.body_map_digest, 32)) {
					return ERR_UNAUTHORIZED;
				}
			}
		}
	}
	const uint64_t phase_sets[2][2] = { { q.capture_set, q.capture_set1 }, { q.body_set, q.body_set1 } };
	const uint64_t pipelines[2] = { q.capture_pipeline, q.body_pipeline };
	const uint32_t counts[2] = { q.capture_set_count, q.body_set_count };
	for (uint32_t record_phase = 0; record_phase < 2; ++record_phase) {
		for (uint32_t index = 0; index < counts[record_phase]; ++index) {
			QualificationBridge::UniformSet *c = nullptr;
			for (auto *candidate : b->sets) {
				if (candidate && candidate->rid == phase_sets[record_phase][index] && !candidate->acknowledged) {
					c = candidate;
					break;
				}
			}
			if (!c) {
				if (q.readback_count == 3) {
					return ERR_UNAUTHORIZED;
				}
				continue;
			}
			const uint8_t *expected = record_phase == 0 ? q.capture_map_digest : q.body_map_digest;
			auto *set = rd->uniform_set_owner.get_or_null(RID::from_uint64(c->rid));
			auto *pipeline = rd->compute_pipeline_owner.get_or_null(RID::from_uint64(pipelines[record_phase]));
			if (!set || set->qualification_return != &c->physical || !pipeline || pipeline->shader.get_id() != c->origin.shader_rid || c->creating || c->retiring || c->source.session_serial != q.qualification_session_serial || c->source.generation != q.record_generation || c->origin.ticket.registry_instance != instance || c->origin.ticket.handle_serial != ticket->serial || c->origin.shader_set != index || memcmp(c->source.digest, expected, 32)) {
				return ERR_UNAUTHORIZED;
			}
			bool unit = false;
			for (auto *u : b->units) {
				if (u && c->origin.unit.registry_instance == instance && u->serial == c->origin.unit.handle_serial && !u->creating && !u->retiring && !u->acknowledged && u->backing.unit.registry_instance == instance && u->backing.unit.handle_serial == u->serial && u->backing.state == 1 && u->backing.known_mask == 3 && !u->backing.possible_mask && u->backing.pipeline_rid == pipelines[record_phase] && u->backing.shader_rid == c->origin.shader_rid && u->backing.shader_serial == c->origin.shader_serial && pipeline->qualification_return == &u->pipeline) {
					unit = true;
					break;
				}
			}
			if (!unit) {
				return ERR_UNAUTHORIZED;
			}
		}
	}
	for (auto *c : ticket->texture_uploads) {
		if (c->source.session_serial != q.qualification_session_serial || c->source.generation != q.record_generation || memcmp(c->source.digest, q.body_map_digest, 32) || !resources.has(RID::from_uint64(c->rid))) {
			return ERR_UNAUTHORIZED;
		}
		if (c->upload_failed || c->upload.consumed != c->backing.byte_size || c->upload_report.state != 1) {
			return ERR_BUSY;
		}
	}
	return OK;
}
void Registry::qualification_resource_retire_step(QualificationBridge *b, RenderingDevice *rd, QualificationResource *c) {
	if (!c || !c->retiring) {
		return;
	}
	uint32_t reason = 0;
	{
		Lock lock(*this);
		if (c->accepted_rt_refs) {
			reason = 3;
		}
	}
	if (c->maintenance_error != OK) {
		reason = 5;
	}
	if (!reason && !c->physical.enqueued) {
		if (c->physical.production_uses) {
			reason = 3;
		}
		for (auto *ticket : c->physical.uses) {
			if (!ticket->physically_retired.load()) {
				reason = 3;
			}
		}
		auto *dependents = rd->dependency_map.getptr(RID::from_uint64(c->rid));
		if (c->physical.dependent_children.load() || (dependents && !dependents->is_empty())) {
			reason = 4;
		}
		if (!reason) {
			RID rid = RID::from_uint64(c->rid);
			auto *texture = rd->texture_owner.owns(rid) ? rd->texture_owner.get_or_null(rid) : nullptr;
			auto *sampler = rd->sampler_owner.owns(rid) ? rd->sampler_owner.get_or_null(rid) : nullptr;
			auto *set = rd->uniform_set_owner.owns(rid) ? rd->uniform_set_owner.get_or_null(rid) : nullptr;
			auto *actual = texture ? texture->qualification_return : sampler ? sampler->qualification_return
					: set													 ? set->qualification_return
																			 : nullptr;
			if (actual != &c->physical) {
				reason = 6;
			} else if (texture && (texture->transfer_worker_index >= 0 || texture->shared_fallback)) {
				reason = 3;
			} else if (!rd->_qualification_free_event_available(c->physical.event_capacity_reserved) || !rd->frames[rd->frame].qualification_epoch) {
				reason = 5;
			} else if (rd->_recording_error() != OK || rd->frame_wait_draining || rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active || rd->draw_graph.has_driver_callbacks()) {
				reason = 1;
			} else {
				const bool previous_retiring = rd->qualification_retiring;
				rd->qualification_retiring = true;
				// qualification_retire_step owns one outer RD lock; the retained
				// original child survives synchronous callbacks without that lock.
				rd->_thread_safe_.unlock();
				rd->free_rid(rid);
				rd->_thread_safe_.lock();
				rd->qualification_retiring = previous_retiring;
				if (!c->physical.enqueued) {
					reason = 5;
				}
			}
		}
	}
	Lock lock(*this);
	MutexLock physical_lock(c->physical.mutex);
	auto next = c->report;
	next.frame_slot = c->physical.frame_slot;
	next.frame_epoch = c->physical.frame_epoch;
	next.event_seq = c->physical.free_event;
	if (c->physical.enqueued) {
		next.dependency_serial = c->dependency_serial;
	}
	if (c->maintenance_error != OK) {
		next.state = 2;
		next.native_error = c->maintenance_error;
		next.reason = 5;
	} else if (c->physical.free_event) {
		next.state = 1;
		next.return_status = 1;
		next.reason = 0;
	} else {
		next.reason = reason ? reason : 1;
		if (reason >= 5) {
			next.state = 2;
		}
	}
	if (memcmp(&next, &c->report, sizeof(next))) {
		if (next.revision == UINT64_MAX) {
			next.state = 2;
			next.return_status = 0;
			next.reason = 5;
		} else {
			++next.revision;
		}
		c->report = next;
	}
}
GDExtensionDeviceSessionResultV1 Registry::qualification_resource_retire(GDExtensionQualificationBridgeRefV1 ref, const GDExtensionQualificationResourceRetireV1 *q, GDExtensionQualificationResourceRetireReportV1 *out) {
	if (!aligned(q) || !aligned(out) || !separate(q, sizeof(*q), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(q->abi) || !q->retire_operation) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	QualificationBridge *b = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationResource *c = nullptr;
	{
		Lock lock(*this);
		Error e = qualification_bridge_locked(ref, true, false, b, rd);
		if (e != OK) {
			return result(e, 16);
		}
		e = qualification_resource_locked(b, q->resource, 0, c);
		if (e != OK) {
			return result(e, 17);
		}
		if (c->creating) {
			return result(ERR_BUSY, 16);
		}
		if (q->rid != c->rid || q->object_serial != c->object_serial) {
			return result(ERR_INVALID_DATA, 16);
		}
		if (c->retiring) {
			if (memcmp(&c->retirement, q, sizeof(*q))) {
				return result(ERR_INVALID_DATA, 16);
			}
			*out = c->report;
			return result(OK);
		}
		c->retirement = *q;
		c->retiring = true;
		c->physical.use_closed.store(true);
		auto &r = c->report;
		r.abi = { 1, 16, 0 };
		r.resource = q->resource;
		r.rid = c->rid;
		r.object_serial = c->object_serial;
		r.retire_operation = q->retire_operation;
		r.revision = 1;
		r.frame_slot = UINT32_MAX;
		r.kind = c->kind;
	}
	qualification_retire_step(b, rd, c->serial, 4);
	Lock lock(*this);
	*out = c->report;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_resource_retire_observe(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationResourceRefV1 resource, const GDExtensionQualificationBudgetV1 *budget, GDExtensionQualificationResourceRetireReportV1 *out) {
	if (!aligned(budget) || !aligned(out) || !separate(budget, sizeof(*budget), out, sizeof(*out))) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	*out = {};
	if (!abi(budget->abi) || budget->reserved || budget->max_passes < 1 || budget->max_passes > 32 || budget->max_micros < 1 || budget->max_micros > 5000) {
		return result(ERR_INVALID_PARAMETER, 2);
	}
	uint64_t serial = 0;
	{
		Lock lock(*this);
		QualificationBridge *b = nullptr;
		RenderingDevice *rd = nullptr;
		QualificationResource *c = nullptr;
		Error e = qualification_bridge_locked(ref, false, false, b, rd);
		if (e != OK) {
			return result(e, 17);
		}
		e = qualification_resource_locked(b, resource, 0, c);
		if (e != OK || !c->retiring) {
			return result(ERR_DOES_NOT_EXIST, 17);
		}
		serial = c->serial;
	}
	Error e = qualification_schedule(ref, serial, 4, *budget);
	if (e != OK) {
		return result(e, 17);
	}
	Lock lock(*this);
	QualificationBridge *b = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationResource *c = nullptr;
	e = qualification_bridge_locked(ref, false, false, b, rd);
	if (e != OK) {
		return result(e, 17);
	}
	e = qualification_resource_locked(b, resource, 0, c);
	if (e != OK) {
		return result(e, 17);
	}
	*out = c->report;
	return result(OK);
}
GDExtensionDeviceSessionResultV1 Registry::qualification_resource_retire_release(GDExtensionQualificationBridgeRefV1 ref, GDExtensionQualificationResourceRefV1 resource, uint64_t revision) {
	Lock lock(*this);
	QualificationBridge *b = nullptr;
	RenderingDevice *rd = nullptr;
	QualificationResource *c = nullptr;
	Error e = qualification_bridge_locked(ref, false, false, b, rd);
	if (e != OK) {
		return result(e, 17);
	}
	e = qualification_resource_locked(b, resource, 0, c);
	if (e != OK) {
		return result(e, 17);
	}
	const auto &r = c->report;
	MutexLock physical_lock(c->physical.mutex);
	if (!abi(r.abi) || r.resource.registry_instance != instance || r.resource.handle_serial != c->serial || r.kind != c->kind || c->physical.bridge != b->serial || c->physical.child != c->serial || c->physical.rid != c->rid || c->physical.serial != c->object_serial || !c->physical.use_closed.load() || !c->physical.enqueued || !c->physical.reserved_event || c->physical.free_event != c->physical.reserved_event || !r.frame_epoch || r.frame_slot == UINT32_MAX || !revision || revision != r.revision || !c->retiring || c->creating || c->queued || c->accepted_rt_refs || c->maintenance_error != OK || r.state != 1 || r.return_status != 1 || r.reason || r.native_error || r.native_code || r.rid != c->rid || r.object_serial != c->object_serial || r.retire_operation != c->retirement.retire_operation || r.dependency_serial != c->dependency_serial || r.frame_slot != c->physical.frame_slot || r.frame_epoch != c->physical.frame_epoch || !r.event_seq || r.event_seq != c->physical.free_event || c->physical.dependent_children.load() || !c->physical.uses.is_empty() || c->physical.production_uses) {
		return result(ERR_BUSY, 16);
	}
	if (c->cleanup_serial_reserved) {
		CRASH_COND(!qualification_cleanup_serial_reservations);
		--qualification_cleanup_serial_reservations;
		c->cleanup_serial_reserved = false;
	}
	c->acknowledged = true;
	--b->children;
	--records[b->library].private_refs;
	qualification_trim_cleanup_ref_credits_locked(b);
	return result(OK);
}
Error Registry::qualification_rt_resource_refs_locked(QualificationBridge *b, const GDExtensionQualificationRtRequestV1 &h, const uint8_t *bytes, const uint8_t *payload, LocalVector<QualificationResource *> &refs) {
	refs.clear();
	auto keep = [&](GDExtensionQualificationResourceRefV1 ref, uint64_t rid, uint64_t serial, uint32_t kind, bool closing) -> Error {QualificationResource *c=nullptr;Error e=qualification_resource_locked(b,ref,kind,c);if(e!=OK){return e;
}if(c->creating||(!closing&&c->retiring)||c->rid!=rid||c->object_serial!=serial){return ERR_INVALID_DATA;
}if(!refs.has(c)){if(refs.size()>=128){return ERR_PARAMETER_RANGE_ERROR;
}refs.push_back(c);}return OK; };
	if (h.kind == GDEXTENSION_QUALIFICATION_RT_RECORD_PREPARED_V4) {
		GDExtensionQualificationRecordV4 q;
		memcpy(&q, bytes, sizeof(q));
		QualificationResource *c = nullptr;
		Error e = qualification_resource_locked(b, q.depth.target, 1, c);
		if (e != OK) {
			return e;
		}
		if (memcmp(&c->source, &q.depth.source, sizeof(q.depth.source)) || static_cast<QualificationBridge::Texture *>(c)->origin.format != GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1) {
			return ERR_INVALID_DATA;
		}
		return keep(q.depth.target, c->rid, q.depth.target_object_serial, 1, false);
	}
	if (h.kind == GDEXTENSION_QUALIFICATION_RT_RASTER_CREATE_V1) {
		GDExtensionQualificationRasterCreateV1 q;
		memcpy(&q, bytes, sizeof(q));
		QualificationResource *c = nullptr;
		Error e = qualification_resource_locked(b, q.target, 1, c);
		if (e != OK) {
			return e;
		}
		if (memcmp(&c->source, &q.source, sizeof(q.source)) || static_cast<QualificationBridge::Texture *>(c)->origin.format != GDEXTENSION_QUALIFICATION_TEXTURE_FORMAT_D32_SFLOAT_V1) {
			return ERR_INVALID_DATA;
		}
		return keep(q.target, c->rid, q.target_object_serial, 1, false);
	}
	if (h.kind == 14) {
		GDExtensionQualificationTextureUploadV1 q;
		memcpy(&q, bytes, sizeof(q));
		QualificationResource *c = nullptr;
		Error e = qualification_resource_locked(b, q.texture, 1, c);
		if (e != OK) {
			return e;
		}
		return keep(q.texture, c->rid, q.object_serial, 1, false);
	}
	if (h.kind == 16) {
		GDExtensionQualificationResourceRetireV1 q;
		memcpy(&q, bytes, sizeof(q));
		return keep(q.resource, q.rid, q.object_serial, 0, true);
	}
	if (h.kind == 15) {
		GDExtensionQualificationSetCreateV1 q;
		memcpy(&q, bytes, sizeof(q));
		for (uint32_t i = 0; i < q.binding_count; ++i) {
			GDExtensionQualificationUniformBindingV1 x;
			memcpy(&x, payload + i * sizeof(x), sizeof(x));
			if (x.type < 1 || x.type > 5) {
				return ERR_INVALID_PARAMETER;
			}
			if (x.type != 1) {
				Error e = keep(x.resource, x.rid, x.object_serial, x.type == 2 || x.type == 5 ? 1 : 2, false);
				if (e != OK) {
					return e;
				}
			}
			if (x.type == 4) {
				Error e = keep(x.secondary, x.secondary_rid, x.secondary_serial, 1, false);
				if (e != OK) {
					return e;
				}
			}
		}
	}
	return OK;
}
namespace {
static GDExtensionDeviceSessionResultV1 texture_create(GDExtensionQualificationBridgeRefV1 b, const GDExtensionQualificationTextureCreateV1 *q, GDExtensionQualificationResourceBackingV1 *o) {
	return Registry::get_singleton().qualification_texture_create(b, q, o);
}
static GDExtensionDeviceSessionResultV1 sampler_create(GDExtensionQualificationBridgeRefV1 b, const GDExtensionQualificationSamplerCreateV1 *q, GDExtensionQualificationResourceBackingV1 *o) {
	return Registry::get_singleton().qualification_sampler_create(b, q, o);
}
static GDExtensionDeviceSessionResultV1 set_create(GDExtensionQualificationBridgeRefV1 b, const GDExtensionQualificationSetCreateV1 *q, const GDExtensionQualificationUniformBindingV1 *p, uint32_t n, GDExtensionQualificationResourceBackingV1 *o) {
	return Registry::get_singleton().qualification_set_create(b, q, p, n, o);
}
static GDExtensionDeviceSessionResultV1 texture_upload(GDExtensionQualificationBridgeRefV1 b, const GDExtensionQualificationTextureUploadV1 *q, const void *p, uint32_t n, GDExtensionQualificationUploadSegmentV1 *s, uint32_t c, GDExtensionQualificationUploadReportV1 *o) {
	return Registry::get_singleton().qualification_texture_upload(b, q, p, n, s, c, o);
}
static GDExtensionDeviceSessionResultV1 resource_retire(GDExtensionQualificationBridgeRefV1 b, const GDExtensionQualificationResourceRetireV1 *q, GDExtensionQualificationResourceRetireReportV1 *o) {
	return Registry::get_singleton().qualification_resource_retire(b, q, o);
}
static GDExtensionDeviceSessionResultV1 resource_observe(GDExtensionQualificationBridgeRefV1 b, GDExtensionQualificationResourceRefV1 r, const GDExtensionQualificationBudgetV1 *q, GDExtensionQualificationResourceRetireReportV1 *o) {
	return Registry::get_singleton().qualification_resource_retire_observe(b, r, q, o);
}
static GDExtensionDeviceSessionResultV1 resource_release(GDExtensionQualificationBridgeRefV1 b, GDExtensionQualificationResourceRefV1 r, uint64_t v) {
	return Registry::get_singleton().qualification_resource_retire_release(b, r, v);
}
} //namespace
static_assert(std::is_same_v<decltype(&texture_create), GDExtensionInterfaceDeviceSessionQualificationTextureCreateV1>);
static_assert(std::is_same_v<decltype(&sampler_create), GDExtensionInterfaceDeviceSessionQualificationSamplerCreateV1>);
static_assert(std::is_same_v<decltype(&set_create), GDExtensionInterfaceDeviceSessionQualificationSetCreateV1>);
static_assert(std::is_same_v<decltype(&texture_upload), GDExtensionInterfaceDeviceSessionQualificationTextureUploadV1>);
static_assert(std::is_same_v<decltype(&resource_retire), GDExtensionInterfaceDeviceSessionQualificationResourceRetireV1>);
static_assert(std::is_same_v<decltype(&resource_observe), GDExtensionInterfaceDeviceSessionQualificationResourceRetireObserveV1>);
static_assert(std::is_same_v<decltype(&resource_release), GDExtensionInterfaceDeviceSessionQualificationResourceRetireReleaseV1>);
void register_qualification_typed_interfaces() {
	GDExtension::register_interface_function("device_session_qualification_texture_create_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&texture_create));
	GDExtension::register_interface_function("device_session_qualification_sampler_create_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&sampler_create));
	GDExtension::register_interface_function("device_session_qualification_set_create_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&set_create));
	GDExtension::register_interface_function("device_session_qualification_texture_upload_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&texture_upload));
	GDExtension::register_interface_function("device_session_qualification_resource_retire_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&resource_retire));
	GDExtension::register_interface_function("device_session_qualification_resource_retire_observe_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&resource_observe));
	GDExtension::register_interface_function("device_session_qualification_resource_retire_release_v1", reinterpret_cast<GDExtensionInterfaceFunctionPtr>(&resource_release));
}
} // namespace GDExtensionDeviceExit
