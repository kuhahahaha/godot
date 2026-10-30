/**************************************************************************/
/*  render_data_rd.h                                                      */
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

#include "servers/rendering/renderer_rd/storage_rd/render_scene_buffers_rd.h"
#include "servers/rendering/renderer_rd/storage_rd/render_scene_data_rd.h"
#include "servers/rendering/rendering_server_types.h"
#include "servers/rendering/storage/render_data.h"

class RenderDataRD : public RenderData {
	GDCLASS(RenderDataRD, RenderData);

public:
	enum class PacketAdmissionStatus {
		ADMITTED,
		ORDINARY_MESH,
		CAMERA_SOURCE_MISSING,
		MATERIAL_GENERATION_MISMATCH,
		RETIRED,
	};

	// Immutable identity carried with one admitted render packet. Material
	// generation/capability are intentionally explicit so an external packet
	// consumer can fail closed without changing ordinary Mesh rendering.
	struct PacketIdentity {
		uint64_t view_id = 0;
		uint64_t view_epoch = 0;
		uint64_t camera_capture_ordinal = 0;
		uint64_t previous_view_id = 0;
		uint64_t previous_view_epoch = 0;
		uint64_t previous_camera_capture_ordinal = 0;
		uint64_t material_generation = 0;
		uint32_t material_capabilities = 0;
		bool external = false;
		bool retired = false;

		bool has_camera_source() const {
			return !retired && view_id != 0 && view_epoch != 0 && camera_capture_ordinal != 0;
		}
		bool camera_source_matches(uint64_t p_view_id, uint64_t p_view_epoch, uint64_t p_capture_ordinal) const {
			return !retired && has_camera_source() && view_id == p_view_id && view_epoch == p_view_epoch && camera_capture_ordinal == p_capture_ordinal;
		}
		bool has_previous_camera_source() const {
			return !retired && previous_view_id != 0 && previous_view_epoch != 0 && previous_camera_capture_ordinal != 0;
		}
		bool previous_camera_source_matches(uint64_t p_view_id, uint64_t p_view_epoch, uint64_t p_capture_ordinal) const {
			return !retired && has_previous_camera_source() && previous_view_id == p_view_id && previous_view_epoch == p_view_epoch && previous_camera_capture_ordinal == p_capture_ordinal;
		}
		bool material_admitted(uint64_t p_generation, uint32_t p_required_capabilities) const {
			return external && !retired && material_generation == p_generation && p_generation != 0 && p_generation != UINT64_MAX && (material_capabilities & p_required_capabilities) == p_required_capabilities;
		}
		bool surface_admitted(uint64_t p_generation, uint32_t p_required_capabilities = 0) const {
			return !retired && (!external || material_admitted(p_generation, p_required_capabilities));
		}
		bool external_camera_admitted() const {
			return !retired && (!external || has_camera_source());
		}
		void invalidate_material_admission() {
			material_generation = 0;
			material_capabilities = 0;
		}
		void retire() {
			view_id = 0;
			view_epoch = 0;
			camera_capture_ordinal = 0;
			previous_view_id = 0;
			previous_view_epoch = 0;
			previous_camera_capture_ordinal = 0;
			invalidate_material_admission();
			retired = true;
		}
	};

	// Internal retirement acknowledgment for a packet's native references.
	// This is an identity receipt only; it does not claim GPU completion.
	struct NativeRetirementReceipt {
		uint64_t view_id = 0;
		uint64_t view_epoch = 0;
		uint64_t capture_ordinal = 0;
		uint64_t previous_capture_ordinal = 0;
		uint64_t material_generation = 0;

		bool is_valid() const {
			return view_id != 0 && view_epoch != 0 && capture_ordinal != 0 && previous_capture_ordinal != 0 && material_generation != 0;
		}
	};

	mutable PacketIdentity packet_identity;
	mutable NativeRetirementReceipt native_retirement_receipt;
	mutable bool native_retirement_receipt_issued = false;
	mutable bool native_retirement_acknowledged_state = false;

	bool set_camera_source(uint64_t p_view_id, uint64_t p_view_epoch, uint64_t p_capture_ordinal) {
		if (packet_identity.retired || native_retirement_receipt_issued) {
			return false;
		}
		packet_identity.view_id = p_view_id;
		packet_identity.view_epoch = p_view_epoch;
		packet_identity.camera_capture_ordinal = p_capture_ordinal;
		return true;
	}
	bool set_previous_camera_source(uint64_t p_view_id, uint64_t p_view_epoch, uint64_t p_capture_ordinal) {
		if (packet_identity.retired || native_retirement_receipt_issued) {
			return false;
		}
		packet_identity.previous_view_id = p_view_id;
		packet_identity.previous_view_epoch = p_view_epoch;
		packet_identity.previous_camera_capture_ordinal = p_capture_ordinal;
		return true;
	}
	bool previous_camera_source_admitted() const {
		if (packet_identity.retired) {
			return false;
		}
		if (!packet_identity.external) {
			return true;
		}
		if (!packet_identity.has_previous_camera_source()) {
			return false;
		}
		// Previous camera history belongs to the same presentation lineage. The
		// first frame may intentionally carry the current tuple as its previous
		// sample, so equal ordinals remain valid.
		return packet_identity.has_camera_source() &&
				packet_identity.previous_view_id == packet_identity.view_id &&
				packet_identity.previous_view_epoch == packet_identity.view_epoch &&
				packet_identity.previous_camera_capture_ordinal <= packet_identity.camera_capture_ordinal;
	}
	bool previous_camera_source_matches(uint64_t p_view_id, uint64_t p_view_epoch, uint64_t p_capture_ordinal) const {
		return packet_identity.previous_camera_source_matches(p_view_id, p_view_epoch, p_capture_ordinal);
	}
	NativeRetirementReceipt issue_native_retirement_receipt() const {
		if (native_retirement_receipt_issued || native_retirement_acknowledged_state || !packet_identity.external || !packet_identity.has_camera_source() || !previous_camera_source_admitted() || packet_identity.material_generation == 0) {
			return NativeRetirementReceipt();
		}
		native_retirement_receipt.view_id = packet_identity.view_id;
		native_retirement_receipt.view_epoch = packet_identity.view_epoch;
		native_retirement_receipt.capture_ordinal = packet_identity.camera_capture_ordinal;
		native_retirement_receipt.previous_capture_ordinal = packet_identity.previous_camera_capture_ordinal;
		native_retirement_receipt.material_generation = packet_identity.material_generation;
		native_retirement_receipt_issued = true;
		return native_retirement_receipt;
	}
	bool acknowledge_native_retirement(uint64_t p_view_id, uint64_t p_view_epoch, uint64_t p_capture_ordinal, uint64_t p_previous_capture_ordinal, uint64_t p_material_generation) const {
		if (!native_retirement_receipt_issued || native_retirement_acknowledged_state || p_view_id != native_retirement_receipt.view_id || p_view_epoch != native_retirement_receipt.view_epoch || p_capture_ordinal != native_retirement_receipt.capture_ordinal || p_previous_capture_ordinal != native_retirement_receipt.previous_capture_ordinal || p_material_generation != native_retirement_receipt.material_generation) {
			return false;
		}
		native_retirement_acknowledged_state = true;
		packet_identity.retire();
		return true;
	}
	bool native_retirement_acknowledged() const {
		return native_retirement_acknowledged_state;
	}

	// Shared consumer seam for renderer backends. These wrappers preserve the
	// PacketIdentity policy while keeping camera/surface admission call sites
	// identical across Forward+ and Mobile.
	bool external_camera_admitted() const {
		return packet_identity.external_camera_admitted();
	}
	bool surface_admitted(uint64_t p_generation, uint32_t p_required_capabilities = 0) const {
		return packet_identity.surface_admitted(p_generation, p_required_capabilities);
	}
	void retire_packet() const {
		packet_identity.retire();
	}
	PacketAdmissionStatus admit_camera_status() const {
		if (packet_identity.retired) {
			return PacketAdmissionStatus::RETIRED;
		}
		if (!packet_identity.external) {
			return PacketAdmissionStatus::ORDINARY_MESH;
		}
		if (!packet_identity.has_camera_source()) {
			retire_packet();
			return PacketAdmissionStatus::CAMERA_SOURCE_MISSING;
		}
		return PacketAdmissionStatus::ADMITTED;
	}
	PacketAdmissionStatus admit_surface_status(uint64_t p_generation, uint32_t p_required_capabilities = 0) const {
		if (packet_identity.retired) {
			return PacketAdmissionStatus::RETIRED;
		}
		if (!packet_identity.external) {
			return PacketAdmissionStatus::ORDINARY_MESH;
		}
		if (!packet_identity.material_admitted(p_generation, p_required_capabilities)) {
			retire_packet();
			return PacketAdmissionStatus::MATERIAL_GENERATION_MISMATCH;
		}
		return PacketAdmissionStatus::ADMITTED;
	}
	static bool admission_succeeded(PacketAdmissionStatus p_status) {
		return p_status == PacketAdmissionStatus::ADMITTED || p_status == PacketAdmissionStatus::ORDINARY_MESH;
	}
	bool admit_camera() const {
		return admission_succeeded(admit_camera_status());
	}
	bool admit_surface(uint64_t p_generation, uint32_t p_required_capabilities = 0) const {
		return admission_succeeded(admit_surface_status(p_generation, p_required_capabilities));
	}

	// Access methods to expose data externally
	virtual Ref<RenderSceneBuffers> get_render_scene_buffers() const override { return render_buffers; }
	virtual RenderSceneData *get_render_scene_data() const override { return scene_data; }

	virtual RID get_environment() const override { return environment; }
	virtual RID get_camera_attributes() const override { return camera_attributes; }

	// Members are publicly accessible within the render engine.
	Ref<RenderSceneBuffersRD> render_buffers;
	RenderSceneDataRD *scene_data = nullptr;

	const PagedArray<RenderGeometryInstance *> *instances = nullptr;
	const PagedArray<RID> *lights = nullptr;
	const PagedArray<RID> *reflection_probes = nullptr;
	const PagedArray<RID> *voxel_gi_instances = nullptr;
	const PagedArray<RID> *decals = nullptr;
	const PagedArray<RID> *lightmaps = nullptr;
	const PagedArray<RID> *fog_volumes = nullptr;
	RID environment;
	RID camera_attributes;
	RID compositor;
	RID shadow_atlas;
	RID occluder_debug_tex;
	RID reflection_atlas;
	RID reflection_probe;
	int reflection_probe_pass = 0;

	RID cluster_buffer;
	uint32_t cluster_size = 0;
	uint32_t cluster_max_elements = 0;

	uint32_t directional_light_count = 0;
	bool directional_light_soft_shadows = false;

	bool lightmap_bicubic_filter = false;

	float window_output_max_value = 1.0;

	RenderingServerTypes::RenderInfo *render_info = nullptr;

	/* Viewport data */
	bool transparent_bg = false;
	Rect2i render_region;

	/* Shadow data */
	const RendererSceneRender::RenderShadowData *render_shadows = nullptr;
	int render_shadow_count = 0;

	LocalVector<int> cube_shadows;
	LocalVector<int> shadows;
	LocalVector<int> directional_shadows;

	/* GI info */
	const RendererSceneRender::RenderSDFGIData *render_sdfgi_regions = nullptr;
	int render_sdfgi_region_count = 0;
	const RendererSceneRender::RenderSDFGIUpdateData *sdfgi_update_data = nullptr;

	uint32_t voxel_gi_count = 0;
};
