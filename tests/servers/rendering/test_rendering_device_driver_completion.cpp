/**************************************************************************/
/*  test_rendering_device_driver_completion.cpp                           */
/**************************************************************************/

#include "servers/rendering/rendering_device_driver.h"
#include "tests/test_macros.h"

TEST_FORCE_LINK(test_rendering_device_driver_completion)

TEST_CASE("[K03][P2] RenderingDeviceDriver exposes typed native completion facts") {
	RenderingDeviceDriver::QueueCallReport report;
	report.issued = true;
	report.submission_serial = 41;
	report.native_code = -7;
	RenderingDeviceDriver::NativeCallFact fact;
	fact.serial = 41;
	fact.issue = RenderingDeviceDriver::IssueFact::IssuedUnknown;
	fact.completion = RenderingDeviceDriver::CompletionCapability::Fence;
	report.calls.push_back(fact);
	CHECK(report.issued);
	CHECK(report.submission_serial == 41);
	CHECK(report.native_code == -7);
	REQUIRE(report.calls.size() == 1);
	CHECK(report.calls[0].issue == RenderingDeviceDriver::IssueFact::IssuedUnknown);
	CHECK(report.calls[0].completion == RenderingDeviceDriver::CompletionCapability::Fence);

	RenderingDeviceDriver::FencePollResult poll;
	poll.completed = true;
	poll.completed_serial = 41;
	poll.native_code = 0;
	CHECK(poll.completed);
	CHECK(poll.completed_serial == 41);
}

TEST_CASE("[K03][P2] default driver completion seam fails closed") {
	const RenderingDeviceDriver::FencePollResult poll = RenderingDeviceDriver::FencePollResult();
	CHECK_FALSE(poll.completed);
	CHECK(poll.completed_serial == 0);
}

TEST_CASE("[K03][P2] fence completion requires the operation serial") {
	RenderingDeviceDriver::FencePollResult poll;
	poll.completed = true;
	poll.completed_serial = 41;
	CHECK(poll.matches(41));
	CHECK_FALSE(poll.matches(40));
	poll.completed = false;
	CHECK_FALSE(poll.matches(41));
}

TEST_CASE("[K03][P2] native fence errors are not pending") {
	RenderingDeviceDriver::FencePollResult poll;
	poll.completed_serial = 41;
	poll.native_error = true;
	CHECK_FALSE(poll.matches(41));
	CHECK(poll.native_error);
}
