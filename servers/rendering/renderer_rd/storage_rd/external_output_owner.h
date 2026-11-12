/**************************************************************************/
/*  external_output_owner.h                                               */
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

#include "core/extension/gdextension_production_request.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_packet.h"
#include "servers/rendering/renderer_rd/storage_rd/external_output_request.h"
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"

namespace RendererRD {
class ParticlesStorage;
class TestExternalOutputOwnerStateAccess;
class ExternalOutputStorage;

// The retained output state belongs to exactly one original Particles RID.
// Source tags are frozen equality projections, never source or budget grants.
// This object has no world, operation issuer, scheduler, or automatic submission.
class ExternalOutputOwner {
	friend class GDExtensionDeviceExit::Registry;
	friend class ::RenderingDevice;
	friend class ParticlesStorage;
	friend class ExternalPacketViewLease;
	friend class ExternalSurfaceSelection;
	friend class ExternalPacketBinding;
	friend class ExternalPipelinePreparation;
#ifdef TESTS_ENABLED
	friend class TestExternalOutputOwnerStateAccess;
	friend class ExternalLinkedOwnerTest;
#endif
	static constexpr uint32_t MAX_VIEWS = 8;
	static constexpr uint32_t MAX_INFLIGHT = 4;
	static constexpr uint32_t MAX_SLOTS = MAX_VIEWS * MAX_INFLIGHT;

	enum class GenerationState : uint8_t {
		CONFIGURED,
		ACTIVE,
		WITHDRAWING,
		RETIRING,
		FAULTED_RETIRING,
		RELEASED,
	};
	enum class LeaseState : uint8_t {
		EMPTY,
		RESERVED,
		WRITING,
		SEALED_RECORDED,
		ADOPTED,
		EMPTY_OR_FAULT_PACKET,
		RETIRING,
		RELEASED,
	};
	struct SurfaceBinding {
		// Reserved metadata is distinct from an opened native pass prefix.
		// The stable row belongs to this slot incarnation; it grants no pass.
		ExternalPipelinePreparation *preparation_row = nullptr;
		uint32_t preparation_index = UINT32_MAX; // Stable owner-bank locator, independent of slot/surface arithmetic.
		ExternalPipelinePreparation *preparation = nullptr;
		// Legacy component fixture custody only; production bank roots/groups
		// live in preparation_row and are never mirrored into these fields.
		RID created_pipeline;
		uint64_t pipeline_owned_group = 0;
		bool pipeline_group_returned = false;
		MeshStorage::NativeSurfaceHold geometry;
		MaterialStorage::NativeMaterialHold material;
		uint64_t command_offset = 0;
		uint32_t command_extent = 0;
		uint32_t color_draw_graph = UINT32_MAX, color_close_graph = UINT32_MAX;
		uint32_t color_empty_close_graph = UINT32_MAX;
		uint64_t geometry_group = 0, material_group = 0;
		bool geometry_transferred = false, material_transferred = false;
		bool geometry_metadata_returned = false, material_metadata_returned = false;
		bool geometry_directory_returned = false;
		bool geometry_group_returned = false, material_group_returned = false;
	};
	struct Slot {
		LeaseState state = LeaseState::EMPTY;
		ExternalOutputRequest request;
		ExternalPacketViewLease consumed_proof_view; // Original quoted ViewCell retained before native append.
		uint64_t consumed_borrow_serial = 0;
		bool consumed_proof_requested = false;
		PackedByteArray lease_token;
		PackedByteArray record_token;
		PackedByteArray packet_token;
		// Native color adoption correlation only, not A/G's whole consumed tuple.
		// Issued by the original checked read/close path, never by cancellation.
		GDExtensionOutputTokenV1 native_color_consumption_token = {};
		RID native_color_consumption_particles; // Original action identity captured by the issuer.
		uint64_t native_color_consumption_serial = 0;
		bool native_color_empty = false; // Persisted original receipt kind, independent of reusable reads.
		uint64_t native_color_read_serial = 0;
		uint32_t native_color_read_cell = UINT32_MAX;
		uint32_t native_color_close_graph = UINT32_MAX;
		ExternalOutputStorage *storage = nullptr;
		bool storage_placement = false, surfaces_placement = false;
		uint32_t physical_returned = 0;
		SurfaceBinding *surfaces = nullptr;
		uint32_t surface_count = 0;
		// Correlation only; the original finite accepted-arena reverse custody
		// must be installed by Registry before this slot can become Reserved.
		// A copied DTO cannot retain a request or authorize any append.
		GDExtensionProductionRequestRefV1 original_request = {};
		GDExtensionDeviceExit::Registry *original_registry = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin original_pin;
		uint64_t original_revision = 0;
		uint64_t original_action_ordinal = UINT64_MAX;
		uint64_t lease_serial = 0;
		// Original prepared action source retained by the authenticated seal.
		Ref<RendererViewport::Viewport::CameraCapture> native_camera_source;
		uint64_t guard_owned_group = 0;
		RID guard_uniform_sets[5];
		NativeDisposalWitness guard_uniform_witnesses[5];
		uint8_t *guard_proof_payload = nullptr;
		uint32_t guard_proof_bytes = 0;
		bool guard_resources_ready = false;
		uint64_t owned_group = 0, output_membership_group = 0;
		uint32_t retire_surface = 0, retire_resource = 0;
		uint32_t read_settle_cursor = 0;
		uint32_t preparation_reserved_count = 0, preparation_selected_count = 0, retire_preparation_index = 0;
		bool output_group_returned = false;
		bool publication_closed = false;
		bool pass_uses_closed = false;
		Transform3D current_host, previous_host;
		bool retained_host_snapshot = false;
	};
	struct View {
		// CPU records for the original at-most-four in-flight slots, priced
		// inline by sizeof(owner). A record grants no source/GPU custody.
		struct ConsumedTuple {
		 GDExtensionOutputTokenV1 token{};
		 GDExtensionOutputCameraV1 camera{};
		 uint64_t present = 0, epoch = 0;
		 bool valid = false;
		} consumed[4];
		PackedByteArray tag;
		uint64_t epoch = 0;
		uint64_t last_present = 0;
		// History points only to a retained slot of this same owner.
		uint32_t history_slot = UINT32_MAX;
		bool occupied = false;
		bool closed = false;
	};

	// Explicit original parent metadata capacity; no implicit per-pass default.
	// Arrays are installed once before any custody and never relocated live.
	struct SurfaceCell {
		ExternalSurfaceSelection *consumer = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin pin;
		uint64_t serial = 0;
		uint32_t slot = UINT32_MAX, surface = UINT32_MAX;
	};
	SurfaceCell *surface_cells = nullptr;
	uint32_t surface_capacity = 0;
	uint64_t next_surface_serial = 1;
	struct ViewCell {
		Ref<RendererViewport::Viewport::CameraCapture> camera_source;
		ExternalPacketViewLease *consumer = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin pin;
		uint64_t serial = 0;
		uint32_t slot = UINT32_MAX;
	};
	struct ReadCell {
		enum class ConsumptionKind : uint8_t { RASTER, EMPTY_COLOR };
		ConsumptionKind consumption_kind = ConsumptionKind::RASTER;
		uint32_t empty_color_pass = 0; // Actual ExternalPacketPass, never schema pass mask.
		enum class State : uint8_t { UNUSED,
			RESERVED,
			RECORDED,
			UNKNOWN,
			RETURNED };
		ExternalPacketBinding *consumer = nullptr;
		GDExtensionDeviceExit::ProductionOutputPin pin;
		// When this read is a successor with a validated consumed tuple, retain
		// the predecessor's original pin/source alongside the current read pin.
		// This is source custody only; it does not synthesize previous records.
		GDExtensionDeviceExit::ProductionOutputPin previous_pin;
		uint32_t previous_slot = UINT32_MAX;
		Ref<RendererViewport::Viewport::CameraCapture> previous_camera_source;
		uint64_t serial = 0;
		uint32_t slot = UINT32_MAX;
		State state = State::UNUSED;
		// Paid by the original metadata quote; never relocated while pinned.
		static constexpr uint32_t GRAPH_CAPACITY = 40;
		uint32_t graph_indices[GRAPH_CAPACITY] = {};
		uint32_t graph_count = 0, graph_limit = 0;
		uint32_t preparation = UINT32_MAX, surface = UINT32_MAX;
		uint32_t draw_graph = UINT32_MAX, close_graph = UINT32_MAX;
		uint32_t read_ordinal = UINT32_MAX;
		bool admitted = false, capture_complete = false, uncaptured_effect = false;
		bool draw_accepted = false;
		// Original Registry serial prepaid before native effects. Not published
		// until successful color coverage/list closure commits this slot.
		uint64_t color_consumption_serial = 0;
		uint32_t settle_graph_cursor = 0, settle_native_cursor = 0;
		bool settle_member_found = false;
		ExternalOutputOwner *list_next_owner = nullptr;
		uint32_t list_next_read = UINT32_MAX;
		RDG::RecordHandle list_begin;
		bool list_linked = false;
		static constexpr uint32_t TRANSFER_CAPACITY = 32;
		RenderingDevice::NativeTransferDependency transfers[TRANSFER_CAPACITY];
		uint32_t transfer_count = 0;
		bool raster_bound = false; // Only the exact RD raster resource scope may set this.

		// Inline in the original prequoted ReadCell (sizeof(ReadCell) layout).
		// This retains native source facts only. No C01/Pending/view authority.
		Ref<RendererViewport::Viewport::CameraCapture> camera_source;
		RID captured_viewport;
		RendererViewport::Viewport::ViewPresentationKey camera_key;
		RendererViewport::Viewport::CameraSourceToken camera_token;
		RendererViewport::Viewport::CameraInvocation camera_invocation;
		RendererSceneRender::CameraData camera_data;
		float camera_origin[3] = {}, camera_forward[3] = {};
		bool camera_retained = false;
	};
	ViewCell *view_cells = nullptr;
	ReadCell *read_cells = nullptr;
	uint32_t view_capacity = 0, read_capacity = 0;
	uint64_t next_view_serial = 1, next_read_serial = 1;
	// Placement directories belong to the original metadata block. Their
	// element destruction never returns that block or permits reinstallation.
	bool metadata_directory_installed = false;
	bool metadata_directories_placement = false;
	struct GenerationMeshSelection {
		RID mesh;
		MeshStorage::NativeSelectionLease lease;
	};
	struct GenerationMaterialSelection {
		// Flattened original source row. CPU selection custody and immutable
		// native backing custody are independent; native source holds return first.
		RID mesh;
		uint32_t native_surface = UINT32_MAX;
		MeshStorage::NativeSurfaceHold geometry_source;
		RID material;
		uint64_t generation = 0;
		MaterialStorage::NativeSelectionLease lease;
	};
	GenerationMeshSelection *generation_meshes = nullptr;
	GenerationMaterialSelection *generation_materials = nullptr;
	uint32_t generation_mesh_count = 0, generation_material_count = 0;
	uint32_t retained_mesh_count = 0, retained_material_count = 0, retained_surface_count = 0;
	bool generation_surfaces_attempted = false, generation_surfaces_complete = false;
	bool generation_selection_unpublished = false;
	bool generation_config_inline = false;
	uint32_t binding_capacity = 0, storage_capacity = 0;
	ExternalPipelinePreparation *preparation_rows = nullptr;
	uint32_t preparation_capacity = 0;
	bool preparation_row_matches(uint32_t, uint32_t, const ExternalPipelinePreparation &) const;
	bool preparation_bank_row_matches(uint32_t, uint32_t, const ExternalPipelinePreparation &) const;
	uint32_t free_preparation_rows() const;
	Error bind_preparation_rows(uint32_t, uint32_t p_count = 0);
	Error return_preparation_bank_row(uint32_t, uint32_t);
	bool preparation_source_matches(const ExternalPipelinePreparation &) const;
	Error material_future_pipeline_plan(const ExternalPipelinePreparation &, RID, RID, MaterialStorage::NativeMaterialFuturePipelinePlan &) const;
	GDExtensionDeviceExit::Registry *preparation_registry(const ExternalPipelinePreparation &p_row) const {
		if (p_row.owner != this || p_row.owner_slot >= slot_count) {
			return nullptr;
		}
		const auto &slot = slots[p_row.owner_slot];
		return p_row.pipeline_pin == &slot.original_pin && slot.original_registry == slot.original_pin.registry ? slot.original_registry : nullptr;
	}
	GDExtensionDeviceExit::ProductionRequest *preparation_request(uint32_t p_slot) const { return p_slot < slot_count ? slots[p_slot].original_pin.request : nullptr; }
	uint64_t preparation_pin_serial(uint32_t p_slot) const { return p_slot < slot_count ? slots[p_slot].original_pin.serial : 0; }
	Error select_preparation_tuple(uint32_t, uint32_t, const ExternalPacketPipelineRequest &, uint32_t &, bool &);
	bool preparation_view_matches(const ExternalPacketViewLease &, uint32_t, bool p_cleanup = false) const;
	Error return_preparation_row(uint32_t, uint32_t);
	bool generation_selection_attempted = false;
	bool generation_selection_complete = false;
	bool has_generation_selection_custody() const { return retained_mesh_count || retained_material_count || retained_surface_count; }
	Error acquire_generation_selection(GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1);
	Error acquire_generation_surface_sources(GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1);
	struct MetadataLayout {
		uint32_t view_count = 0, read_count = 0, surface_count = 0;
		uint32_t view_offset = 0, read_offset = 0, surface_offset = 0;
		uint32_t storage_offset = 0, surface_binding_offset = 0, preparation_offset = 0;
		uint32_t total_bytes = 0, alignment = 0;
		uint32_t guard_proof_offset = 0, guard_proof_bytes = 0;
		uint32_t mesh_selection_count = 0, material_selection_count = 0;
		uint32_t mesh_selection_offset = 0, material_selection_offset = 0;
		uint32_t binding_count = 0, storage_count = 0, preparation_count = 0;
	};
	static Error plan_metadata_layout(uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces, MetadataLayout &r_layout, uint32_t p_mesh_selections = 0, uint32_t p_material_selections = 0, uint32_t p_bindings = UINT32_MAX, uint32_t p_storage = UINT32_MAX, uint32_t p_preparations = UINT32_MAX);
	static Error plan_generation_selection_layout(const ExternalOutputConfig &, uint32_t, uint32_t, uint32_t, MetadataLayout &);
	Error install_metadata_directory(GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1,
			GDExtensionDeviceExit::Registry &, const GDExtensionDeviceExit::ProductionOutputMetadataLease &,
			uint32_t p_views, uint32_t p_reads, uint32_t p_surfaces, bool p_generation_selection = false);

	ExternalOutputConfig config;
	// Production creation stores identity comparison bytes inline in its quoted
	// metadata block; parser-owned PackedByteArray allocations are not retained.
	uint8_t frozen_config_tags[5][32] = {};
	bool config_tags_inline = false;
	GenerationState state = GenerationState::CONFIGURED;
	uint64_t generation = 0;
	RenderingDevice *device = nullptr;
	uint32_t slot_count = 0;
	uint64_t next_lease_serial = 1;
	// Independent original Create-request custody, never borrowed from a slot.
	// The serial/ref remain correlation after return; only the live pin grants custody.
	GDExtensionDeviceExit::ProductionOutputPin generation_pin;
	GDExtensionDeviceExit::ProductionOutputMetadataLease metadata_lease;
	GDExtensionDeviceExit::Registry *generation_registry = nullptr;
	GDExtensionProductionRequestRefV1 generation_request = {};
	uint64_t generation_root_serial = 0;
	View views[MAX_VIEWS];
	Slot slots[MAX_SLOTS];
	// Original parent capacity/accounting and identity association must be
	// installed before constructing this owner. No Dictionary value is a charge.
	ExternalOutputOwner(const ExternalOutputConfig &p_config, uint64_t p_generation, bool p_inline_config = false, bool p_inline_selection = false) :
			config(p_config), generation(p_generation), slot_count(p_config.max_views * p_config.max_inflight) {
		CRASH_COND(!generation || !p_config.max_views || p_config.max_views > MAX_VIEWS || !p_config.max_inflight || p_config.max_inflight > MAX_INFLIGHT);
		CRASH_COND(p_inline_selection && !p_inline_config);
		if (p_inline_config) {
			// Render profiles additionally need the original retained native
			// selection table. Only SceneOnly has no such persistent allocation.
			CRASH_COND(p_inline_selection ? p_config.profile != ExternalOutputConfig::INSTANCE_192 : (p_config.profile != ExternalOutputConfig::SCENE_ONLY || !p_config.surfaces.is_empty() || !p_config.surface_materials.is_empty()));
			PackedByteArray *tags[] = { &config.producer_tag, &config.device_tag, &config.layout_digest, &config.mapping_digest, &config.numeric_profile_digest };
			for (uint32_t i = 0; i < 5; ++i) {
				CRASH_COND(tags[i]->size() != 32);
				memcpy(frozen_config_tags[i], tags[i]->ptr(), 32);
				*tags[i] = PackedByteArray();
			}
			config_tags_inline = true;
		}
	}
	ExternalOutputOwner(const ExternalOutputOwner &) = delete;
	ExternalOutputOwner &operator=(const ExternalOutputOwner &) = delete;

public:
	~ExternalOutputOwner() {
		CRASH_COND(surface_cells || view_cells || read_cells || generation_meshes || generation_materials || preparation_rows || preparation_capacity || has_generation_selection_custody() || !generation_pin.is_empty());
		// No destructor fabricates native/source return or drops accepted uses.
		CRASH_COND(state != GenerationState::RELEASED);
		for (uint32_t i = 0; i < slot_count; ++i) {
			CRASH_COND(slots[i].storage || slots[i].surfaces || slots[i].guard_owned_group || slots[i].guard_resources_ready || slots[i].guard_proof_payload || !slots[i].original_pin.is_empty());
		}
	}

private:
	struct BoundMaterialContact {
		uint64_t output_generation = 0;
		RID mesh;
		uint64_t mesh_revision = 0;
		uint32_t native_surface = UINT32_MAX;
		AABB mesh_bounds;
		uint32_t vertex_count = 0, index_count = 0;
		MaterialStorage::NativeSelectionShaderObservation material;
	};
	// Native-only observation under this original generation's live root and
	// the same material/mesh holds. No exported DTO, qualification or new lease.
	// Failure leaves the caller's observation unchanged.
	Error read_bound_material_contact(uint32_t, BoundMaterialContact &) const;
	// Read-only native selection preflight. This neither retains the selection
	// nor grants publication; Create must retain the same owners before commit.
	static Error validate_native_selection(const ExternalOutputConfig &, uint32_t &);
	// Pure finite preflight only. Original Registry custody and original U08
	// charge must both succeed before a caller commits a Reserved slot.
	Error select_slot(const ExternalOutputRequest &p_request, uint32_t &r_view, uint32_t &r_slot) const;
	// Component substrate for the original accepted Create receiver. The public
	// Create leaf remains unavailable until its real admission/source join exists.
	bool generation_root_matches(const GDExtensionDeviceExit::ProductionOutputPin &) const;
	bool has_inflight_preparation() const;
	Error generation_returnable(uint32_t &, uint32_t &) const;
	Error install_generation_root(GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, GDExtensionDeviceExit::Registry &, RenderingDevice &, RID, GDExtensionDeviceExit::ProductionOutputMetadataLease * = nullptr);
	Error install_reserved_cells(uint32_t p_views, uint32_t p_reads);
	Error install_reserved_surface_cells(uint32_t p_surfaces);
	Error surface_acquire(uint32_t, ExternalSurfaceSelection &);
	bool generation_surface_has_custody() const;
	Error surface_return(ExternalSurfaceSelection &);
	void surface_move(ExternalSurfaceSelection &, ExternalSurfaceSelection &);
	Error return_reserved_cells();
	// A real existing pin is mandatory even for metadata-only reservation.
	Error reserve_slot(const ExternalOutputRequest &, GDExtensionDeviceExit::Registry &, GDExtensionProductionRequestRefV1, const GDExtensionDeviceExit::ProductionOutputPin &, RenderingDevice &, uint32_t &);
	Error cancel_slot(uint32_t);
	bool slot_has_custody(uint32_t) const;
	Error return_slot_pin(uint32_t);
	// Correlation check on retained original slot facts only. Caller still owes
	// original Registry/source authority; this cannot grant a previous read.
	bool native_color_consumption_matches(uint32_t, const GDExtensionOutputTokenV1 &) const;
	// Registry calls this only after original action/guard/record authentication.
	Error seal_recorded_slot(uint32_t, const Ref<RendererViewport::Viewport::CameraCapture> &, const GDExtensionOutputTokenV1 &, const GDExtensionOutputTokenV1 &);
	Error view_acquire(uint32_t, ExternalPacketViewLease &);
	Error view_acquire(uint32_t, RendererViewport &, const Ref<RendererViewport::Viewport::CameraCapture> &, ExternalPacketViewLease &);
	Error view_copy_native_color_consumption(const ExternalPacketViewLease &, ExternalConsumedColorProofSnapshot &) const;
	Error view_retain_native_color_consumption(const ExternalPacketViewLease &, ExternalPacketViewLease &);
	Error view_return(ExternalPacketViewLease &);
	void view_move(ExternalPacketViewLease &, ExternalPacketViewLease &);
	// Metadata custody only under an already-authenticated original view.
	// Does not resolve buffers, record native reads or authorize a draw.
	Error read_reserve_from_view(const ExternalPacketViewLease &, ExternalPacketBinding &);
	// Independent original native capture custody arm. Deliberately private:
	// the missing generation/C01/Pending receiver must authenticate selection
	// before calling; retaining a native capture cannot grant a view or append.
	Error read_retain_native_camera(ExternalPacketBinding &, RendererViewport &, RID);
	Error read_return(ExternalPacketBinding &, bool);
	void read_clear_returned(ReadCell &);
	void read_move(ExternalPacketBinding &, ExternalPacketBinding &);

	// Withdrawal changes admission only. Existing captures, buffers, bindings,
	// original request references and history stay retained for real retirement.
	void invalidate_generation_demand();
	void withdraw() {
		invalidate_generation_demand();
		if (state == GenerationState::RELEASED || state == GenerationState::FAULTED_RETIRING || state == GenerationState::WITHDRAWING || state == GenerationState::RETIRING) {
			return;
		}
		state = GenerationState::WITHDRAWING;
		for (uint32_t i = 0; i < config.max_views; ++i) {
			views[i].closed = true;
		}
		for (uint32_t i = 0; i < slot_count; ++i) {
			slots[i].publication_closed = true;
			slots[i].pass_uses_closed = true;
			if (slots[i].state != LeaseState::EMPTY && slots[i].state != LeaseState::RELEASED) {
				slots[i].state = LeaseState::RETIRING;
			}
		}
	}
	bool admits_work() const {
		return state == GenerationState::CONFIGURED || state == GenerationState::ACTIVE;
	}
	bool has_live_slots() const {
		for (uint32_t i = 0; i < slot_count; ++i) {
			if (slots[i].state != LeaseState::EMPTY && slots[i].state != LeaseState::RELEASED) {
				return true;
			}
		}
		return false;
	}
};
} // namespace RendererRD
