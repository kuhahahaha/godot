/**************************************************************************/
/*  gdextension_host_transform.cpp                                        */
/**************************************************************************/

#include "core/extension/gdextension_production_request.h"

#include <cmath>
#include <limits>

namespace GDExtensionDeviceExit {
// Six signed triple products of F32 inputs. A product of two F32 values
// is exact in double; FMA retains the rounding residual of the third factor.
// F32 triple exponents fit the normal double range, including subnormals.
static double exact_host_determinant(const float (&affine)[12]) {
	double expansion[12]{};
	uint32_t count = 0;
	auto grow = [&](double value) {
		double next[12]{};
		uint32_t next_count = 0;
		for (uint32_t i = 0; i < count; ++i) {
			const double sum = value + expansion[i];
			const double b_virtual = sum - value;
			const double a_virtual = sum - b_virtual;
			const double residual = (value - a_virtual) + (expansion[i] - b_virtual);
			if (residual != 0) {
				next[next_count++] = residual;
			}
			value = sum;
		}
		if (value != 0 || !next_count) {
			next[next_count++] = value;
		}
		count = next_count;
		for (uint32_t i = 0; i < count; ++i) {
			expansion[i] = next[i];
		}
	};
	auto triple = [&](uint32_t a, uint32_t b, uint32_t c, double sign) {
		const double pair = double(affine[a]) * double(affine[b]);
		const double product = pair * double(affine[c]);
		const double residual = std::fma(pair, double(affine[c]), -product);
		grow(sign * residual);
		grow(sign * product);
	};
	triple(0, 5, 10, 1);
	triple(1, 6, 8, 1);
	triple(2, 4, 9, 1);
	triple(2, 5, 8, -1);
	triple(1, 4, 10, -1);
	triple(0, 6, 9, -1);
	// A zero-eliminating expansion is exactly zero iff its highest component
	// is zero. Sum from small to large only after that exact classification.
	if (expansion[count - 1] == 0) {
		return 0;
	}
	double determinant = 0;
	for (uint32_t i = 0; i < count; ++i) {
		determinant += expansion[i];
	}
	return determinant;
}

Error production_host_transform(const GDExtensionProductionHostTransformInputV1 &input, GDExtensionProductionHostTransformV1 &out) {
	if (!production_dto_valid(input)) {
		return ERR_INVALID_PARAMETER;
	}
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 4; ++column) {
			if (!Math::is_finite(input.affine[row * 4 + column])) {
				return ERR_INVALID_DATA;
			}
		}
	}
	// F32 inputs can have a finite F32 inverse even when the determinant
	// overflows or underflows real_t. Widen all intermediate products.
	double basis[3][3];
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			basis[row][column] = input.affine[row * 4 + column];
		}
	}
	double inverse[3][4]{};
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			inverse[column][row] = basis[(row + 1) % 3][(column + 1) % 3] * basis[(row + 2) % 3][(column + 2) % 3] - basis[(row + 1) % 3][(column + 2) % 3] * basis[(row + 2) % 3][(column + 1) % 3];
		}
	}
	const double determinant = exact_host_determinant(input.affine);
	if (!Math::is_finite(determinant) || determinant == 0) {
		return ERR_INVALID_DATA;
	}
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 3; ++column) {
			inverse[row][column] /= determinant;
		}
		// Form Cramer's numerator before dividing, preserving cancellation in
		// the original F32 products instead of summing rounded inverse entries.
		float numerator_affine[12];
		for (uint32_t i = 0; i < 12; ++i) {
			numerator_affine[i] = input.affine[i];
		}
		for (uint32_t column = 0; column < 3; ++column) {
			numerator_affine[column * 4 + row] = input.affine[column * 4 + 3];
		}
		inverse[row][3] = -exact_host_determinant(numerator_affine) / determinant;
	}
	GDExtensionProductionHostTransformV1 sample{};
	sample.abi = production_dto_abi<GDExtensionProductionHostTransformV1>();
	sample.host_token = input.host_token;
	for (uint32_t row = 0; row < 3; ++row) {
		for (uint32_t column = 0; column < 4; ++column) {
			sample.affine[row * 4 + column] = input.affine[row * 4 + column];
			if (!Math::is_finite(inverse[row][column]) || Math::abs(inverse[row][column]) > double(std::numeric_limits<float>::max())) {
				return ERR_INVALID_DATA;
			}
			sample.inverse[row * 4 + column] = float(inverse[row][column]);
			if (!Math::is_finite(sample.inverse[row * 4 + column])) {
				return ERR_INVALID_DATA;
			}
		}
	}
	out = sample;
	return OK;
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_host_transform(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, const GDExtensionProductionHostTransformInputV1 *input) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionHostTransformInputV1)) {
		return status(ERR_INVALID_PARAMETER);
	}
	GDExtensionProductionHostTransformV1 sample{};
	Error error = production_host_transform(*input, sample);
	if (error != OK) {
		return status(error);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	error = production_request_locked(ref, true, request, device);
	if (error != OK) {
		return status(error);
	}
	const auto *entry = RegistrationContext::current;
	if (is_production_metadata_entry() || !entry || !entry->admitted || !entry->reference || entry->reference->registry != this || entry->reference->slot != request->attachment->library) {
		return status(ERR_UNAUTHORIZED);
	}
	if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost) {
		return status(ERR_INVALID_DATA);
	}
	if (request->cancelled || request->release_started || request->frozen || request->append_effect_entered || request->effects_pending || request->continuation_inflight || request->control.state != ProductionControlState::OWNER_CONSUMED) {
		return status(ERR_BUSY);
	}
	auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
		return status(ERR_INVALID_DATA);
	}
	if (action.preinput_host_returned) {
		return status(ERR_BUSY);
	}
	if (action.preinput_host_prepared) {
		return status(memcmp(&action.preinput_host_transform, &sample, sizeof(sample)) == 0 ? OK : ERR_INVALID_DATA);
	}
	if (action.prepared || action.output_state != ProductionActionCell::OutputState::UNBOUND) {
		return status(ERR_BUSY);
	}
	// Runtime's original HostControlService authenticates and pins its source.
	// This immutable value copy issues neither that authority nor a view grant.
	action.preinput_host_transform = sample;
	action.preinput_host_prepared = true;
	return status(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_prepare_output_scene(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, const GDExtensionProductionOutputSceneInputV1 *input) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	if (!input || uintptr_t(input) % alignof(GDExtensionProductionOutputSceneInputV1) || !production_dto_valid(*input) ||
			!input->instance || !input->particles || !input->generation || !input->scenario) { return status(ERR_INVALID_PARAMETER); }
	GDExtensionProductionHostTransformV1 host{};
	Error error = production_host_transform(input->host, host);
	if (error != OK) { return status(error); }
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	error = production_request_locked(ref, true, request, device);
	if (error != OK) { return status(error); }
	const auto *entry = RegistrationContext::current;
	if (is_production_metadata_entry() || !entry || !entry->admitted || !entry->reference || entry->reference->registry != this || entry->reference->slot != request->attachment->library) { return status(ERR_UNAUTHORIZED); }
	if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost) { return status(ERR_INVALID_DATA); }
	if (request->cancelled || request->release_started || request->frozen || request->append_effect_entered || request->effects_pending || request->continuation_inflight || request->control.state != ProductionControlState::OWNER_CONSUMED) { return status(ERR_BUSY); }
	auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) { return status(ERR_INVALID_DATA); }
	if (!action.preinput_host_prepared || action.preinput_host_returned) { return status(ERR_UNCONFIGURED); }
	if (memcmp(&action.preinput_host_transform, &host, sizeof(host))) { return status(ERR_INVALID_DATA); }
	if (action.output_scene_prepared) { return status(memcmp(&action.output_scene, input, sizeof(*input)) ? ERR_INVALID_DATA : OK); }
	if (action.prepared || action.output_state != ProductionActionCell::OutputState::UNBOUND) { return status(ERR_BUSY); }
	// Same trust boundary as prepare_host_transform: original E09 checks and
	// retains the semantic source. Core binds only physical values here, and
	// checks the actual committed instance later in the original RT operation.
	action.output_scene = *input;
	action.output_scene_prepared = true;
	return status(OK);
}

GDExtensionDeviceSessionResultV1 Registry::production_read_host_transform(GDExtensionProductionRequestRefV1 ref, uint64_t ordinal, GDExtensionProductionHostTransformV1 *out) {
	auto status = [](Error e) -> GDExtensionDeviceSessionResultV1 { return { int32_t(e), e == OK ? 0u : 16u, 0, 0 }; };
	if (!out || uintptr_t(out) % alignof(GDExtensionProductionHostTransformV1)) {
		return status(ERR_INVALID_PARAMETER);
	}
	Lock lock(*this);
	ProductionRequest *request = nullptr;
	RenderingDevice *device = nullptr;
	const Error error = production_request_locked(ref, false, request, device);
	if (error != OK) {
		return status(error);
	}
	if (request->contract_version != 2 || ordinal >= request->action_count || request->source_device_lost) {
		return status(ERR_INVALID_DATA);
	}
	const auto &action = request->actions[ordinal];
	if (action.request != request || action.declaration.action_ordinal != ordinal || action.declaration.action_kind != GDEXTENSION_PRODUCTION_NATIVE_OUTPUT_V1) {
		return status(ERR_INVALID_DATA);
	}
	if (!action.preinput_host_prepared || action.preinput_host_returned) {
		return status(ERR_UNAVAILABLE);
	}
	*out = action.preinput_host_transform;
	return status(OK);
}
} // namespace GDExtensionDeviceExit
