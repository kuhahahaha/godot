#include "tests/test_macros.h"
#include "servers/rendering/rendering_device.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
TEST_FORCE_LINK(test_qualification_finalize_tail)
class TestQualificationFinalizeAccess {
public:
 static void attach(RenderingDevice &device, RenderingDeviceDriver &driver, RenderingContextDriver &context, const char *mode) {
  device.driver=&driver;device.context=&context;
  if(!std::strcmp(mode,"dependency"))device.dependency_map[RID::from_uint64(71)].insert(RID::from_uint64(72));
  if(!std::strcmp(mode,"reverse"))device.reverse_dependency_map[RID::from_uint64(72)].insert(RID::from_uint64(71));
 }
 static void finish(RenderingDevice &device) { device._finalize_on_rendering_server_thread(); }
 static bool returned(const RenderingDevice &device) { return device.native_finalization_returned; }
};
namespace TestQualificationFinalizeTail {
class Context : public RenderingContextDriver {
public:
 Error initialize() override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 const Device &device_get(uint32_t p_device_index) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 uint32_t device_get_count() const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 bool device_supports_present(uint32_t p_device_index, SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 RenderingDeviceDriver *driver_create() override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void driver_free(RenderingDeviceDriver *p_driver) override { std::fprintf(stderr,"QB1_DRIVER_FREE_RETURN\n"); std::fflush(stderr); }
 SurfaceID surface_create(const void *p_platform_data) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_set_size(SurfaceID p_surface, uint32_t p_width, uint32_t p_height) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_set_vsync_mode(SurfaceID p_surface, DisplayServerEnums::VSyncMode p_vsync_mode) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 DisplayServerEnums::VSyncMode surface_get_vsync_mode(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_set_hdr_output_enabled(SurfaceID p_surface, bool p_enabled) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 bool surface_get_hdr_output_enabled(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_set_hdr_output_reference_luminance(SurfaceID p_surface, float p_reference_luminance) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 float surface_get_hdr_output_reference_luminance(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_set_hdr_output_max_luminance(SurfaceID p_surface, float p_max_luminance) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 float surface_get_hdr_output_max_luminance(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_set_hdr_output_linear_luminance_scale(SurfaceID p_surface, float p_linear_luminance_scale) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 float surface_get_hdr_output_linear_luminance_scale(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 float surface_get_hdr_output_max_value(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 uint32_t surface_get_width(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 uint32_t surface_get_height(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_set_needs_resize(SurfaceID p_surface, bool p_needs_resize) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 bool surface_get_needs_resize(SurfaceID p_surface) const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 void surface_destroy(SurfaceID p_surface) override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
 bool is_debug_utils_enabled() const override { CRASH_NOW_MSG("Unexpected finalize-tail fixture context operation."); }
};
static void child() {
 const char *mode=std::getenv("GODOT_QB_FINALIZE_TAIL_CONTROL");
 if(!mode || (std::strcmp(mode,"clean") && std::strcmp(mode,"dependency") && std::strcmp(mode,"reverse")))std::_Exit(2);
 RenderingDevice device;TransferTestDriverBase driver;Context context;
 TestQualificationFinalizeAccess::attach(device,driver,context,mode);
 std::fprintf(stderr,"QB1_FINALIZE_ENTER mode=%s\n",mode);std::fflush(stderr);
 TestQualificationFinalizeAccess::finish(device);
 std::fprintf(stderr,"QB1_AFTER_FINALIZE returned=%d\n",int(TestQualificationFinalizeAccess::returned(device)));std::fflush(stderr);
 std::_Exit(!std::strcmp(mode,"clean") && TestQualificationFinalizeAccess::returned(device) ? 0 : 88);
}
REGISTER_TEST_COMMAND("--qualification-finalize-tail-child",child);
}
