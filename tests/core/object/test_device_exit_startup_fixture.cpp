#include "device_exit_startup_fixture.h"
#include "tests/test_macros.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

TEST_FORCE_LINK(test_device_exit_startup_fixture)

namespace TestDeviceExitStartup {
static bool enabled = false;
static bool consumed = false;
static Error requested = OK;
static bool configuration_failure = false;
static bool instance_exhaustion = false;

void configure(int p_argc, char **p_argv) {
	bool child = false;
	int selected = -1;
	for (int i = 0; i < p_argc; i++) {
		if (std::strcmp(p_argv[i], "--") == 0 || std::strcmp(p_argv[i], "++") == 0) {
			break;
		}
		child |= std::strcmp(p_argv[i], "--device-exit-startup-child") == 0;
		configuration_failure |= std::strcmp(p_argv[i], "--device-exit-config-failure-child") == 0;
		instance_exhaustion |= std::strcmp(p_argv[i], "--device-exit-instance-exhaustion-child") == 0;
		if (std::strcmp(p_argv[i], "--device-exit-startup-error=44") == 0) {
			selected = ERR_BUSY;
		} else if (std::strcmp(p_argv[i], "--device-exit-startup-error=5") == 0) {
			selected = ERR_PARAMETER_RANGE_ERROR;
		} else if (std::strcmp(p_argv[i], "--device-exit-startup-error=0") == 0) {
			selected = OK;
		}
	}
	enabled = child && selected >= 0;
	requested = enabled ? Error(selected) : OK;
}

bool instance_exhaustion_requested() {
	return instance_exhaustion;
}

bool is_fixture_argument(const char *p_argument) {
	return std::strcmp(p_argument, "--device-exit-startup-child") == 0 ||
			std::strcmp(p_argument, "--device-exit-config-failure-child") == 0 ||
			std::strcmp(p_argument, "--device-exit-startup-error=0") == 0 ||
			std::strcmp(p_argument, "--device-exit-startup-error=5") == 0 ||
			std::strcmp(p_argument, "--device-exit-startup-error=44") == 0;
}

void normal_setup_returned(Error p_error) {
	mark("NORMAL_CALLER_RETURN", p_error);
	if (enabled && configuration_failure && p_error == OK) {
		// Catch the wrong return at the real caller before Main::start can access
		// services already destroyed by setup's error unwind.
		std::_Exit(92);
	}
}

void normal_start_entered() {
	mark("NORMAL_START");
	if (enabled && configuration_failure) {
		std::_Exit(93);
	}
}

bool observe_configuration_alert(const char *p_message) {
	if (!enabled || !configuration_failure) {
		return false;
	}
	mark("NATIVE_ALERT");
	std::fprintf(stderr, "DEVICE_EXIT_NATIVE_ALERT %s\n", p_message);
	std::fflush(stderr);
	return true;
}

void mark(const char *p_event, int p_value) {
	if (enabled) {
		std::fprintf(stderr, "DEVICE_EXIT_STARTUP %s value=%d\n", p_event, p_value);
		std::fflush(stderr);
	}
}

Error consume_begin_error() {
	if (!enabled || consumed) {
		return OK;
	}
	consumed = true;
	mark("REGISTRY_BEGIN", requested);
	return requested;
}

void test_body_entered() {
	mark("TEST_BODY");
	if (enabled && consumed && requested != OK) {
		// A negative-control caller that ignores test_setup's failure must not
		// execute against a partial host. This is detection, never a passing exit.
		std::_Exit(90);
	}
}

void test_cleanup_entered() {
	mark("TEST_CLEANUP");
	if (enabled && consumed && requested != OK) {
		std::_Exit(91);
	}
}
}
