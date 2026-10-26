#include "tests/test_macros.h"

TEST_FORCE_LINK(test_editor_help_shutdown)

#ifdef TOOLS_ENABLED
#include "core/object/callable_mp.h"
#include "core/object/message_queue.h"
#include "editor/doc/editor_help.h"

#include <cstdio>
#include <cstdlib>

TEST_CASE("[EditorHelp][Shutdown] retired deferred generation cannot reach the next owner") {
	auto *queue = MessageQueue::get_main_singleton();
	REQUIRE_FALSE(queue->is_flushing());
	REQUIRE(EditorHelp::get_doc_data() == nullptr);
	REQUIRE_EQ(queue->flush(), OK);
	EditorHelp::begin_doc_lifetime();
	queue->push_callable(callable_mp_static(&EditorHelp::generate_doc).bind(false, false));
	REQUIRE(queue->has_messages());
	EditorHelp::finish_doc_generation();
	CHECK_FALSE(queue->has_messages());
	// Keep a failed control safe: drain while admission is still closed before
	// asking a new native owner to begin its lifetime.
	REQUIRE_EQ(queue->flush(), OK);
	EditorHelp::begin_doc_lifetime();
	CHECK(EditorHelp::get_doc_data() == nullptr);
}

TEST_CASE("[EditorHelp][Shutdown] late cache entry does not resurrect a closed owner") {
	REQUIRE(EditorHelp::get_doc_data() == nullptr);
	EditorHelp::finish_doc_generation();
	EditorHelp::generate_doc(false, false);
	EditorHelp::load_script_doc_cache();
	EditorHelp::regenerate_script_doc_cache();
	CHECK(EditorHelp::get_doc_data() == nullptr);
	EditorHelp::begin_doc_lifetime();
}

static void editor_help_reentrant_close_child() {
	auto *queue = MessageQueue::get_main_singleton();
	queue->push_callable(callable_mp_static(&EditorHelp::finish_doc_generation));
	std::fputs("EDITOR_HELP_REENTRANT_CLOSE_BEFORE\n", stderr);
	queue->flush();
	std::fputs("EDITOR_HELP_REENTRANT_CLOSE_UNEXPECTED_RETURN\n", stderr);
	std::_Exit(88);
}
REGISTER_TEST_COMMAND("--editor-help-reentrant-close", editor_help_reentrant_close_child);
#endif
