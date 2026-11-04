/**************************************************************************/
/*  test_system_scalar.cpp                                                */
/**************************************************************************/

#include "core/extension/gdextension_qualification_internal.h"
#include "tests/servers/rendering/transfer_test_driver_base.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_system_scalar)

namespace TestSystemScalar {
using namespace GDExtensionDeviceExit;

struct CapacityDriver : TransferTestDriverBase {
	mutable uint32_t main_slots = 0, worker_slots = 0, frames = 0, calls = 0;
	uint32_t bound = 0;
	uint32_t qualification_native_call_bound_for_profile(uint32_t p_main, uint32_t p_worker, uint32_t p_frames) const override {
		main_slots = p_main;
		worker_slots = p_worker;
		frames = p_frames;
		++calls;
		return bound;
	}
};

TEST_CASE("[SystemScalar] Unknown append retains possible ownership even without native members") {
	QualificationTicket ticket(true);
	ticket.recording = true;
	ticket.body_append.state = CheckedRecordAppend::State::Unknown;
	ticket.body_append.error = FAILED;
	ticket.finish_system_append(FAILED);
	CHECK(ticket.mapping_failed);
	CHECK(ticket.observation_dirty);
	CHECK(ticket.append_failed);
	CHECK_FALSE(ticket.recording);
	CHECK_FALSE(ticket.physically_retired.load());
	CHECK(ticket.natives.is_empty());
	CHECK(ticket.graphs.is_empty());
	CHECK_FALSE(ticket.body_returned_successfully());
	CHECK(ticket.report.native_error == FAILED);
	// Observation/retry cannot erase the unknown mapping or its first cause.
	ticket.finish_system_append(ERR_BUSY);
	CHECK(ticket.mapping_failed);
	CHECK(ticket.report.native_error == FAILED);
}

TEST_CASE("[SystemScalar] known rejected append remains distinguishable from Unknown") {
	QualificationTicket ticket(true);
	ticket.recording = true;
	ticket.body_append.state = CheckedRecordAppend::State::Rejected;
	ticket.finish_system_append(ERR_BUSY);
	CHECK_FALSE(ticket.mapping_failed);
	CHECK(ticket.append_failed);
	CHECK(ticket.report.native_error == ERR_BUSY);
}

TEST_CASE("[SystemScalar] native admission includes every copy and continuation worker source") {
	CapacityDriver driver;
	// 36 bytes: two copies plus dispatch = three graphs, four possible frames.
	// Seven current commands + four main slots; two workers in all four frames.
	driver.bound = 4093;
	CHECK(system_scalar_native_capacity(driver, 7, 2, 36, 32) == OK);
	CHECK(driver.main_slots == 11);
	CHECK(driver.worker_slots == 8);
	CHECK(driver.frames == 4);
	driver.bound = 4094;
	CHECK(system_scalar_native_capacity(driver, 7, 2, 36, 32) == ERR_PARAMETER_RANGE_ERROR);
	driver.bound = 0;
	CHECK(system_scalar_native_capacity(driver, 7, 2, 36, 32) == ERR_UNAVAILABLE);
	const auto calls = driver.calls;
	CHECK(system_scalar_native_capacity(driver, 7, 2, 36, 16) == ERR_UNAVAILABLE);
	CHECK(system_scalar_native_capacity(driver, UINT32_MAX, 2, 36, 32) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(system_scalar_native_capacity(driver, 7, 2, 0, 32) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(system_scalar_native_capacity(driver, 7, 2, 16385, 32) == ERR_PARAMETER_RANGE_ERROR);
	CHECK(driver.calls == calls);
	// Largest admitted output: 512 copies + dispatch. No guessed driver cost.
	driver.bound = 3583;
	CHECK(system_scalar_native_capacity(driver, 0, 0, 16384, 32) == OK);
	driver.bound = 3584;
	CHECK(system_scalar_native_capacity(driver, 0, 0, 16384, 32) == ERR_PARAMETER_RANGE_ERROR);
}
TEST_CASE("[SystemScalar] set retirement keeps first error without changing completed computation") {
	QualificationTicket ticket(true);
	ticket.system_set = RID::from_uint64(7);
	ticket.physically_retired.store(true);
	ticket.report.state = 8;
	ticket.report.revision = 23;
	ticket.report.native_code = -41;
	ticket.system_report.state = 8;
	ticket.system_report.revision = 23;
	ticket.system_report.native_code = -41;
	const auto report = ticket.report;
	const auto system_report = ticket.system_report;
	ticket.remember_system_set_return_error(ERR_BUSY);
	CHECK(ticket.system_set_return_error == OK);
	ticket.remember_system_set_return_error(FAILED);
	CHECK(ticket.system_set_return_error == FAILED);
	ticket.remember_system_set_return_error(ERR_INVALID_DATA);
	ticket.remember_system_set_return_error(OK);
	CHECK(ticket.system_set_return_error == FAILED);
	CHECK(memcmp(&ticket.report, &report, sizeof(report)) == 0);
	CHECK(memcmp(&ticket.system_report, &system_report, sizeof(system_report)) == 0);
	CHECK_FALSE(ticket.acknowledged);
}

TEST_CASE("[SystemScalar] set retirement error does not replace active-operation or absent-set outcomes") {
	QualificationTicket ticket(true);
	ticket.system_set = RID::from_uint64(7);
	ticket.remember_system_set_return_error(FAILED);
	CHECK(ticket.system_set_return_error == OK);
	ticket.physically_retired.store(true);
	ticket.system_set = RID();
	ticket.remember_system_set_return_error(FAILED);
	CHECK(ticket.system_set_return_error == OK);
	QualificationTicket qualification;
	qualification.physically_retired.store(true);
	qualification.system_set = RID::from_uint64(7);
	qualification.remember_system_set_return_error(FAILED);
	CHECK(qualification.system_set_return_error == OK);
}
} // namespace TestSystemScalar
