#pragma once
#include "p01_capture_codec.h"
#include <vector>

struct P01CaptureStartup { int descriptor = -1; P01CaptureIdentity identity; };
enum class P01StartupParse { Absent, Valid, Invalid };
P01StartupParse p01_parse_capture_startup(const std::vector<std::string> &p_args, P01CaptureStartup &r_startup);
// Original engine lifecycle only; not registered to ClassDB or GDExtension.
void p01_capture_startup();
void p01_capture_shutdown();
void p01_capture_publish(const P01HookEvent &p_event);
bool p01_capture_transport_available();

// Private child-bound one-shot admission; never exposed through ClassDB.
bool p01_capture_claim_child(const std::string &p_member, const std::string &p_route, uint64_t &r_epoch);
