#include "servers/rendering/storage/mesh_storage.h"
#include "servers/rendering/renderer_rd/storage_rd/mesh_storage.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_multimesh_rebuild)

namespace RendererRD {
class TestMultimeshRebuildAccess {
public:
	static uint32_t upload_size(uint32_t p_regions, uint32_t p_region_bytes, uint32_t p_buffer_bytes) {
		return MeshStorage::_multimesh_dirty_upload_size(p_regions, p_region_bytes, p_buffer_bytes);
	}
};
} //namespace RendererRD

TEST_CASE("[B4][MultiMesh] rounded dirty uploads cannot wrap into a zero byte success") {
	using Access = RendererRD::TestMultimeshRebuildAccess;
	// A legal 134217727-instance 2D allocation is 4294967264 bytes.
	// Its 262144 rounded regions span exactly 2^32 bytes before clamping.
	CHECK(Access::upload_size(262144, 16384, 4294967264u) == 4294967264u);
	CHECK(Access::upload_size(2, 24576, 30000) == 30000);
	CHECK(Access::upload_size(1, 24576, 49152) == 24576);
	CHECK(Access::upload_size(0, 24576, 49152) == 0);
}

TEST_CASE("[B4][MultiMesh] allocation preflight bounds the complete buffer before mutation") {
	struct Case {
		int instances;
		RSE::MultimeshTransformFormat format;
		bool colors;
		bool custom;
		bool motion;
		Error error;
		uint32_t bytes;
	};
	const Case cases[] = {
		{ 0, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 0 },
		{ 3, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 96 },
		{ 3, RSE::MULTIMESH_TRANSFORM_2D, true, true, false, OK, 192 },
		{ 3, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, OK, 240 },
		{ 3, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, OK, 480 },
		{ -1, RSE::MULTIMESH_TRANSFORM_3D, false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 0, RSE::MultimeshTransformFormat(99), false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 134217727, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, OK, 4294967264u },
		{ 134217728, RSE::MULTIMESH_TRANSFORM_2D, false, false, false, ERR_INVALID_PARAMETER, 17 },
		{ 53687091, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, OK, 4294967280u },
		{ 53687092, RSE::MULTIMESH_TRANSFORM_3D, true, true, false, ERR_INVALID_PARAMETER, 17 },
		{ 26843545, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, OK, 4294967200u },
		{ 26843546, RSE::MULTIMESH_TRANSFORM_3D, true, true, true, ERR_INVALID_PARAMETER, 17 },
	};
	for (const Case &test : cases) {
		uint32_t bytes = 17;
		CHECK(RendererMeshStorage::_multimesh_buffer_size(test.instances, test.format, test.colors, test.custom, test.motion, bytes) == test.error);
		CHECK(bytes == test.bytes);
	}
}
