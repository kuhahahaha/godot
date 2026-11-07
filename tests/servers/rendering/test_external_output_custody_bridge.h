/**************************************************************************/
/*  test_external_output_custody_bridge.h                                 */
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
#include "servers/rendering/renderer_rd/storage_rd/material_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"

namespace RendererRD {
class ParticlesStorage;
class ExternalOutputOwner;
// Explicit private component setup only. Pins are still issued by the original
// protected RECORD; this helper creates no source grant, completion, or admission.
class ExternalLinkedOwnerTest {
	ParticlesStorage *storage = nullptr;
	ExternalOutputOwner *owner = nullptr;
	RID particles;

public:
	explicit ExternalLinkedOwnerTest(RenderingDevice &, bool p_query_enabled = false);
	~ExternalLinkedOwnerTest();
	GDExtensionDeviceExit::ProductionOutputPin &pin(uint32_t);
	void install(uint32_t, GDExtensionDeviceExit::Registry &, GDExtensionProductionRequestRefV1);
	// Independent generation substrate only; never a public Create/source issuer.
	Error install_generation(GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, GDExtensionDeviceExit::Registry &);
	Error install_generation_at(GDExtensionDeviceExit::ProductionPhysicalContextRef, GDExtensionProductionRequestRefV1, GDExtensionDeviceExit::Registry &, RID);
	GDExtensionDeviceExit::ProductionOutputPin &generation_pin();
	uint64_t generation_serial() const;
	bool generation_uninstalled() const;
	Error prepare_generation_directory();
	bool generation_directory_empty() const;
	GDExtensionDeviceExit::ProductionOutputPin &generation_directory_pin();
	void withdraw_generation();
	RID particle_rid() const { return particles; }
	static constexpr uint64_t generation = 33;
	static uint64_t lease(uint32_t slot) { return 11 + slot; }
	void prepare_lease_tokens();
	void copy_lease_token(uint32_t, uint8_t *) const;
	bool slot_reserved(uint32_t) const;
	bool withdrawing() const;
	bool view_open() const;
	void install_geometry(MeshStorage::NativeSurfaceHold &, uint64_t);
	bool geometry_group_returned() const;
	bool geometry_active() const;
	void prepare_storage(uint32_t, uint32_t);
	Error allocate_storage(GDExtensionProductionPhysicalContextRefV1);
	uint64_t storage_group() const;
	RID storage_rid(uint32_t) const;
	uint32_t storage_bytes(uint32_t) const;
	uint64_t storage_total_bytes() const;
	NativeDisposalWitness &storage_witness(uint32_t);
	bool storage_tokens_empty() const;
	bool storage_empty() const;
	void discard_unattempted_storage();
	void install_pipeline(uint64_t);
	RID &pipeline_root();
	bool pipeline_returned() const;
	MaterialStorage::NativeMaterialHold &material_hold();
	void install_material(uint64_t);
	bool material_active() const;
	void withdraw();
	bool history_cleared() const;
	bool released() const;
};
// Actual native mismatch injection and exact test-only undo, on the original RT.
Error external_storage_poison_and_free(RenderingDevice &, NativeDisposalWitness &, NativeDisposalWitness::Report &);
Error external_storage_restore_poison(RenderingDevice &, NativeDisposalWitness &, const NativeDisposalWitness::Report &);
// Opaque original MaterialStorage/ShaderRD test profile; creates no admission.
void *pipeline_material_profile_create(RID);
Error pipeline_material_profile_acquire(void *, RID, uint32_t, MaterialStorage::NativeMaterialHold &);
Error pipeline_material_profile_capture(GDExtensionProductionPhysicalContextRefV1, GDExtensionDeviceExit::ProductionOutputPin &, MaterialStorage::NativeMaterialHold &, uint64_t &);
void pipeline_material_profile_destroy(void *);
enum class ExternalCustodyTestMode { VIEW_RETURN,
	READ_CANCEL,
	READ_UNKNOWN,
	SLOT_PENDING,
	SLOT_RETURN,
	SURFACE_RETURN };
// Receives only a pin already issued by the original Registry/context fixture.
// Test wiring neither creates a source grant nor exercises an absent issuer.
void external_custody_issued_pin_test(GDExtensionDeviceExit::Registry &, GDExtensionProductionRequestRefV1,
		GDExtensionDeviceExit::ProductionOutputPin &, ExternalCustodyTestMode,
		MeshStorage::NativeSurfaceHold * = nullptr);
} //namespace RendererRD
