#pragma once

#include "core/math/transform_3d.h"
#include "core/math/vector3.h"
#include "core/object/ref_counted.h"
#include "core/os/mutex.h"
#include "core/variant/dictionary.h"
#include "servers/rendering/renderer_rd/storage_rd/height_frame.h"

class RenderingDevice;
class RenderGeometryInstance;

class ParticlesHeightfieldClearTestFixture : public RefCounted {
	GDCLASS(ParticlesHeightfieldClearTestFixture, RefCounted)
	Dictionary result_data;
	mutable Mutex result_mutex;
	bool started = false;
	bool frame_resources_capture = false;
	bool quoted_resources_capture = false;
	bool candidate_capture = false;
	Ref<RendererRD::ParticlesStorage::HeightFrame> candidate_frame;
	Ref<RendererRD::ParticlesStorage::HeightFrame> rejected_candidate;
	Ref<RendererRD::ParticlesStorage::HeightFrame> postbind_candidate;
	bool frame_resources_sampled = false;
	bool frame_resources_poll_pending = false;
	Error frame_resources_error = OK;
	Ref<RendererRD::ParticlesStorage::HeightFrameResources> frame_resources_a;
	Ref<RendererRD::ParticlesStorage::HeightFrameResources> frame_resources_b;
	Ref<RendererRD::ParticlesStorage::HeightFrameResources> retained_frame_resources_a;
	bool geometry_capture = false;
	bool transformed_capture = false;
	bool legacy_normal_capture = false;
	int legacy_transform_case = 0;
	RID legacy_particle;
	RID legacy_shader;
	RID legacy_material;
	int legacy_neighbor_case = -1;
	RID legacy_texture;
	Vector3 capture_scale = Vector3(1, 1, 1);
	float geometry_height = 0;
	float capture_extents_y = 1.0f;
	float capture_transform_y = 0.0f;
	RID mesh;
	RenderGeometryInstance *geometry = nullptr;
	RID collision;
	RID first_instance;
	RID second_instance;
	RID shader;
	RID pipeline;
	RID buffer;
	RID sampler;
	RID uniform_set;
	RID first_texture;
	RID second_texture;
	RenderingDevice *rd = nullptr;
	Error run_capture();
	Error sample_depth_pair();
	Error capture_candidate();
	void retire_frame_resources();
	void cleanup();
	void sample_done(const PackedByteArray &p_bytes);
	Error run_legacy_process(const Transform3D &p_field_transform);
	Error run_neighbor_setup();
	void legacy_sample_done(const PackedByteArray &p_bytes);

protected:
	static void _bind_methods();

public:
	Error run();
	Error run_frame_resources();
	Error run_candidate();
	void poll_frame_resources();
	Error run_legacy_normal(int p_transform_case = 0);
	Error run_legacy_neighbor(int p_case);
	Error run_geometry(float p_height);
	Error run_transformed(const Vector3 &p_scale);
	Error run_geometry_params(float p_height, float p_extents_y, float p_transform_y);
	Dictionary result() const;
};
