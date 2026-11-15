#include "servers/rendering/rendering_device_graph.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"
TEST_FORCE_LINK(test_qualification_graph_order)
TEST_CASE("[R02][Qualification] only consumed synchronization with actual adjacency proves Capture to Body ordering") {
 TransferTestDriverBase driver;RenderingDeviceGraph graph;
 graph.initialize(&driver,[](RenderingDeviceDriver *,VectorView<RenderingDeviceDriver::AttachmentLoadOp>,VectorView<RenderingDeviceDriver::AttachmentStoreOp>,void *){return RenderingDeviceDriver::RenderPassID();},1,RenderingDeviceDriver::CommandQueueFamilyID(1),0);
 graph.begin();graph.add_compute_list_begin();
 const auto capture=graph.add_compute_list_dispatch(1,1,1);graph.add_compute_list_end();
 graph.add_compute_list_begin();const auto unordered=graph.add_compute_list_dispatch(1,1,1);graph.add_compute_list_end();
 CHECK_FALSE(graph.has_synchronization_edge(capture,unordered));
 graph.add_synchronization();graph.add_compute_list_begin();
 const auto body=graph.add_compute_list_dispatch(1,1,1);
 CHECK_FALSE(graph.has_synchronization_edge(capture,body));
 graph.add_compute_list_end();
 CHECK(graph.has_synchronization_edge(capture,body));
 CHECK_FALSE(graph.has_synchronization_edge(body,capture));
 auto foreign=capture;foreign.graph_instance++;
 CHECK_FALSE(graph.has_synchronization_edge(foreign,body));
 graph.begin();CHECK_FALSE(graph.has_synchronization_edge(capture,body));
}
