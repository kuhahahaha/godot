#pragma once

#ifdef TESTS_ENABLED
#include "core/error/error_list.h"

namespace TestDeviceExitStartup {
// Explicit isolated child options only. Ordinary tests and production have no
// injected failures; this fixture never manufactures successful native facts.
void configure(int p_argc, char **p_argv);
Error consume_begin_error();
void mark(const char *p_event, int p_value = 0);
void test_body_entered();
void test_cleanup_entered();
bool is_fixture_argument(const char *p_argument);
void normal_setup_returned(Error p_error);
void normal_start_entered();
bool observe_configuration_alert(const char *p_message);
bool instance_exhaustion_requested();
bool core_cycle_requested(int p_argc, char **p_argv);
int run_core_cycle();
void replace_core_extension_manager();
}
#endif
