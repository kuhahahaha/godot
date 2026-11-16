/**************************************************************************/
/*  gdextension_production_material_qualification.cpp                     */
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

#include "core/extension/gdextension_production_material_qualification.h"

#include "core/crypto/crypto_core.h"
#include "core/os/os.h"
#include "servers/rendering/renderer_rd/renderer_compositor_rd.h"
#include "servers/rendering/renderer_rd/renderer_scene_render_rd.h"
#include "servers/rendering/rendering_device.h"
#include "servers/rendering/rendering_server.h"
#include "servers/rendering/rendering_server_default.h"
#include "servers/rendering/rendering_server_globals.h"

#include <cstring>
#include <new>

namespace GDExtensionDeviceExit {

static_assert(sizeof(GDExtensionMaterialQualificationTextV1) == 16);
static_assert(sizeof(GDExtensionMaterialQualificationDeviceV1) == 16);
static_assert(sizeof(GDExtensionMaterialQualificationDigestV1) == 32);
static_assert(sizeof(GDExtensionMaterialQualificationSessionV1) == 32);
static_assert(sizeof(GDExtensionMaterialQualificationOriginV1) == 104);
static_assert(sizeof(GDExtensionMaterialQualificationTargetV1) == 392);
static_assert(sizeof(GDExtensionMaterialQualificationMappingV1) == 176);
static_assert(sizeof(GDExtensionMaterialQualificationPrepareV1) == 784);
static_assert(sizeof(GDExtensionMaterialQualificationSelectionFactsV1) == 232);
static_assert(sizeof(GDExtensionMaterialQualificationRawDepthV1) == 144);
static_assert(sizeof(GDExtensionMaterialQualificationResultMetadataV1) == 1000);
static_assert(sizeof(GDExtensionMaterialQualificationFixedOracleV1) == 512);
static_assert(sizeof(GDExtensionMaterialQualificationResultMetadataV1) + 1024 * sizeof(uint32_t) == 5096);
static_assert(MaterialQualificationState::MQ_RETAINED_RESULT_BYTES == 5096);

using NativeRejection = RendererRD::MaterialStorage::ShaderData::NativeMaterialRecipeRejection;
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_VERTEX_WRITE) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_VERTEX_WRITE_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_VERTEX_MODE) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_VERTEX_MODE_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_ALPHA) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_ALPHA_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_DISCARD) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_DISCARD_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_TIME) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_TIME_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_DEPTH_WRITE) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_DEPTH_WRITE_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_CLIP_WRITE) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_CLIP_WRITE_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_DEPTH_MODE) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_DEPTH_MODE_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_FRAMEBUFFER_READ) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_FRAMEBUFFER_READ_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_STENCIL) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_STENCIL_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_WIREFRAME) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_WIREFRAME_V1);
static_assert(uint32_t(NativeRejection::MATERIAL_RECIPE_CULL_SCOPE) == GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_CULL_SCOPE_V1);
static_assert(GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_KNOWN_MASK_V1 == 0x00000fffu);

namespace {

using RdUnlock = MutexScopeUnlock<Mutex>;

bool service_budget_active(const GDExtensionProductionServiceBudgetV1 &p_budget) {
	return p_budget.controls && p_budget.receipts && p_budget.passes && p_budget.deadline_micros && OS::get_singleton() && OS::get_singleton()->get_ticks_usec() < p_budget.deadline_micros;
}

GDExtensionDeviceSessionResultV1 mq_result(Error p_error, uint32_t p_reason = 0) {
	return { int32_t(p_error), p_error == OK ? 0u : p_reason, 0, 0 };
}

bool mq_ranges_overlap(const void *p_a, size_t p_a_bytes, const void *p_b, size_t p_b_bytes) {
	if (!p_a || !p_b || !p_a_bytes || !p_b_bytes) {
		return false;
	}
	const uintptr_t a0 = reinterpret_cast<uintptr_t>(p_a);
	const uintptr_t b0 = reinterpret_cast<uintptr_t>(p_b);
	if (a0 > UINTPTR_MAX - p_a_bytes || b0 > UINTPTR_MAX - p_b_bytes) {
		return true;
	}
	return a0 < b0 + p_b_bytes && b0 < a0 + p_a_bytes;
}

bool mq_text_valid(const GDExtensionMaterialQualificationTextV1 &p_text, uint32_t p_payload_bytes) {
	if (p_text.reserved != 0) {
		return false;
	}
	if (p_text.present == 0) {
		return p_text.offset == 0 && p_text.bytes == 0;
	}
	if (p_text.present != 1) {
		return false;
	}
	return p_text.offset <= p_payload_bytes && p_text.bytes <= p_payload_bytes - p_text.offset;
}

GDExtensionMaterialQualificationDigestV1 mq_sha256_domain(const char *p_domain, const void *p_data, size_t p_bytes) {
	GDExtensionMaterialQualificationDigestV1 out = {};
	CryptoCore::SHA256Context ctx;
	ctx.start();
	if (p_domain) {
		const size_t domain_len = std::strlen(p_domain) + 1;
		ctx.update(reinterpret_cast<const uint8_t *>(p_domain), domain_len);
	}
	if (p_data && p_bytes) {
		ctx.update(reinterpret_cast<const uint8_t *>(p_data), p_bytes);
	}
	ctx.finish(out.bytes);
	return out;
}

void mq_populate_fixed_oracle(GDExtensionMaterialQualificationFixedOracleV1 &r_oracle, bool transparent) {
	r_oracle = {};
	r_oracle.abi = { 1, sizeof(r_oracle), 0 };
	r_oracle.recipe_version = 1;
	r_oracle.verifier_version = 1;
	r_oracle.vector_count = 1;
	r_oracle.obligation_count = 1;
	r_oracle.raw_words = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_WORDS_V1;
	r_oracle.raw_bytes = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
	r_oracle.metadata_bytes = sizeof(GDExtensionMaterialQualificationResultMetadataV1);
	r_oracle.raw_word_offset = sizeof(GDExtensionMaterialQualificationResultMetadataV1);
	r_oracle.clear_bits = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_CLEAR_BITS_V1;
	r_oracle.middle_bits = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_MID_BITS_V1;
	r_oracle.width = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_WIDTH_V1;
	r_oracle.height = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_HEIGHT_V1;
	for (uint32_t y = 0; y < 32; ++y) {
		r_oracle.interior_mask[y] = (!transparent && y >= 9 && y <= 22) ? 0x007ffe00u : 0x00000000u;
		r_oracle.exterior_mask[y] = (transparent || y <= 7 || y >= 24) ? 0xffffffffu : 0xff0000ffu;
	}

	const float fixture_quad_desc[18] = {
		-0.5f, -0.5f, 0.0f,
		-0.5f, 0.5f, 0.0f,
		0.5f, 0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		0.0f, 2.0f, 1.0f, 0.0f, 3.0f, 2.0f
	};
	const float fixed_camera_desc[8] = {
		2.0f, 1.0f, 3.0f, 0.0f, 0.0f, 0.0f, 32.0f, 32.0f
	};
	float fixed_instance192[48] = {};
	fixed_instance192[0] = fixed_instance192[5] = fixed_instance192[10] = 1.0f;
	fixed_instance192[11] = -2.0f;
	fixed_instance192[12] = fixed_instance192[17] = fixed_instance192[22] = 1.0f;
	fixed_instance192[24] = fixed_instance192[29] = fixed_instance192[34] = 1.0f;
	fixed_instance192[35] = 2.0f;
	fixed_instance192[36] = fixed_instance192[37] = fixed_instance192[38] = fixed_instance192[39] = 1.0f;

	r_oracle.fixture_geometry = mq_sha256_domain("ange.mq.fixture_geometry.v1", fixture_quad_desc, sizeof(fixture_quad_desc));
	r_oracle.camera = mq_sha256_domain("ange.mq.fixed_camera.v1", fixed_camera_desc, sizeof(fixed_camera_desc));
	r_oracle.instances = mq_sha256_domain("ange.mq.fixed_instance192.v1", fixed_instance192, sizeof(fixed_instance192));

	uint32_t masks[64] = {};
	std::memcpy(&masks[0], r_oracle.interior_mask, sizeof(r_oracle.interior_mask));
	std::memcpy(&masks[32], r_oracle.exterior_mask, sizeof(r_oracle.exterior_mask));
	r_oracle.verifier = mq_sha256_domain("ange.mq.verifier_masks.v1", masks, sizeof(masks));

	uint8_t vec_bytes[96] = {};
	std::memcpy(&vec_bytes[0], r_oracle.fixture_geometry.bytes, 32);
	std::memcpy(&vec_bytes[32], r_oracle.camera.bytes, 32);
	std::memcpy(&vec_bytes[64], r_oracle.instances.bytes, 32);
	r_oracle.vector_table = mq_sha256_domain("ange.mq.vector_table.v1", vec_bytes, sizeof(vec_bytes));

	uint8_t recipe_bytes[128] = {};
	std::memcpy(&recipe_bytes[0], vec_bytes, 96);
	std::memcpy(&recipe_bytes[96], r_oracle.verifier.bytes, 32);
	const auto recipe_domain = transparent ? "ange.mq.recipe_current_reset_f32_world_transparent.v1" :
			"ange.mq.recipe_current_reset_f32_world_opaque.v1";
	r_oracle.recipe = mq_sha256_domain(recipe_domain, recipe_bytes, sizeof(recipe_bytes));
}

bool mq_valid_transparent_pipeline(const RenderingDevice::PipelineColorBlendState &blend,
		const RenderingDevice::PipelineDepthStencilState &depth) {
	using RD = RenderingDevice;
	if (blend.enable_logic_op || blend.attachments.size() != 1 || !depth.enable_depth_test || depth.enable_depth_write ||
			depth.enable_depth_range || depth.enable_stencil || depth.depth_compare_operator != RD::COMPARE_OP_GREATER_OR_EQUAL) {
		return false;
	}
	const auto &attachment = blend.attachments[0];
	if (!attachment.enable_blend || !attachment.write_r || !attachment.write_g || !attachment.write_b || !attachment.write_a ||
			attachment.src_color_blend_factor != RD::BLEND_FACTOR_SRC_ALPHA ||
			attachment.color_blend_op != RD::BLEND_OP_ADD || attachment.alpha_blend_op != RD::BLEND_OP_ADD) {
		return false;
	}
	if (attachment.dst_color_blend_factor == RD::BLEND_FACTOR_ONE) {
		return attachment.src_alpha_blend_factor == RD::BLEND_FACTOR_SRC_ALPHA && attachment.dst_alpha_blend_factor == RD::BLEND_FACTOR_ONE;
	}
	return attachment.dst_color_blend_factor == RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA &&
			attachment.src_alpha_blend_factor == RD::BLEND_FACTOR_ONE &&
			attachment.dst_alpha_blend_factor == RD::BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
}

bool mq_budget_has_step(const GDExtensionProductionServiceBudgetV1 &p_budget, const ProductionServiceUsage &p_usage) {
	if (!p_budget.controls || !p_budget.passes || !p_budget.receipts || !p_budget.deadline_micros) {
		return false;
	}
	if (OS::get_singleton()->get_ticks_usec() >= p_budget.deadline_micros) {
		return false;
	}
	return p_usage.passes < MIN(p_budget.passes, 64u) && p_usage.receipts < MIN(p_budget.receipts, 128u);
}

RendererSceneRenderRD *mq_get_scene_render_rd() {
	return RendererSceneRenderRD::get_singleton();
}

Error mq_create_fixture_quad_surface(uint64_t p_candidate_format, RenderingServerTypes::SurfaceData &r_sd) {
	auto *rs = RenderingServer::get_singleton();
	if (!rs) {
		return ERR_UNAVAILABLE;
	}
	Array arrays;
	arrays.resize(RSE::ARRAY_MAX);

	Vector<Vector3> verts;
	verts.resize(4);
	verts.write[0] = Vector3(-0.5f, -0.5f, 0.0f);
	verts.write[1] = Vector3(-0.5f, 0.5f, 0.0f);
	verts.write[2] = Vector3(0.5f, 0.5f, 0.0f);
	verts.write[3] = Vector3(0.5f, -0.5f, 0.0f);
	arrays[RSE::ARRAY_VERTEX] = verts;

	Vector<int32_t> indices;
	indices.resize(6);
	indices.write[0] = 0;
	indices.write[1] = 2;
	indices.write[2] = 1;
	indices.write[3] = 0;
	indices.write[4] = 3;
	indices.write[5] = 2;
	arrays[RSE::ARRAY_INDEX] = indices;

	if (p_candidate_format & RSE::ARRAY_FORMAT_NORMAL) {
		Vector<Vector3> normals;
		normals.resize(4);
		for (int i = 0; i < 4; ++i) {
			normals.write[i] = Vector3(0.0f, 0.0f, 1.0f);
		}
		arrays[RSE::ARRAY_NORMAL] = normals;
	}
	if (p_candidate_format & RSE::ARRAY_FORMAT_TANGENT) {
		Vector<float> tangents;
		tangents.resize(16);
		for (int i = 0; i < 4; ++i) {
			tangents.write[i * 4 + 0] = 1.0f;
			tangents.write[i * 4 + 1] = 0.0f;
			tangents.write[i * 4 + 2] = 0.0f;
			tangents.write[i * 4 + 3] = 1.0f;
		}
		arrays[RSE::ARRAY_TANGENT] = tangents;
	}
	if (p_candidate_format & RSE::ARRAY_FORMAT_COLOR) {
		Vector<Color> colors;
		colors.resize(4);
		for (int i = 0; i < 4; ++i) {
			colors.write[i] = Color(1.0f, 1.0f, 1.0f, 1.0f);
		}
		arrays[RSE::ARRAY_COLOR] = colors;
	}
	if (p_candidate_format & RSE::ARRAY_FORMAT_TEX_UV) {
		Vector<Vector2> uv;
		uv.resize(4);
		uv.write[0] = Vector2(0.0f, 1.0f);
		uv.write[1] = Vector2(0.0f, 0.0f);
		uv.write[2] = Vector2(1.0f, 0.0f);
		uv.write[3] = Vector2(1.0f, 1.0f);
		arrays[RSE::ARRAY_TEX_UV] = uv;
	}
	if (p_candidate_format & RSE::ARRAY_FORMAT_TEX_UV2) {
		Vector<Vector2> uv2;
		uv2.resize(4);
		uv2.write[0] = Vector2(0.0f, 1.0f);
		uv2.write[1] = Vector2(0.0f, 0.0f);
		uv2.write[2] = Vector2(1.0f, 0.0f);
		uv2.write[3] = Vector2(1.0f, 1.0f);
		arrays[RSE::ARRAY_TEX_UV2] = uv2;
	}

	const uint64_t compress_flags = p_candidate_format & RSE::ARRAY_FLAG_COMPRESS_ATTRIBUTES;
	return rs->mesh_create_surface_data_from_arrays(&r_sd, RSE::PRIMITIVE_TRIANGLES, arrays, Array(), Dictionary(), compress_flags);
}

} // namespace

bool Registry::production_material_qualification_has_rt_custody(const MaterialQualificationState &mq) const {
	if (mq.candidate_mesh_lease.is_active() || mq.candidate_material_lease.is_active() || mq.roster.is_active() || mq.derived_surface.is_active()) {
		return true;
	}
	if (mq.fixture_mesh_rid.is_valid() || mq.color_tex.is_valid() || mq.depth_tex.is_valid() || mq.framebuffer_rid.is_valid() || mq.pipeline_rid.is_valid() || mq.depth_readback_ssbo.is_valid()) {
		return true;
	}
	for (uint32_t i = 0; i < MaterialQualificationState::MAX_OWNED_BUFFERS; ++i) {
		if (mq.owned_buffers[i].is_valid()) {
			return true;
		}
	}
	for (int i = 0; i < 3; ++i) {
		if (mq.uniform_sets[i].is_valid()) {
			return true;
		}
	}
	if (mq.framebuffer_physical.event_capacity_reserved || (mq.framebuffer_physical.enqueued && !mq.framebuffer_physical.free_event)) {
		return true;
	}
	for (uint32_t i = 0; i < mq.disposal_use_count; ++i) {
		const uint8_t st = mq.disposal_uses[i].state.load(std::memory_order_acquire);
		if (st == 1 || st == 2) {
			return true;
		}
	}
	if (mq.readback_state.issued && mq.readback_state.returned != mq.readback_state.issued) {
		return true;
	}
	return false;
}

void Registry::production_material_qualification_destroy_empty(MaterialQualificationState *&mq) {
	if (!mq) {
		return;
	}
	CRASH_COND(production_material_qualification_has_rt_custody(*mq));
	mq->~MaterialQualificationState();
	new (mq) MaterialQualificationState();
	mq = nullptr;
}

bool Registry::production_material_qualification_owns_framebuffer_physical(const ProductionActionCell &action, const QualificationPhysicalReturn *physical) const {
	return physical && action.material_qualification && &action.material_qualification->framebuffer_physical == physical;
}

bool Registry::production_material_qualification_owns_readback_capture(const ProductionActionCell &action, const BufferReadbackSourceCapture &capture) const {
	return action.material_qualification && &action.material_qualification->readback_capture == &capture && capture.state == &action.material_qualification->readback_state;
}

bool Registry::production_material_qualification_needs_in_place_maintenance_locked(const ProductionRequest &request) const {
	for (uint32_t i = 0; i < request.action_count; ++i) {
		const auto *mq = request.actions[i].material_qualification;
		if (!mq) {
			continue;
		}
		if (mq->readback_state.issued && mq->readback_state.returned != mq->readback_state.issued) {
			return true;
		}
	}
	return false;
}

bool Registry::production_material_qualification_needs_maintenance_locked(const ProductionRequest &request) const {
	if (production_material_qualification_needs_in_place_maintenance_locked(request)) {
		return true;
	}
	for (uint32_t i = 0; i < request.action_count; ++i) {
		const auto *mq = request.actions[i].material_qualification;
		if (!mq) {
			continue;
		}
		if (mq->framebuffer_physical.enqueued && !mq->framebuffer_physical.free_event) {
			return true;
		}
		for (uint32_t u = 0; u < mq->disposal_use_count; ++u) {
			const uint8_t st = mq->disposal_uses[u].state.load(std::memory_order_acquire);
			if (st == 2) {
				return true;
			}
		}
	}
	return false;
}

bool Registry::production_request_is_pure_mq_in_place_locked(const ProductionRequest &request) const {
	if (!request.action_count || request.resource_count != 0) {
		return false;
	}
	for (uint32_t i = 0; i < request.action_count; ++i) {
		if (request.actions[i].declaration.action_kind != GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1) {
			return false;
		}
	}
	return true;
}

void Registry::production_material_qualification_report_locked(const ProductionActionCell &action, GDExtensionProductionReportV2 &report, Error &cause) const {
	const auto *mq = action.material_qualification;
	if (!mq) {
		return;
	}
	if (mq->recipe_failed && cause == OK) {
		cause = mq->recipe_error != OK ? mq->recipe_error : ERR_CANT_CREATE;
	}
	if (mq->recorded) {
		if (mq->readback_state.error != OK && cause == OK) {
			cause = mq->readback_state.error;
		}
		if (!mq->readback_state.issued || mq->readback_state.returned != mq->readback_state.issued || !mq->readback_state.delivered) {
			++report.possible_obligations;
		} else if (mq->readback_state.completed == GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1) {
			++report.ready_readbacks;
		}
	}
	if (mq->retiring_started) {
		if (mq->framebuffer_physical.enqueued && !mq->framebuffer_physical.free_event) {
			++report.physical_returns_pending;
		}
		for (uint32_t u = 0; u < mq->disposal_use_count; ++u) {
			const uint8_t st = mq->disposal_uses[u].state.load(std::memory_order_acquire);
			if (st == 1 || st == 2) {
				++report.physical_returns_pending;
			}
		}
	}
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_material_qualification(GDExtensionProductionRequestRefV1 ref, const GDExtensionMaterialQualificationPrepareV1 *prepare, const uint8_t *payload, uint32_t payload_bytes) {
	if (!prepare || uintptr_t(prepare) % alignof(GDExtensionMaterialQualificationPrepareV1) != 0) {
		return mq_result(ERR_INVALID_PARAMETER, 1);
	}
	if (prepare->abi.abi_version != 1 || prepare->abi.struct_size != sizeof(GDExtensionMaterialQualificationPrepareV1) || prepare->abi.reserved != 0 || prepare->reserved != 0) {
		return mq_result(ERR_INVALID_PARAMETER, 1);
	}
	if ((payload_bytes > 0 && !payload) || prepare->payload_bytes != payload_bytes || mq_ranges_overlap(prepare, sizeof(*prepare), payload, payload_bytes)) {
		return mq_result(ERR_INVALID_PARAMETER, 1);
	}
	if (prepare->target.reserved != 0 || prepare->mapping.reserved != 0 ||
			prepare->target.profile_canonical_offset > payload_bytes ||
			prepare->target.profile_canonical_bytes > payload_bytes - prepare->target.profile_canonical_offset) {
		return mq_result(ERR_INVALID_PARAMETER, 2);
	}
	const GDExtensionMaterialQualificationTextV1 *texts[] = {
		&prepare->target.driver_name,
		&prepare->target.rendering_method,
		&prepare->target.device_vendor_name,
		&prepare->target.device_name,
		&prepare->target.api_version,
		&prepare->target.pipeline_cache_uuid,
		&prepare->target.os_name,
		&prepare->target.os_version,
		&prepare->mapping.renderer_contract,
		&prepare->mapping.source_renderer_contract,
	};
	for (const auto *t : texts) {
		if (!mq_text_valid(*t, payload_bytes)) {
			return mq_result(ERR_INVALID_PARAMETER, 2);
		}
	}
	if (prepare->candidate_mesh_present != 1 || prepare->candidate_mesh_rid == 0 || prepare->material_rid == 0 ||
			(prepare->recipe != GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_OPAQUE_V1 &&
					prepare->recipe != GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_TRANSPARENT_V1)) {
		return mq_result(ERR_INVALID_DATA, 3);
	}
	if (prepare->generation_mode == GDEXTENSION_MATERIAL_QUALIFICATION_READ_CURRENT_V1) {
		if (prepare->expected_material_generation != 0) {
			return mq_result(ERR_INVALID_DATA, 3);
		}
	} else if (prepare->generation_mode == GDEXTENSION_MATERIAL_QUALIFICATION_EXACT_V1) {
		if (prepare->expected_material_generation == 0 || prepare->expected_material_generation == UINT64_MAX) {
			return mq_result(ERR_INVALID_DATA, 3);
		}
	} else {
		return mq_result(ERR_INVALID_DATA, 3);
	}

	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	const Error err = production_request_locked(ref, true, r, rd);
	if (err != OK) {
		return mq_result(err, 16);
	}
	if (r->contract_version != 2 || r->frozen || r->capture_inflight || prepare->action_ordinal >= r->action_count) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	auto &action = r->actions[prepare->action_ordinal];
	if (action.declaration.action_kind != GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	if (action.material_qualification) {
		const auto &existing = *action.material_qualification;
		const uint8_t *existing_payload = r->attachment->payload + existing.payload_offset;
		const bool same = std::memcmp(&existing.prepare, prepare, sizeof(*prepare)) == 0 &&
				existing.payload_bytes == payload_bytes &&
				(payload_bytes == 0 || std::memcmp(existing_payload, payload, payload_bytes) == 0);
		return mq_result(same ? OK : ERR_ALREADY_EXISTS, same ? 0 : 5);
	}
	const uint64_t needed = uint64_t(MaterialQualificationState::MQ_RETAINED_RESULT_BYTES) + uint64_t(payload_bytes);
	if (needed > r->capacity.retained_payload_bytes - r->payload_used) {
		return mq_result(ERR_OUT_OF_MEMORY, 19);
	}
	if (!r->material_qualifications || prepare->action_ordinal >= r->capacity.suboperations) {
		return mq_result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	if (production_serials.exhausted || production_serials.next > UINT64_MAX - 2 || r->revision == UINT64_MAX) {
		return mq_result(ERR_PARAMETER_RANGE_ERROR, 19);
	}

	MaterialQualificationState *mq = &r->material_qualifications[prepare->action_ordinal];
	mq->~MaterialQualificationState();
	new (mq) MaterialQualificationState();
	mq->prepare = *prepare;
	mq->result_payload_offset = r->payload_offset + r->payload_used;
	mq->payload_offset = mq->result_payload_offset + MaterialQualificationState::MQ_RETAINED_RESULT_BYTES;
	mq->payload_bytes = payload_bytes;
	std::memset(r->attachment->payload + mq->result_payload_offset, 0, MaterialQualificationState::MQ_RETAINED_RESULT_BYTES);
	if (payload_bytes > 0) {
		std::memcpy(r->attachment->payload + mq->payload_offset, payload, payload_bytes);
	}
	r->payload_used += needed;
	action.payload_offset = mq->payload_offset;
	action.payload_bytes = payload_bytes;

	mq->selection.abi = { 1, sizeof(mq->selection), 0 };
	mq->selection.action_ordinal = prepare->action_ordinal;
	mq->selection.origin = prepare->origin;
	mq->selection.resource_content_version = prepare->resource_content_version;
	mq->selection.material_rid = prepare->material_rid;
	mq->selection.candidate_mesh_rid = prepare->candidate_mesh_rid;
	mq->selection.surface_index = prepare->surface_index;
	mq->selection.state = GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1;

	mq_populate_fixed_oracle(mq->oracle, prepare->recipe == GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_TRANSPARENT_V1);

	mq->framebuffer_physical.bridge = r->attachment->serial;
	production_serials.take(mq->framebuffer_physical.serial);
	mq->framebuffer_physical.child = mq->framebuffer_physical.serial;

	action.material_qualification = mq;
	++r->revision;
	return mq_result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_read_material_selection(GDExtensionProductionRequestRefV1 ref, uint64_t action_ordinal, GDExtensionMaterialQualificationSelectionFactsV1 *out_selection) {
	if (!out_selection || uintptr_t(out_selection) % alignof(GDExtensionMaterialQualificationSelectionFactsV1) != 0) {
		return mq_result(ERR_INVALID_PARAMETER, 1);
	}
	*out_selection = {};
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	const Error err = production_request_locked(ref, false, r, rd);
	if (err != OK) {
		return mq_result(err, 16);
	}
	if (action_ordinal >= r->action_count) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	const auto &action = r->actions[action_ordinal];
	if (action.declaration.action_kind != GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1 || !action.material_qualification) {
		return mq_result(ERR_UNCONFIGURED, 6);
	}
	auto snapshot = action.material_qualification->selection;
	if (r->cancelled && snapshot.state == GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1) {
		snapshot.state = GDEXTENSION_MATERIAL_QUALIFICATION_CANCELLED_V1;
		snapshot.native_error = int32_t(ERR_UNAVAILABLE);
	}
	*out_selection = snapshot;
	return mq_result(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_material_recipe(GDExtensionProductionRequestRefV1 ref, uint64_t action_ordinal, GDExtensionMaterialQualificationFixedOracleV1 *out_oracle) {
	if (!out_oracle || uintptr_t(out_oracle) % alignof(GDExtensionMaterialQualificationFixedOracleV1) != 0) {
		return mq_result(ERR_INVALID_PARAMETER, 1);
	}
	*out_oracle = {};
	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	const Error err = production_request_locked(ref, false, r, rd);
	if (err != OK) {
		return mq_result(err, 16);
	}
	if (r->cancelled || r->release_started) {
		return mq_result(ERR_UNAVAILABLE, 16);
	}
	if (action_ordinal >= r->action_count) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	auto &action = r->actions[action_ordinal];
	if (action.declaration.action_kind != GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1 || !action.material_qualification) {
		return mq_result(ERR_UNCONFIGURED, 6);
	}
	auto &mq = *action.material_qualification;
	if (!mq.selection_done || mq.selection.state == GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1) {
		return mq_result(ERR_BUSY, 7);
	}
	if (mq.selection.state != GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1) {
		const Error sel_err = mq.selection.native_error != 0 ? Error(mq.selection.native_error) : ERR_UNAVAILABLE;
		return mq_result(sel_err, mq.selection.reason ? mq.selection.reason : 8);
	}
	if (mq.selection.source_gate_known != GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_KNOWN_MASK_V1 ||
			mq.selection.source_gate_rejections != 0 ||
			(mq.selection.compiler_known & GDEXTENSION_MATERIAL_QUALIFICATION_ZERO_DISPLACEMENT_V1) == 0 ||
			(mq.selection.compiler_values & GDEXTENSION_MATERIAL_QUALIFICATION_ZERO_DISPLACEMENT_V1) == 0 ||
			mq.selection.candidate_geometry_profile != GDEXTENSION_MATERIAL_QUALIFICATION_GEOMETRY_STATIC_FINITE_SURFACE_V1) {
		return mq_result(ERR_UNAVAILABLE, 9);
	}
	if (mq.recipe_failed) {
		return mq_result(mq.recipe_error != OK ? mq.recipe_error : ERR_CANT_CREATE, 10);
	}
	*out_oracle = mq.oracle;
	if (mq.recipe_ready) {
		return mq_result(OK);
	}
	if (r->frozen || r->capture_inflight || r->revision == UINT64_MAX) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	if (!mq.recipe_requested) {
		mq.recipe_requested = true;
		++r->revision;
	}
	return mq_result(ERR_BUSY, 11);
}

Error Registry::production_material_qualification_observe_locked(ProductionRequest &request, RenderingDevice &rd, const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	if (request.cancelled || request.release_started || request.source_device_lost || request.capture_inflight || request.continuation_inflight) {
		return OK;
	}
	if (!rd.native_immutable_is_render_thread() || rd.draw_list.active || rd.compute_list.active || rd.raytracing_list.active || rd.draw_graph.has_driver_callbacks()) {
		return ERR_BUSY;
	}

	for (uint32_t i = 0; i < request.action_count; ++i) {
		auto &action = request.actions[i];
		auto *mq = action.material_qualification;
		if (!mq) {
			continue;
		}

        const bool transparent = mq->prepare.recipe == GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_TRANSPARENT_V1;
		// Phase A: RT Selection Acquisition & Shader Feature Observation.
		if (!mq->selection_done) {
			if (!mq_budget_has_step(budget, usage)) {
				return ERR_BUSY;
			}
			const uint64_t request_serial = request.serial;
			const uint64_t control_serial = request.control.serial;
			auto *const attachment = request.attachment;
			const uint64_t epoch = attachment->epoch;
			const GDExtensionMaterialQualificationPrepareV1 prep = mq->prepare;

			++request.capture_inflight;
			rd._thread_safe_.unlock();
			unlock_mutex_for_call();

			GDExtensionMaterialQualificationSelectionFactsV1 observed = mq->selection;
			uint64_t observed_raw_format = 0;
			Error step_err = OK;

			auto *scene_render = mq_get_scene_render_rd();
			auto *materials = RendererRD::MaterialStorage::get_singleton();
			auto *meshes = RendererRD::MeshStorage::get_singleton();
			if (!scene_render || !materials || !meshes || scene_render->get_native_material_recipe_backend() == 0) {
				step_err = ERR_UNAVAILABLE;
			} else {
				step_err = scene_render->ensure_native_material_recipe_sources_ready();
				if (step_err == OK) {
					for (int b = 0; b < RendererRD::MeshStorage::DEFAULT_RD_BUFFER_MAX; ++b) {
						const RID def_buf = meshes->mesh_get_default_rd_buffer(RendererRD::MeshStorage::DefaultRDBuffer(b));
						if (def_buf.is_valid()) {
							const Error settled = rd._native_settle_transfer_worker(def_buf);
							if (settled != OK) {
								step_err = settled;
								break;
							}
						}
					}
				}
				const RID mesh_rid = RID::from_uint64(prep.candidate_mesh_rid);
				const RID mat_rid = RID::from_uint64(prep.material_rid);
				if (step_err == OK) {
					step_err = materials->material_prepare_native_qualification(mat_rid);
				}
				if (step_err == OK) {
					step_err = meshes->mesh_selection_acquire(mesh_rid, mq->candidate_mesh_lease);
				}
				uint64_t expected_mat_gen = 0;
				if (step_err == OK) {
					expected_mat_gen = prep.generation_mode == GDEXTENSION_MATERIAL_QUALIFICATION_EXACT_V1
							? prep.expected_material_generation
							: materials->material_get_generation(mat_rid);
					if (expected_mat_gen == 0 || expected_mat_gen == UINT64_MAX) {
						step_err = ERR_DOES_NOT_EXIST;
					} else {
						step_err = materials->material_selection_acquire(mat_rid, mq->candidate_material_lease, expected_mat_gen);
					}
				}
				RendererRD::MeshStorage::NativeSelectionSurfaceObservation mesh_obs;
				if (step_err == OK) {
					step_err = meshes->mesh_selection_get_surface_observation(mq->candidate_mesh_lease, prep.surface_index, mesh_obs);
				}
				if (step_err == OK) {
					observed_raw_format = mesh_obs.format;
					observed.candidate_mesh_revision = mesh_obs.mesh_revision;
					observed.candidate_primitive = uint32_t(mesh_obs.primitive);
					const uint64_t forbidden_mesh_bits = RSE::ARRAY_FORMAT_BONES | RSE::ARRAY_FORMAT_WEIGHTS |
							RSE::ARRAY_FORMAT_CUSTOM0 | RSE::ARRAY_FORMAT_CUSTOM1 | RSE::ARRAY_FORMAT_CUSTOM2 | RSE::ARRAY_FORMAT_CUSTOM3;
					const bool static_finite = meshes->mesh_get_blend_shape_count(mesh_rid) == 0 &&
							mesh_obs.vertex_count > 0 &&
							mesh_obs.primitive == RSE::PRIMITIVE_TRIANGLES &&
							(mesh_obs.format & forbidden_mesh_bits) == 0;
					observed.candidate_geometry_profile = static_finite
							? GDEXTENSION_MATERIAL_QUALIFICATION_GEOMETRY_STATIC_FINITE_SURFACE_V1
							: GDEXTENSION_MATERIAL_QUALIFICATION_GEOMETRY_UNKNOWN_V1;

					const Error warm_err = scene_render->warm_native_material_instance192_shader(mat_rid, mesh_obs.primitive, transparent);
					if (warm_err == ERR_BUSY) {
						step_err = ERR_BUSY;
					} else {
						RendererRD::MaterialStorage::NativeSelectionShaderObservation shader_obs;
						step_err = materials->material_selection_get_shader_observation(mq->candidate_material_lease, shader_obs);
						if (step_err == OK) {
							observed.material_generation = shader_obs.material_generation;
							observed.shader_version_rid = shader_obs.shader_version.get_id();
							observed.compiler_known = GDEXTENSION_MATERIAL_QUALIFICATION_NEEDS_INVERSE_V1 | GDEXTENSION_MATERIAL_QUALIFICATION_ZERO_DISPLACEMENT_V1;
							observed.compiler_values = (shader_obs.needs_inverse ? GDEXTENSION_MATERIAL_QUALIFICATION_NEEDS_INVERSE_V1 : 0u) |
									(shader_obs.zero_displacement ? GDEXTENSION_MATERIAL_QUALIFICATION_ZERO_DISPLACEMENT_V1 : 0u);
							const auto rejections = transparent ? shader_obs.transparent_recipe_rejections : shader_obs.material_recipe_rejections;
							if (rejections != UINT32_MAX) {
								observed.source_gate_known = GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_KNOWN_MASK_V1;
								observed.source_gate_rejections = rejections & GDEXTENSION_MATERIAL_QUALIFICATION_SOURCE_GATE_KNOWN_MASK_V1;
							}
							RendererRD::MaterialStorage::MaterialData *mat_data = nullptr;
							if (materials->material_get_native_preparation_data(mat_rid, shader_obs.material_generation, mat_data) == OK && mat_data) {
								RID ready_shader, ready_set;
								uint64_t mask = 0;
								if (mat_data->get_native_recipe_shader_if_ready(ready_shader, mask, ready_set, transparent) == OK) {
									observed.candidate_input_mask = mask;
									uint32_t pos_stride = 0;
									observed.candidate_vertex_format = uint64_t(meshes->_mesh_surface_generate_vertex_format(mesh_obs.format, mask, false, false, false, pos_stride));
								}
							}
						}
					}
				}
			}

			if (step_err != OK) {
				if (mq->candidate_material_lease.is_active() && materials) {
					materials->material_selection_release(mq->candidate_material_lease);
				}
				if (mq->candidate_mesh_lease.is_active() && meshes) {
					meshes->mesh_selection_release(mq->candidate_mesh_lease);
				}
			}

			rd._thread_safe_.lock();
			relock_mutex_after_call();
			CRASH_COND(!request.capture_inflight);
			--request.capture_inflight;

			if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
				return ERR_INVALID_DATA;
			}
			++usage.passes;
			++usage.receipts;

			if (step_err == ERR_BUSY) {
				return ERR_BUSY;
			}
			if (step_err == OK) {
				observed.state = GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1;
				observed.native_error = int32_t(OK);
				observed.reason = 0;
				mq->candidate_surface_raw_format = observed_raw_format;
			} else {
				observed.state = GDEXTENSION_MATERIAL_QUALIFICATION_FAILED_V1;
				observed.native_error = int32_t(step_err);
				observed.reason = 12;
			}
			mq->selection = observed;
			mq->selection_done = true;
			if (request.revision < UINT64_MAX) {
				++request.revision;
			}
		}

		// Phase B: RT Fixed-Recipe Resource & Pipeline Instantiation (100% idempotent across mid-step ERR_BUSY).
		if (mq->recipe_requested && !mq->recipe_ready && !mq->recipe_failed) {
			if (!mq_budget_has_step(budget, usage)) {
				return ERR_BUSY;
			}
			const uint64_t request_serial = request.serial;
			const uint64_t control_serial = request.control.serial;
			auto *const attachment = request.attachment;
			const uint64_t epoch = attachment->epoch;
			const uint64_t candidate_raw_format = mq->candidate_surface_raw_format;
			const uint64_t candidate_vf = mq->selection.candidate_vertex_format;
			const RID selected_mat_rid = RID::from_uint64(mq->selection.material_rid);

			++request.capture_inflight;
			rd._thread_safe_.unlock();
			unlock_mutex_for_call();

			Error recipe_err = OK;
			GDExtensionMaterialQualificationResultMetadataV1 prepared_meta = {};
			bool prepared_meta_valid = false;
			auto *scene_render = mq_get_scene_render_rd();
			auto *materials = RendererRD::MaterialStorage::get_singleton();
			auto *meshes = RendererRD::MeshStorage::get_singleton();

			auto attach_disposal = [&](RID p_rid) -> Error {
				if (!p_rid.is_valid()) {
					return OK;
				}
				if (mq->disposal_use_count >= MaterialQualificationState::MAX_DISPOSAL_USES) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				return rd._production_disposal_attach_owned(mq->disposal_uses[mq->disposal_use_count++], request, p_rid);
			};

			if (!scene_render || !materials || !meshes || scene_render->get_native_material_recipe_backend() == 0) {
				recipe_err = ERR_UNAVAILABLE;
			} else {
				recipe_err = scene_render->ensure_native_material_recipe_sources_ready();
				if (recipe_err == OK) {
					recipe_err = materials->material_prepare_native_qualification(selected_mat_rid);
				}
				if (recipe_err == OK) {
					recipe_err = materials->material_settle_native_uploads(selected_mat_rid);
				}
			}

			// 1. Create 4-vertex 6-index Z=0 quad fixture mesh matching candidate format & settle its buffers idempotently.
			if (recipe_err == OK && mq->fixture_mesh_rid.is_null()) {
				RenderingServerTypes::SurfaceData sd;
				recipe_err = mq_create_fixture_quad_surface(candidate_raw_format, sd);
				if (recipe_err == OK) {
					mq->fixture_mesh_rid = meshes->mesh_allocate();
					if (mq->fixture_mesh_rid.is_null()) {
						recipe_err = ERR_OUT_OF_MEMORY;
					} else {
						meshes->mesh_initialize(mq->fixture_mesh_rid);
						meshes->mesh_add_surface(mq->fixture_mesh_rid, sd);
					}
				}
			}
			if (recipe_err == OK && mq->fixture_mesh_rid.is_valid() && !mq->fixture_mesh_settled) {
				auto *mesh_ptr = meshes->mesh_owner.get_or_null(mq->fixture_mesh_rid);
				auto *surf_ptr = (mesh_ptr && mesh_ptr->surface_count == 1) ? mesh_ptr->surfaces[0] : nullptr;
				if (!surf_ptr) {
					recipe_err = ERR_CANT_CREATE;
				} else {
					const RID surf_bufs[5] = {
						surf_ptr->vertex_buffer,
						surf_ptr->attribute_buffer,
						surf_ptr->skin_buffer,
						surf_ptr->index_buffer,
						surf_ptr->blend_shape_buffer,
					};
					for (uint32_t k = 0; k < 5 && recipe_err == OK; ++k) {
						if (surf_bufs[k].is_valid() && !mq->fixture_buf_disposal_attached[k]) {
							recipe_err = attach_disposal(surf_bufs[k]);
							if (recipe_err == OK) {
								mq->fixture_buf_disposal_attached[k] = true;
							}
						}
					}
					if (recipe_err == OK && surf_ptr->uniform_set.is_valid() && !mq->fixture_uset_disposal_attached) {
						recipe_err = attach_disposal(surf_ptr->uniform_set);
						if (recipe_err == OK) {
							mq->fixture_uset_disposal_attached = true;
						}
					}
					for (uint32_t k = 0; k < 5 && recipe_err == OK; ++k) {
						if (surf_bufs[k].is_valid() && !mq->fixture_buf_settled[k]) {
							recipe_err = rd._native_settle_transfer_worker(surf_bufs[k]);
							if (recipe_err == OK) {
								mq->fixture_buf_settled[k] = true;
							}
						}
					}
					if (recipe_err == OK) {
						mq->fixture_mesh_settled = true;
					}
				}
			}

			// 2. Create 32x32 single-sample R16G16B16A16_SFLOAT + D32_SFLOAT targets and framebuffer atomically.
			RD::FramebufferFormatID fb_format = RD::INVALID_ID;
			if (recipe_err == OK) {
				if (mq->framebuffer_rid.is_null()) {
					RD::TextureFormat color_fmt;
					color_fmt.format = RD::DATA_FORMAT_R16G16B16A16_SFLOAT;
					color_fmt.width = 32;
					color_fmt.height = 32;
					color_fmt.depth = 1;
					color_fmt.array_layers = 1;
					color_fmt.mipmaps = 1;
					color_fmt.texture_type = RD::TEXTURE_TYPE_2D;
					color_fmt.samples = RD::TEXTURE_SAMPLES_1;
					color_fmt.usage_bits = RD::TEXTURE_USAGE_COLOR_ATTACHMENT_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;
					if (transparent) { color_fmt.usage_bits |= RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT; }

					RD::TextureFormat depth_fmt;
					depth_fmt.format = RD::DATA_FORMAT_D32_SFLOAT;
					depth_fmt.width = 32;
					depth_fmt.height = 32;
					depth_fmt.depth = 1;
					depth_fmt.array_layers = 1;
					depth_fmt.mipmaps = 1;
					depth_fmt.texture_type = RD::TEXTURE_TYPE_2D;
					depth_fmt.samples = RD::TEXTURE_SAMPLES_1;
					depth_fmt.usage_bits = RD::TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | RD::TEXTURE_USAGE_CAN_COPY_FROM_BIT | RD::TEXTURE_USAGE_SAMPLING_BIT;

					recipe_err = rd._production_mq_create_targets_and_framebuffer(color_fmt, depth_fmt, RD::INVALID_ID, mq->framebuffer_physical, mq->color_tex, mq->depth_tex, mq->framebuffer_rid);
				}
				if (mq->color_tex.is_valid() && !mq->color_tex_disposal_attached) {
					const Error att = attach_disposal(mq->color_tex);
					if (att == OK) {
						mq->color_tex_disposal_attached = true;
					} else if (recipe_err == OK) {
						recipe_err = att;
					}
				}
				if (mq->depth_tex.is_valid() && !mq->depth_tex_disposal_attached) {
					const Error att = attach_disposal(mq->depth_tex);
					if (att == OK) {
						mq->depth_tex_disposal_attached = true;
					} else if (recipe_err == OK) {
						recipe_err = att;
					}
				}
				if (recipe_err == OK) {
					fb_format = rd.framebuffer_get_format(mq->framebuffer_rid);
					MutexLock rd_lock(rd._thread_safe_);
					if (const auto *tex_ptr = rd.texture_owner.get_or_null(mq->depth_tex)) {
						mq->depth_backing_serial = tex_ptr->native_disposal_serial ? tex_ptr->native_disposal_serial : mq->depth_tex.get_id();
						mq->depth_allocation_generation = tex_ptr->driver_id.id ? tex_ptr->driver_id.id : 1;
					}
				}
			}

			// 3. Plan native material recipe via active backend (Mobile / Forward+).
			if (recipe_err == OK && !mq->roster.is_active()) {
				recipe_err = scene_render->plan_native_material_recipe(selected_mat_rid, mq->fixture_mesh_rid, 0, fb_format, mq->roster, 256, transparent);
			}

			// 4. Derive vertex/index arrays from retained fixture surface and verify exact candidate VF match.
			if (recipe_err == OK && !mq->derived_surface.is_active()) {
				recipe_err = RendererRD::MeshStorage::mesh_surface_hold_derive(mq->roster.surface, mq->roster.material.get_input_mask(), false, false, 0, mq->derived_surface);
			}
			if (recipe_err == OK) {
				mq->vertex_array_rid = mq->derived_surface.get_vertex_array();
				mq->index_array_rid = mq->derived_surface.get_index_array();
				if (mq->vertex_array_rid.is_null() || mq->index_array_rid.is_null() || uint64_t(mq->derived_surface.get_vertex_format()) != candidate_vf) {
					recipe_err = ERR_UNAVAILABLE;
				}
			}

			// 5. Instantiate owned fixed UBOs/SSBOs, readback SSBO, uniform sets 0..2, and render pipeline idempotently.
			if (recipe_err == OK) {
				static constexpr uint32_t MAX_ROSTER_SYMBOLS = 256;
				RID symbol_rids[MAX_ROSTER_SYMBOLS];
				if (uint32_t(mq->roster.symbols.size()) > MAX_ROSTER_SYMBOLS) {
					recipe_err = ERR_PARAMETER_RANGE_ERROR;
				}
				uint32_t buf_idx = 0;
				for (uint32_t s = 0; s < uint32_t(mq->roster.owned.size()) && recipe_err == OK; ++s) {
					const auto &item = mq->roster.owned[int(s)];
					if (item.symbol >= uint32_t(mq->roster.symbols.size())) {
						recipe_err = ERR_INVALID_DATA;
						break;
					}
					const auto sym_kind = mq->roster.symbols[int(item.symbol)].kind;
					if (sym_kind != RD::NativeProspectiveSymbol::FUTURE_UNIFORM_BUFFER && sym_kind != RD::NativeProspectiveSymbol::FUTURE_STORAGE_BUFFER) {
						continue;
					}
					if (buf_idx >= MaterialQualificationState::MAX_OWNED_BUFFERS) {
						recipe_err = ERR_PARAMETER_RANGE_ERROR;
						break;
					}
					const uint32_t slot = buf_idx++;
					if (mq->owned_buffers[slot].is_null()) {
						if (sym_kind == RD::NativeProspectiveSymbol::FUTURE_UNIFORM_BUFFER) {
							mq->owned_buffers[slot] = rd.uniform_buffer_create(item.bytes.size(), item.bytes);
						} else {
							mq->owned_buffers[slot] = rd.storage_buffer_create(item.bytes.size(), item.bytes);
						}
						if (mq->owned_buffers[slot].is_null()) {
							recipe_err = ERR_CANT_CREATE;
							break;
						}
						if (slot + 1 > mq->owned_buffer_count) {
							mq->owned_buffer_count = slot + 1;
						}
					}
					symbol_rids[item.symbol] = mq->owned_buffers[slot];
					if (!mq->owned_buffer_disposal_attached[slot]) {
						recipe_err = attach_disposal(mq->owned_buffers[slot]);
						if (recipe_err != OK) {
							break;
						}
						mq->owned_buffer_disposal_attached[slot] = true;
					}
					if (!mq->owned_buffer_settled[slot]) {
						recipe_err = rd._native_settle_transfer_worker(mq->owned_buffers[slot]);
						if (recipe_err != OK) {
							break;
						}
						mq->owned_buffer_settled[slot] = true;
					}
				}

				if (recipe_err == OK) {
					if (mq->depth_readback_ssbo.is_null()) {
						mq->depth_readback_ssbo = rd.storage_buffer_create(GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1);
						if (mq->depth_readback_ssbo.is_null()) {
							recipe_err = ERR_CANT_CREATE;
						}
					}
					if (recipe_err == OK && !mq->readback_ssbo_disposal_attached) {
						recipe_err = attach_disposal(mq->depth_readback_ssbo);
						if (recipe_err == OK) {
							mq->readback_ssbo_disposal_attached = true;
						}
					}
					if (recipe_err == OK && !mq->readback_prepared) {
						recipe_err = rd._production_mq_prepare_readback_buffer(mq->depth_readback_ssbo);
						if (recipe_err == OK) {
							mq->readback_prepared = true;
						}
					}
				}

				if (recipe_err == OK) {
					mq->native_shader_rid = mq->roster.material.get_plan().material.snapshot.shader;
					mq->material_uniform_set = mq->roster.material.get_plan().material.snapshot.uniform_set;
					if (mq->roster.push.size() != 16 || mq->native_shader_rid.is_null()) {
						recipe_err = ERR_INVALID_DATA;
					} else {
						std::memcpy(mq->push_constant, mq->roster.push.ptr(), 16);
					}
				}

				if (recipe_err == OK) {
					static constexpr uint32_t MAX_SET_UNIFORMS = 64;
					for (uint32_t set_idx = 0; set_idx < 3 && recipe_err == OK; ++set_idx) {
						if (mq->uniform_sets[set_idx].is_null()) {
							RD::Uniform set_uniforms[MAX_SET_UNIFORMS];
							uint32_t set_uniform_count = 0;
							for (int b = 0; b < mq->roster.bindings.size(); ++b) {
								const auto &binding = mq->roster.bindings[b];
								if (binding.set != set_idx) {
									continue;
								}
								if (set_uniform_count >= MAX_SET_UNIFORMS) {
									recipe_err = ERR_PARAMETER_RANGE_ERROR;
									break;
								}
								RD::Uniform &u = set_uniforms[set_uniform_count++];
								u = RD::Uniform();
								u.uniform_type = binding.type;
								u.binding = binding.binding;
								u.immutable_sampler = binding.immutable_sampler;
								for (int k = 0; k < binding.inputs.size(); ++k) {
									const auto &key = binding.inputs[k];
									const RID resolved = key.actual.is_valid() ? key.actual : (key.future < mq->roster.symbols.size() ? symbol_rids[key.future] : RID());
									if (resolved.is_null()) {
										recipe_err = ERR_INVALID_DATA;
										break;
									}
									u.append_id(resolved);
								}
								if (recipe_err != OK) {
									break;
								}
							}
							if (recipe_err != OK) {
								break;
							}
							if (set_uniform_count > 0) {
								mq->uniform_sets[set_idx] = rd.uniform_set_create(VectorView<RD::Uniform>(set_uniforms, set_uniform_count), mq->native_shader_rid, set_idx);
								if (mq->uniform_sets[set_idx].is_null()) {
									recipe_err = ERR_CANT_CREATE;
									break;
								}
							}
						}
						if (mq->uniform_sets[set_idx].is_valid() && !mq->uniform_set_disposal_attached[set_idx]) {
							recipe_err = attach_disposal(mq->uniform_sets[set_idx]);
							if (recipe_err == OK) {
								mq->uniform_set_disposal_attached[set_idx] = true;
							}
						}
					}
				}

				if (recipe_err == OK && (mq->roster.transparent_pass != transparent ||
						(transparent && !mq_valid_transparent_pipeline(mq->roster.blend, mq->roster.depth_stencil)))) {
					recipe_err = ERR_UNAVAILABLE;
				}
				if (recipe_err == OK) {
					if (mq->pipeline_rid.is_null()) {
						mq->pipeline_rid = rd.render_pipeline_create(
								mq->native_shader_rid,
								fb_format,
								mq->derived_surface.get_vertex_format(),
								mq->roster.primitive,
								mq->roster.raster,
								mq->roster.multisample,
								mq->roster.depth_stencil,
								mq->roster.blend,
								0,
								0,
								mq->roster.specialization);
						if (mq->pipeline_rid.is_null()) {
							recipe_err = ERR_CANT_CREATE;
						}
					}
					if (recipe_err == OK && !mq->pipeline_disposal_attached) {
						recipe_err = attach_disposal(mq->pipeline_rid);
						if (recipe_err == OK) {
							mq->pipeline_disposal_attached = true;
						}
					}
				}

				// Populate static ResultMetadataV1 fields & provenance digests into retained payload.
				if (recipe_err == OK) {
					static constexpr uint32_t MAX_BORROWED_DEPS = 256;
					if (uint32_t(mq->roster.borrowed.size()) > MAX_BORROWED_DEPS) {
						recipe_err = ERR_PARAMETER_RANGE_ERROR;
					} else {
						GDExtensionMaterialQualificationResultMetadataV1 &meta = prepared_meta;
						meta = {};
						meta.abi = { 1, sizeof(meta), 0 };
						meta.action_ordinal = mq->prepare.action_ordinal;
						meta.origin = mq->prepare.origin;
						meta.selection = mq->selection;
						meta.recipe = mq->oracle.recipe;
						meta.verifier = mq->oracle.verifier;
						meta.vector_table = mq->oracle.vector_table;
						meta.recipe_version = 1;
						meta.verifier_version = 1;
						meta.vector_ordinal = 0;
						meta.vector_count = 1;
						meta.obligation_ordinal = 0;
						meta.obligation_count = 1;
						meta.state = GDEXTENSION_MATERIAL_QUALIFICATION_PENDING_V1;
						meta.precision = 1;
						meta.fixture_mesh_rid = mq->fixture_mesh_rid.get_id();
						meta.fixture_mesh_revision = mq->roster.mesh.get_revision();
						meta.fixture_surface_generation = mq->derived_surface.current_generation;
						meta.fixture_surface_index = 0;
						meta.fixture_primitive = uint32_t(mq->derived_surface.get_primitive());
						meta.fixture_vertex_format = uint64_t(mq->derived_surface.get_vertex_format());
						meta.fixture_input_mask = mq->roster.material.get_input_mask();
						meta.fixture_geometry = mq->oracle.fixture_geometry;
						meta.fixed_camera = mq->oracle.camera;
						meta.fixed_instance_records = mq->oracle.instances;

						const uint32_t backend_id = scene_render->get_native_material_recipe_backend();
						meta.renderer_variant = mq_sha256_domain("ange.mq.renderer_variant.v1", &backend_id, sizeof(backend_id));

						struct PipelineKeyBytes {
							uint64_t fb_format;
							uint64_t vertex_format;
							uint32_t primitive;
							uint32_t cull_mode;
							uint32_t depth_compare;
							uint32_t depth_write;
						} pkey = {
							uint64_t(fb_format),
							uint64_t(mq->derived_surface.get_vertex_format()),
							uint32_t(mq->roster.primitive),
							uint32_t(mq->roster.raster.cull_mode),
							uint32_t(mq->roster.depth_stencil.depth_compare_operator),
							mq->roster.depth_stencil.enable_depth_write ? 1u : 0u,
						};
						if (transparent) {
							const auto &a = mq->roster.blend.attachments[0];
							const uint64_t state[] = {pkey.fb_format, pkey.vertex_format, pkey.primitive, pkey.cull_mode, pkey.depth_compare,
								pkey.depth_write, uint64_t(mq->roster.depth_stencil.enable_depth_test), uint64_t(a.enable_blend),
								uint64_t(a.color_blend_op), uint64_t(a.alpha_blend_op), uint64_t(a.src_color_blend_factor),
								uint64_t(a.dst_color_blend_factor), uint64_t(a.src_alpha_blend_factor), uint64_t(a.dst_alpha_blend_factor),
								uint64_t(a.write_r) | (uint64_t(a.write_g) << 1) | (uint64_t(a.write_b) << 2) | (uint64_t(a.write_a) << 3), uint64_t(mq->roster.transparent_pass)};
							meta.pipeline_key_state = mq_sha256_domain("ange.mq.pipeline_key_transparent_state.v1", state, sizeof(state));
						} else {
							meta.pipeline_key_state = mq_sha256_domain("ange.mq.pipeline_key_state.v1", &pkey, sizeof(pkey));
						}

						uint64_t dep_ids[MAX_BORROWED_DEPS] = {};
						const uint32_t dep_count = uint32_t(mq->roster.borrowed.size());
						for (uint32_t b = 0; b < dep_count; ++b) {
							dep_ids[b] = mq->roster.borrowed[int(b)].get_id();
						}
						meta.material_dependencies = mq_sha256_domain("ange.mq.material_dependencies.v1", dep_ids, size_t(dep_count) * sizeof(uint64_t));

						meta.native_shader_rid = mq->native_shader_rid.get_id();
						meta.pipeline_rid = mq->pipeline_rid.get_id();
						meta.render_target_rid = mq->framebuffer_rid.get_id();
						meta.actual_pass = mq->roster.transparent_pass ? 1 : 0;
						meta.actual_samples = 1;
						{
							MutexLock rd_lock(rd._thread_safe_);
							if (const auto *sh = rd.shader_owner.get_or_null(mq->native_shader_rid)) {
								meta.native_shader_generation = sh->driver_id.id ? sh->driver_id.id : mq->native_shader_rid.get_id();
							}
							if (const auto *pl = rd.render_pipeline_owner.get_or_null(mq->pipeline_rid)) {
								meta.pipeline_generation = pl->driver_id.id ? pl->driver_id.id : mq->pipeline_rid.get_id();
							}
						}
						prepared_meta_valid = true;
					}
				}
			}

			rd._thread_safe_.lock();
			relock_mutex_after_call();
			CRASH_COND(!request.capture_inflight);
			--request.capture_inflight;

			if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
				return ERR_INVALID_DATA;
			}
			++usage.passes;
			++usage.receipts;

			if (recipe_err == ERR_BUSY) {
				return ERR_BUSY;
			}
			if (recipe_err == OK && prepared_meta_valid) {
				std::memcpy(attachment->payload + mq->result_payload_offset, &prepared_meta, sizeof(prepared_meta));
				mq->recipe_ready = true;
				action.prepared = true;
			} else {
				mq->recipe_failed = true;
				mq->recipe_error = recipe_err != OK ? recipe_err : ERR_CANT_CREATE;
			}
			if (request.revision < UINT64_MAX) {
				++request.revision;
			}
		}
	}
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_record_material_qualification(GDExtensionProductionPhysicalContextRefV1 context, GDExtensionProductionRequestRefV1 ref, uint64_t action_ordinal) {
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *rd = nullptr;
	Error err = production_physical_request_locked(context, ref, request, rd, true);
	if (err != OK) {
		return mq_result(err, 16);
	}
	if (!rd->_thread_safe_.try_lock()) {
		return mq_result(ERR_BUSY, 16);
	}
	RdUnlock rd_unlock{ rd->_thread_safe_ };

	if (request->contract_version != 2 || request->effects_pending || request->draw_list_open || rd->production_recording_request || rd->draw_list.active || rd->raytracing_list.active || rd->draw_graph.has_driver_callbacks()) {
		return mq_result(ERR_BUSY, 16);
	}
	if (action_ordinal >= request->action_count) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	auto &action = request->actions[action_ordinal];
	auto *mq = action.material_qualification;
	if (action.declaration.action_kind != GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1 || !mq || !mq->recipe_ready || mq->recipe_failed || !action.prepared) {
		return mq_result(ERR_UNCONFIGURED, 6);
	}
	if (mq->recorded) {
		return mq_result(ERR_ALREADY_EXISTS, 5);
	}
	if (request->segment.segment != request->acknowledged_segment || action_ordinal != request->next_action) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	err = production_freeze_rd_locked(*request, rd);
	if (err != OK) {
		return mq_result(err, 16);
	}
	if (request->compute_list_open) {
		err = production_close_lists_locked(*request, *rd);
		if (err != OK) {
			return mq_result(err, 16);
		}
	}
	if (request->capacity.graph_memberships - request->graph_count < 4 ||
			production_serials.exhausted || production_serials.next > UINT64_MAX - 3 ||
			request->revision > UINT64_MAX - uint64_t(request->capacity.graph_memberships) * 2 - uint64_t(request->capacity.native_memberships) * 2 - request->capacity.readbacks - request->capacity.resources - 16) {
		return mq_result(ERR_PARAMETER_RANGE_ERROR, 19);
	}
	uint32_t uncaptured_graphs = request->capacity.graph_memberships - request->graph_count;
	for (uint32_t g = 0; g < request->graph_count; ++g) {
		uncaptured_graphs += request->graphs[g].pending;
	}
	uint32_t memberships = 0;
	err = production_membership_bound_locked(*rd, uncaptured_graphs, request->native_workers_bound, memberships);
	if (err != OK) {
		return mq_result(err, 19);
	}
	if (request->native_count > request->capacity.native_memberships || memberships > request->capacity.native_memberships - request->native_count) {
		return mq_result(ERR_PARAMETER_RANGE_ERROR, 19);
	}

	uint8_t *raw_depth_dst = request->attachment->payload + mq->result_payload_offset + sizeof(GDExtensionMaterialQualificationResultMetadataV1);
	std::memset(raw_depth_dst, 0, GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1);

	mq->readback_state = {};
	mq->readback_state.request_serial = request->serial;
	mq->readback_segments[0] = {};
	mq->readback_segments[1] = {};
	mq->readback_capture = {};
	mq->readback_capture.registry = this;
	mq->readback_capture.request_serial = request->serial;
	mq->readback_capture.state = &mq->readback_state;
	mq->readback_capture.bytes = raw_depth_dst;
	mq->readback_capture.byte_capacity = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
	mq->readback_capture.segments = mq->readback_segments;
	mq->readback_capture.segment_stride = sizeof(BufferUploadSourceSegment);
	mq->readback_capture.capacity = 2;
	mq->readback_capture.count = 0;

	const OriginalOperationIdentity identity{
		request->operation.owner_request_nonce,
		request->attachment->library_generation,
		device_records[request->attachment->device].session,
		request->operation.record_generation,
		request->operation.canonical_begin,
		request->operation.canonical_end
	};

	err = production_output_native_append_enter_locked();
	if (err != OK) {
		return mq_result(err, 16);
	}
	++request->capture_inflight;
	production_append_effect_enter_locked(*request);
	rd->production_recording_request = request;
	rd->production_mq_permitted_framebuffer = mq->framebuffer_rid;
	rd->production_mq_permitted_readback_ssbo = mq->depth_readback_ssbo;

	unlock_mutex_for_call();

    const bool transparent = mq->prepare.recipe == GDEXTENSION_MATERIAL_QUALIFICATION_RECIPE_CURRENT_RESET_F32_WORLD_TRANSPARENT_V1;
	Color clear_colors[1] = { transparent ? Color(0.125f, 0.25f, 0.5f, 0.5f) : Color(0.0f, 0.0f, 0.0f, 1.0f) };
	const auto begun = rd->_draw_list_begin_recorded(
			mq->framebuffer_rid,
			RenderingDevice::DRAW_CLEAR_COLOR_0 | RenderingDevice::DRAW_CLEAR_DEPTH,
			VectorView<Color>(clear_colors, 1),
			0.0f,
			0,
			Rect2(),
			0,
			false);
	RenderingDevice::DrawAppendResult bind_pipe;
	RenderingDevice::DrawAppendResult drawn;
	RenderingDevice::DrawAppendResult ended;
	Error async_err = ERR_UNCONFIGURED;

	auto record_direct_depth_readback = [&]() -> Error {
		if (!rd->native_immutable_is_render_thread()) {
			return ERR_UNAVAILABLE;
		}
		MutexLock rd_lock(rd->_thread_safe_);
		if (rd->_recording_error() != OK) {
			return rd->_recording_error();
		}
		if (rd->draw_list.active || rd->compute_list.active || rd->raytracing_list.active) {
			return ERR_BUSY;
		}
		RenderingDevice::Texture *tex = rd->texture_owner.owns(mq->depth_tex) ? rd->texture_owner.get_or_null(mq->depth_tex) : nullptr;
		RenderingDevice::Buffer *staging_ref = rd->storage_buffer_owner.owns(mq->depth_readback_ssbo) ? rd->storage_buffer_owner.get_or_null(mq->depth_readback_ssbo) : nullptr;
		if (!tex || tex->width != GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_WIDTH_V1 || tex->height != GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_HEIGHT_V1 || tex->format != RenderingDevice::DATA_FORMAT_D32_SFLOAT || !(tex->usage_flags & RenderingDevice::TEXTURE_USAGE_CAN_COPY_FROM_BIT) || !tex->draw_tracker) {
			return ERR_INVALID_PARAMETER;
		}
		RDD::TextureSubresource subres;
		subres.aspect = RDD::TEXTURE_ASPECT_DEPTH;
		subres.layer = 0;
		subres.mipmap = 0;
		RDD::TextureCopyableLayout mip_layout = {};
		rd->driver->texture_get_copyable_layout(tex->driver_id, subres, &mip_layout);
		if (mip_layout.row_pitch != GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_ROW_BYTES_V1 || mip_layout.size < GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1) {
			return ERR_UNAVAILABLE;
		}
		const uint32_t align = MAX(32u, uint32_t(rd->driver->api_trait_get(RDD::API_TRAIT_TEXTURE_TRANSFER_ALIGNMENT)));
		uint32_t staging_offset = 0;
		uint32_t staging_amount = 0;
		RenderingDevice::StagingRequiredAction staging_action = RenderingDevice::STAGING_REQUIRED_ACTION_NONE;
		const Error alloc_err = rd->_staging_buffer_allocate(rd->download_staging_buffers, GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1, align, staging_offset, staging_amount, staging_action, false);
		if (alloc_err != OK) {
			mq->readback_state.error = alloc_err;
			return alloc_err;
		}
		if (staging_action != RenderingDevice::STAGING_REQUIRED_ACTION_NONE || staging_amount != GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1) {
			return ERR_BUSY;
		}
		if (rd->_texture_make_mutable(tex, mq->depth_tex)) {
			mq->readback_capture.became_mutable = true;
			rd->draw_graph.add_synchronization();
		}
		const RDD::BufferID dst_staging = rd->download_staging_buffers.blocks[rd->download_staging_buffers.current].driver_id;
		RDD::BufferTextureCopyRegion tex_region;
		tex_region.buffer_offset = staging_offset;
		tex_region.row_pitch = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_ROW_BYTES_V1;
		tex_region.texture_subresource = subres;
		tex_region.texture_offset = Vector3i(0, 0, 0);
		tex_region.texture_region_size = Vector3i(GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_WIDTH_V1, GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_HEIGHT_V1, 1);
		const RDG::RecordHandle dl_handle = rd->draw_graph.add_texture_get_data(tex->driver_id, tex->draw_tracker, dst_staging, VectorView<RDD::BufferTextureCopyRegion>(&tex_region, 1));
		if (!dl_handle.is_valid()) {
			return ERR_BUG;
		}
		const uint64_t staging_serial = (staging_ref && staging_ref->backing_serial) ? staging_ref->backing_serial : mq->depth_backing_serial;
		mq->readback_state.backing_serial = staging_serial;
		mq->readback_state.offset = staging_offset;
		mq->readback_state.size = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
		mq->readback_state.owned_bytes = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
		mq->readback_state.issued = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;

		RenderingDevice::BufferGetDataRequest dl_req;
		dl_req.buffer = mq->depth_readback_ssbo;
		dl_req.frame_local_index = rd->frames[rd->frame].download_buffer_copy_regions.size();
		dl_req.frame_local_count = 1;
		dl_req.result_offset = 0;
		dl_req.size = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
		dl_req.final_segment = true;
		dl_req.capture = &mq->readback_capture;

		const RDD::BufferCopyRegion buf_region{ 0, staging_offset, GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1 };
		rd->frames[rd->frame].download_buffer_staging_buffers.push_back(dst_staging);
		rd->frames[rd->frame].download_buffer_copy_regions.push_back(buf_region);
		rd->download_staging_buffers.blocks.write[rd->download_staging_buffers.current].fill_amount = staging_offset + GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
		rd->frames[rd->frame].download_buffer_get_data_requests.push_back(dl_req);

		mq->readback_capture.segment(mq->readback_capture.count++) = { dl_handle, 0, GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1 };
		return OK;
	};

	if (begun.error == OK && begun.list != RenderingDevice::INVALID_ID) {
		bind_pipe = rd->_draw_list_bind_render_pipeline_recorded(begun.list, mq->pipeline_rid, false);
		rd->draw_list_bind_vertex_array(begun.list, mq->vertex_array_rid);
		rd->draw_list_bind_index_array(begun.list, mq->index_array_rid);
		for (uint32_t s = 0; s < 3; ++s) {
			if (mq->uniform_sets[s].is_valid()) {
				rd->draw_list_bind_uniform_set(begun.list, mq->uniform_sets[s], s);
			}
		}
		if (mq->material_uniform_set.is_valid()) {
			rd->draw_list_bind_uniform_set(begun.list, mq->material_uniform_set, 3);
		}
		rd->draw_list_set_push_constant(begun.list, mq->push_constant, 16);
		drawn = rd->_draw_list_draw_recorded(begun.list, true, 1, 0, false);
		ended = rd->_draw_list_end_recorded(false);
		if (bind_pipe.error == OK && drawn.error == OK && ended.error == OK && ended.draw_handle.is_valid()) {
			async_err = record_direct_depth_readback();
		} else {
			async_err = ended.error != OK ? ended.error : (drawn.error != OK ? drawn.error : (bind_pipe.error != OK ? bind_pipe.error : ERR_BUG));
		}
	} else {
		async_err = begun.error != OK ? begun.error : ERR_BUG;
	}

	rd->production_mq_permitted_framebuffer = RID();
	rd->production_mq_permitted_readback_ssbo = RID();
	rd->production_recording_request = nullptr;

	relock_mutex_after_call();
	CRASH_COND(!request->capture_inflight);
	--request->capture_inflight;
	request->draw_list_open = rd->draw_list.active;

	auto capture_handle = [&](const RDG::RecordHandle &p_handle) -> Error {
		if (!p_handle.is_valid()) {
			return ERR_INVALID_DATA;
		}
		CheckedRecordAppend observed;
		observed.state = CheckedRecordAppend::State::Appended;
		observed.handle = p_handle;
		const Error bound = bind_checked_record_append(observed, identity);
		if (bound != OK) {
			return bound;
		}
		return production_capture_graph_locked(*request, *rd, action_ordinal, observed);
	};

	if (ended.draw_handle.is_valid() && capture_handle(ended.draw_handle) != OK) {
		request->effects_pending = true;
	}
	for (uint32_t s = 0; s < mq->readback_capture.count; ++s) {
		if (capture_handle(mq->readback_capture.segment(s).handle) != OK) {
			request->effects_pending = true;
		}
	}

	const bool any_append_effect = begun.effect != RenderingDevice::NativeAppendEffect::NoNewEffect ||
			ended.draw_handle.is_valid() || mq->readback_capture.count > 0 || request->draw_list_open;
	if (async_err != OK || !mq->readback_capture.count || request->draw_list_open) {
		if (any_append_effect) {
			request->effects_pending = true;
		}
		++request->revision;
		return mq_result(async_err != OK ? async_err : ERR_BUSY, 13);
	}

	GDExtensionMaterialQualificationResultMetadataV1 meta = {};
	std::memcpy(&meta, request->attachment->payload + mq->result_payload_offset, sizeof(meta));
	production_serials.take(meta.bind_append_serial);
	production_serials.take(meta.draw_append_serial);
	production_serials.take(meta.list_end_append_serial);
	meta.record_revision = request->revision + 1;
	std::memcpy(request->attachment->payload + mq->result_payload_offset, &meta, sizeof(meta));

	action.original_append.state = CheckedRecordAppend::State::Appended;
	action.original_append.handle = ended.draw_handle;
	bind_checked_record_append(action.original_append, identity);

	mq->recorded = true;
	++request->next_action;
	++request->revision;
	return mq_result(request->effects_pending ? ERR_BUSY : OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_read_material_qualification(GDExtensionProductionRequestRefV1 ref, uint64_t action_ordinal, GDExtensionMaterialQualificationResultMetadataV1 *out_metadata, uint32_t metadata_bytes, uint32_t *out_raw_words, uint32_t raw_word_capacity) {
	if (!out_metadata || !out_raw_words ||
			uintptr_t(out_metadata) % alignof(GDExtensionMaterialQualificationResultMetadataV1) != 0 ||
			uintptr_t(out_raw_words) % alignof(uint32_t) != 0 ||
			metadata_bytes != sizeof(GDExtensionMaterialQualificationResultMetadataV1) ||
			raw_word_capacity != GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_WORDS_V1 ||
			mq_ranges_overlap(out_metadata, metadata_bytes, out_raw_words, size_t(raw_word_capacity) * sizeof(uint32_t))) {
		return mq_result(ERR_INVALID_PARAMETER, 1);
	}

	Lock lock(*this);
	ProductionRequest *r = nullptr;
	RenderingDevice *rd = nullptr;
	const Error err = production_request_locked(ref, false, r, rd);
	if (err != OK) {
		return mq_result(err, 16);
	}
	if (r->cancelled) {
		return mq_result(ERR_UNAVAILABLE, 16);
	}
	if (action_ordinal >= r->action_count) {
		return mq_result(ERR_INVALID_DATA, 4);
	}
	const auto &action = r->actions[action_ordinal];
	auto *mq = action.material_qualification;
	if (action.declaration.action_kind != GDEXTENSION_PRODUCTION_MATERIAL_QUALIFICATION_V1 || !mq || !mq->recorded) {
		return mq_result(ERR_UNCONFIGURED, 6);
	}
	if (r->capture_inflight || r->continuation_inflight) {
		return mq_result(ERR_BUSY, 14);
	}

	GDExtensionProductionReportV2 report = {};
	const Error report_err = production_report_contents_locked(*r, report);
	if (report_err != OK) {
		return mq_result(report_err, 16);
	}
	if (report.cause.native_error != OK) {
		return mq_result(Error(report.cause.native_error), report.cause.reason ? report.cause.reason : 14);
	}
	if (r->maintenance_serial != 0 ||
			report.possible_obligations != 0 ||
			report.known_obligations == 0 ||
			report.known_obligations != report.complete_obligations ||
			!mq->readback_state.delivered ||
			mq->readback_state.returned != GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1 ||
			mq->readback_state.completed != GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1) {
		return mq_result(ERR_BUSY, 14);
	}
	if (report.fact_count > GDEXTENSION_MATERIAL_QUALIFICATION_MAX_FACTS_V1) {
		return mq_result(ERR_PARAMETER_RANGE_ERROR, 19);
	}

	GDExtensionMaterialQualificationResultMetadataV1 final_meta = {};
	std::memcpy(&final_meta, r->attachment->payload + mq->result_payload_offset, sizeof(final_meta));
	final_meta.result_revision = r->revision;
	final_meta.state = GDEXTENSION_MATERIAL_QUALIFICATION_COMPLETE_V1;
	final_meta.completion_revision = r->revision;
	final_meta.physical_facts_revision = r->revision;
	final_meta.issue = 1;
	final_meta.required_count = report.complete_obligations;
	final_meta.known_count = report.known_obligations;
	final_meta.possible_count = report.possible_obligations;
	final_meta.native_error = int32_t(OK);
	final_meta.reason = 0;

	final_meta.depth.attachment_rid = mq->depth_tex.get_id();
	final_meta.depth.backing_registry_instance = instance;
	final_meta.depth.backing_serial = mq->depth_backing_serial;
	final_meta.depth.allocation_generation = mq->depth_allocation_generation;
	final_meta.depth.readback_request = r->serial;
	final_meta.depth.staging_backing_serial = mq->readback_state.backing_serial;
	final_meta.depth.source_offset = 0;
	final_meta.depth.source_bytes = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
	final_meta.depth.staging_offset = mq->readback_state.offset;
	final_meta.depth.staging_bytes = mq->readback_state.size;
	final_meta.depth.layer = 0;
	final_meta.depth.mip = 0;
	final_meta.depth.samples = 1;
	final_meta.depth.format = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_D32_SFLOAT_LE_V1;
	final_meta.depth.width = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_WIDTH_V1;
	final_meta.depth.height = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_HEIGHT_V1;
	final_meta.depth.row_bytes = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_ROW_BYTES_V1;
	final_meta.depth.raw_offset = sizeof(GDExtensionMaterialQualificationResultMetadataV1);
	final_meta.depth.raw_bytes = GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1;
	final_meta.depth.issued_bytes = uint32_t(mq->readback_state.issued);
	final_meta.depth.completed_bytes = uint32_t(mq->readback_state.completed);
	final_meta.depth.owned_bytes = uint32_t(mq->readback_state.owned_bytes);
	final_meta.depth.delivered = mq->readback_state.delivered ? 1u : 0u;
	final_meta.depth.source_returned = (mq->readback_state.returned == GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1) ? 1u : 0u;
	final_meta.depth.reserved0 = 0;
	final_meta.depth.reserved1 = 0;

	std::memcpy(r->attachment->payload + mq->result_payload_offset, &final_meta, sizeof(final_meta));
	*out_metadata = final_meta;
	std::memcpy(out_raw_words, r->attachment->payload + mq->result_payload_offset + sizeof(GDExtensionMaterialQualificationResultMetadataV1), GDEXTENSION_MATERIAL_QUALIFICATION_DEPTH_RAW_BYTES_V1);
	return mq_result(OK);
}

Error Registry::production_material_qualification_release_locked(ProductionRequest &request, RenderingDevice &rd, const GDExtensionProductionServiceBudgetV1 &budget, ProductionServiceUsage &usage) {
	bool any_mq = false;
	for (uint32_t i = 0; i < request.action_count; ++i) {
		if (request.actions[i].material_qualification) {
			any_mq = true;
			break;
		}
	}
	if (!any_mq) {
		return OK;
	}
	if (request.capture_inflight || request.continuation_inflight) {
		return ERR_BUSY;
	}
	if (!service_budget_active(budget) || !rd.native_immutable_is_render_thread() || !rd._thread_safe_.try_lock()) {
		return ERR_BUSY;
	}
	RdUnlock rd_unlock{ rd._thread_safe_ };

	// Step 1: If any action recorded GPU commands or async readback, drain K01 native & graph & readback completion first (in-place, no next draw required).
	if (!request.source_device_lost) {
		bool recorded_in_flight = false;
		for (uint32_t g = 0; g < request.graph_count; ++g) {
			recorded_in_flight |= request.graphs[g].pending;
		}
		for (uint32_t i = 0; i < request.action_count; ++i) {
			if (const auto *mq = request.actions[i].material_qualification) {
				recorded_in_flight |= mq->readback_state.issued && mq->readback_state.returned != mq->readback_state.issued;
			}
		}
		if (recorded_in_flight) {
			const Error maint = production_maintenance_locked(request, rd, budget, usage);
			if (maint != OK && maint != ERR_BUSY) {
				return maint;
			}
		}
		bool natives_incomplete = false;
		for (uint32_t n = 0; n < request.native_count; ++n) {
			const ProductionNativeCell *owner = nullptr;
			if (production_native_owner_locked(request, n, owner) != OK || !owner) {
				return ERR_INVALID_DATA;
			}
			if (request.natives[n].owner_index == n && owner->observation.state != NativeSourceObservation::State::Complete) {
				natives_incomplete = true;
			}
		}
		if (natives_incomplete) {
			const Error polled = production_poll_native_locked(request, *rd.driver, budget, device_records[request.attachment->device].live, usage);
			if (polled != OK && polled != ERR_BUSY) {
				return polled;
			}
		}
		for (uint32_t g = 0; g < request.graph_count; ++g) {
			if (request.graphs[g].pending) {
				return ERR_BUSY;
			}
		}
		for (uint32_t n = 0; n < request.native_count; ++n) {
			const ProductionNativeCell *owner = nullptr;
			if (production_native_owner_locked(request, n, owner) != OK || !owner) {
				return ERR_INVALID_DATA;
			}
			if (request.natives[n].owner_index == n && owner->observation.state != NativeSourceObservation::State::Complete) {
				return ERR_BUSY;
			}
		}
		for (uint32_t i = 0; i < request.action_count; ++i) {
			if (const auto *mq = request.actions[i].material_qualification) {
				if (mq->readback_state.issued && mq->readback_state.returned != mq->readback_state.issued) {
					return ERR_BUSY;
				}
			}
		}
	}

	// Step 2: Poll any unsettled owned transfer workers non-blockingly, release immutable holds, and enqueue owned RD resources onto RD frame disposal lists.
	for (uint32_t i = 0; i < request.action_count; ++i) {
		auto *mq = request.actions[i].material_qualification;
		if (!mq || mq->rids_freed) {
			continue;
		}
		mq->retiring_started = true;
		if (!mq_budget_has_step(budget, usage)) {
			return ERR_BUSY;
		}
		const uint64_t request_serial = request.serial;
		const uint64_t control_serial = request.control.serial;
		auto *const attachment = request.attachment;
		const uint64_t epoch = attachment->epoch;

		RenderingDevice::NativeDisposalBudget poll_budget;
		poll_budget.receipts = MIN(budget.receipts, 128u) - usage.receipts;
		poll_budget.passes = MIN(budget.passes, 64u) - usage.passes;
		poll_budget.deadline_usec = budget.deadline_micros;
		RenderingDevice::NativeDisposalUsage poll_usage;

		++request.capture_inflight;
		rd._thread_safe_.unlock();
		unlock_mutex_for_call();

		Error poll_err = OK;
		if (!request.source_device_lost && rd.driver) {
			if (mq->fixture_mesh_rid.is_valid() && !mq->fixture_mesh_settled) {
				if (auto *meshes = RendererRD::MeshStorage::get_singleton()) {
					auto *mesh_ptr = meshes->mesh_owner.get_or_null(mq->fixture_mesh_rid);
					auto *surf_ptr = (mesh_ptr && mesh_ptr->surface_count == 1) ? mesh_ptr->surfaces[0] : nullptr;
					if (surf_ptr) {
						const RID surf_bufs[5] = {
							surf_ptr->vertex_buffer,
							surf_ptr->attribute_buffer,
							surf_ptr->skin_buffer,
							surf_ptr->index_buffer,
							surf_ptr->blend_shape_buffer,
						};
						for (uint32_t k = 0; k < 5 && poll_err == OK; ++k) {
							if (surf_bufs[k].is_valid() && !mq->fixture_buf_settled[k]) {
								poll_err = rd._production_mq_poll_transfer_worker_nonblocking(surf_bufs[k], poll_budget, poll_usage);
								if (poll_err == OK) {
									mq->fixture_buf_settled[k] = true;
								}
							}
						}
					}
				}
				if (poll_err == OK) {
					mq->fixture_mesh_settled = true;
				}
			}
			for (uint32_t b = 0; b < MaterialQualificationState::MAX_OWNED_BUFFERS && poll_err == OK; ++b) {
				if (mq->owned_buffers[b].is_valid() && !mq->owned_buffer_settled[b]) {
					poll_err = rd._production_mq_poll_transfer_worker_nonblocking(mq->owned_buffers[b], poll_budget, poll_usage);
					if (poll_err == OK) {
						mq->owned_buffer_settled[b] = true;
					}
				}
			}
			if (poll_err == OK && mq->depth_readback_ssbo.is_valid() && !mq->readback_prepared) {
				poll_err = rd._production_mq_poll_transfer_worker_nonblocking(mq->depth_readback_ssbo, poll_budget, poll_usage);
				if (poll_err == OK) {
					mq->readback_prepared = true;
				}
			}
		}

		const uint32_t polled_receipts = poll_usage.polls + poll_usage.frees + poll_usage.issues + poll_usage.retains + poll_usage.releases + poll_usage.recycles;
		const bool can_free_now = poll_err == OK &&
				(usage.passes + poll_usage.passes < MIN(budget.passes, 64u)) &&
				(usage.receipts + polled_receipts < MIN(budget.receipts, 128u)) &&
				OS::get_singleton()->get_ticks_usec() < budget.deadline_micros;

		Error free_err = poll_err;
		if (can_free_now) {
			if (mq->derived_surface.is_active()) {
				free_err = RendererRD::MeshStorage::mesh_surface_hold_release(mq->derived_surface);
			}
			if (free_err == OK && mq->roster.is_active()) {
				free_err = mq->roster.release();
			}
			if (free_err == OK) {
				mq->vertex_array_rid = RID();
				mq->index_array_rid = RID();
				mq->material_uniform_set = RID();
				mq->native_shader_rid = RID();

				if (mq->framebuffer_rid.is_valid()) {
					{
						MutexLock fb_lock(mq->framebuffer_physical.mutex);
						mq->framebuffer_physical.use_closed.store(true, std::memory_order_release);
					}
					MutexLock rd_lock(rd._thread_safe_);
					rd.qualification_retiring = true;
					rd.free_rid(mq->framebuffer_rid);
					rd.qualification_retiring = false;
					mq->framebuffer_rid = RID();
				} else if (mq->framebuffer_physical.event_capacity_reserved) {
					MutexLock rd_lock(rd._thread_safe_);
					rd._qualification_release_free_event(&mq->framebuffer_physical);
				}

				if (mq->pipeline_rid.is_valid()) {
					rd.free_rid(mq->pipeline_rid);
					mq->pipeline_rid = RID();
				}
				for (int s = 0; s < 3; ++s) {
					if (mq->uniform_sets[s].is_valid()) {
						rd.free_rid(mq->uniform_sets[s]);
						mq->uniform_sets[s] = RID();
					}
				}
				if (mq->fixture_mesh_rid.is_valid()) {
					if (auto *meshes = RendererRD::MeshStorage::get_singleton()) {
						meshes->mesh_free(mq->fixture_mesh_rid);
					}
					mq->fixture_mesh_rid = RID();
				}
				for (uint32_t b = 0; b < MaterialQualificationState::MAX_OWNED_BUFFERS; ++b) {
					if (mq->owned_buffers[b].is_valid()) {
						rd.free_rid(mq->owned_buffers[b]);
						mq->owned_buffers[b] = RID();
					}
				}
				mq->owned_buffer_count = 0;
				if (mq->color_tex.is_valid()) {
					rd.free_rid(mq->color_tex);
					mq->color_tex = RID();
				}
				if (mq->depth_tex.is_valid()) {
					rd.free_rid(mq->depth_tex);
					mq->depth_tex = RID();
				}
				if (mq->depth_readback_ssbo.is_valid()) {
					rd.free_rid(mq->depth_readback_ssbo);
					mq->depth_readback_ssbo = RID();
				}
			}
		} else if (free_err == OK) {
			free_err = ERR_BUSY;
		}

		rd._thread_safe_.lock();
		relock_mutex_after_call();
		CRASH_COND(!request.capture_inflight);
		--request.capture_inflight;

		if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
			return ERR_INVALID_DATA;
		}
		usage.passes += poll_usage.passes;
		usage.receipts += polled_receipts;
		if (free_err != OK) {
			return free_err;
		}
		mq->rids_freed = true;
		++usage.passes;
		++usage.receipts;
	}

	// Step 3: Drain this request's owned K03 disposal witnesses with NO following draw,
	// without stalling GPU frames or touching foreign engine disposals.
	if (production_material_qualification_needs_maintenance_locked(request)) {
		if (!mq_budget_has_step(budget, usage)) {
			return ERR_BUSY;
		}
		static constexpr uint32_t MAX_MQ_FB_PHYSICALS = 32;
		const QualificationPhysicalReturn *fb_physicals[MAX_MQ_FB_PHYSICALS] = {};
		uint32_t fb_count = 0;
		for (uint32_t i = 0; i < request.action_count; ++i) {
			if (const auto *mq = request.actions[i].material_qualification) {
				if (fb_count >= MAX_MQ_FB_PHYSICALS) {
					return ERR_PARAMETER_RANGE_ERROR;
				}
				fb_physicals[fb_count++] = &mq->framebuffer_physical;
			}
		}
		const uint64_t request_serial = request.serial;
		const uint64_t control_serial = request.control.serial;
		auto *const attachment = request.attachment;
		const uint64_t epoch = attachment->epoch;

		RenderingDevice::NativeDisposalBudget drain_budget;
		drain_budget.receipts = MIN(budget.receipts, 128u) - usage.receipts;
		drain_budget.passes = MIN(budget.passes, 64u) - usage.passes;
		drain_budget.deadline_usec = budget.deadline_micros;
		RenderingDevice::NativeDisposalUsage drain_usage;

		production_native_effect_enter_locked(request);
		++request.capture_inflight;
		rd._thread_safe_.unlock();
		unlock_mutex_for_call();

		const Error drain_err = rd._production_mq_drain_owned_disposals_step(request, fb_physicals, fb_count, drain_budget, drain_usage);

		rd._thread_safe_.lock();
		relock_mutex_after_call();
		CRASH_COND(!request.capture_inflight);
		--request.capture_inflight;

		if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
			return ERR_INVALID_DATA;
		}
		usage.receipts += drain_usage.polls + drain_usage.frees + drain_usage.issues + drain_usage.retains + drain_usage.releases + drain_usage.recycles;
		usage.passes += drain_usage.passes;
		if (drain_err != OK) {
			return drain_err;
		}
		if (production_material_qualification_needs_maintenance_locked(request)) {
			return ERR_BUSY;
		}
	}

	// Step 4: Release candidate MaterialStorage & MeshStorage selection leases and reset pre-allocated slot.
	for (uint32_t i = 0; i < request.action_count; ++i) {
		auto *&mq = request.actions[i].material_qualification;
		if (!mq) {
			continue;
		}
		if (mq->candidate_material_lease.is_active() || mq->candidate_mesh_lease.is_active()) {
			const uint64_t request_serial = request.serial;
			const uint64_t control_serial = request.control.serial;
			auto *const attachment = request.attachment;
			const uint64_t epoch = attachment->epoch;

			++request.capture_inflight;
			rd._thread_safe_.unlock();
			unlock_mutex_for_call();

			Error lease_err = OK;
			if (mq->candidate_material_lease.is_active()) {
				auto *materials = RendererRD::MaterialStorage::get_singleton();
				lease_err = materials ? materials->material_selection_release(mq->candidate_material_lease) : ERR_UNAVAILABLE;
			}
			if (lease_err == OK && mq->candidate_mesh_lease.is_active()) {
				auto *meshes = RendererRD::MeshStorage::get_singleton();
				lease_err = meshes ? meshes->mesh_selection_release(mq->candidate_mesh_lease) : ERR_UNAVAILABLE;
			}

			rd._thread_safe_.lock();
			relock_mutex_after_call();
			CRASH_COND(!request.capture_inflight);
			--request.capture_inflight;

			if (request.serial != request_serial || request.control.serial != control_serial || request.control.state != ProductionControlState::EXECUTING || request.attachment != attachment || attachment->held_device != &rd || attachment->epoch != epoch || device_records[attachment->device].epoch != epoch || device_records[attachment->device].object != &rd) {
				return ERR_INVALID_DATA;
			}
			if (lease_err != OK) {
				return lease_err;
			}
		}
		production_material_qualification_destroy_empty(mq);
	}
	return OK;
}

} // namespace GDExtensionDeviceExit
