/**************************************************************************/
/*  gdextension_camera_ready.h                                            */
/**************************************************************************/

#pragma once

#include "core/extension/gdextension_library_exit_guard.h"
#include "servers/rendering/renderer_viewport.h"

namespace GDExtensionDeviceExit {
// Non-owning private bridge around an actual renderer stack scope. Constructing
// it grants no authority: every use validates the original live scope again.
class ProductionCameraInvocationBorrow {
	friend class ProductionCameraReadySubscriber;
	const RendererViewport::CameraInvocationScope &scope;
	explicit ProductionCameraInvocationBorrow(const RendererViewport::CameraInvocationScope &p_scope) : scope(p_scope) {}

public:
	ProductionCameraInvocationBorrow() = delete;
	ProductionCameraInvocationBorrow(const ProductionCameraInvocationBorrow &) = delete;
	ProductionCameraInvocationBorrow &operator=(const ProductionCameraInvocationBorrow &) = delete;
	const RendererViewport::CameraInvocationScope &get_scope() const { return scope; }
};

// Legacy attachment owner. RefCounted compatibility does not pay for the
// request facet: each request's subscriber is embedded in its original arena.
class ProductionCameraReadyObserver final : public RendererViewport::Viewport::CameraSourceObserver {
	friend class Registry;
	Registry &registry;
	const GDExtensionProductionAttachmentRefV1 attachment;
	Ref<RendererViewport::Viewport::CameraCapture> delivery;
	Error last_error = ERR_UNAVAILABLE;
	bool closed = false;

public:
	ProductionCameraReadyObserver(Registry &p_registry, GDExtensionProductionAttachmentRefV1 p_attachment) : registry(p_registry), attachment(p_attachment) {}
	void camera_source_retained(const Ref<RendererViewport::Viewport::CameraCapture> &p_capture) override;
	void camera_source_closed() override;
	Error get_last_error() const { return last_error; }
	bool is_closed() const { return closed; }
};

// Plain native facet, constructed with the original paid ActionCell. The
// locator is initialized once at bind and unchanged until RT RELEASE unlinks
// every subscription/pending delivery; only then may the arena be reclaimed.
class ProductionCameraReadySubscriber final : public RendererViewport::Viewport::CameraSourceSubscriber {
	friend class Registry;
#ifdef TESTS_ENABLED
	friend struct ::TestProductionNativeObserve::Access;
#endif
	Registry *registry = nullptr;
	GDExtensionProductionAttachmentRefV1 attachment{};
	GDExtensionProductionRequestRefV1 request{};
	uint64_t action_ordinal = UINT64_MAX;
	Ref<RendererViewport::Viewport::CameraCapture> delivery;
	Error last_error = ERR_UNAVAILABLE;
	bool closed = false, subscribed = false, released = false, delivery_pinned = false;

public:
	ProductionCameraReadySubscriber() = default;
	bool camera_source_has_native_executor() const override { return true; }
	void camera_source_native_invocation(const RendererViewport::CameraInvocationScope &) override;
	void camera_source_retained(const Ref<RendererViewport::Viewport::CameraCapture> &p_capture) override;
	void camera_source_closed() override;
	Error get_last_error() const { return last_error; }
	bool is_closed() const { return closed; }
};
} // namespace GDExtensionDeviceExit
