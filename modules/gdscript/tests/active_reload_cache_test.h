/**************************************************************************/
/*  active_reload_cache_test.h                                            */
/**************************************************************************/

#pragma once

#include "../gdscript_cache.h"

#include "core/io/file_access.h"
#include "core/io/resource_loader.h"
#include "tests/test_macros.h"
#include "tests/test_utils.h"

namespace GDScriptTests {

class ActiveReloadCacheTestAccessor {
public:
	static bool has_shallow(const String &p_path) {
		return GDScriptCache::singleton->shallow_gdscript_cache.has(p_path);
	}

	static bool has_full(const String &p_path) {
		return GDScriptCache::singleton->full_gdscript_cache.has(p_path);
	}
};

TEST_CASE("[Modules][GDScript][ActiveReload] failed update does not promote shallow candidate") {
	GDScriptLanguage::get_singleton()->init();
	const String path = TestUtils::get_temp_path("active_reload_cache_failure.gd");

	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("extends RefCounted\n\nfunc value():\n\treturn 7\n");
	}

	Error error = OK;
	Ref<GDScript> shallow = GDScriptCache::get_shallow_script(path, error);
	REQUIRE(error == OK);
	REQUIRE(shallow.is_valid());
	CHECK(ActiveReloadCacheTestAccessor::has_shallow(path));
	CHECK_FALSE(ActiveReloadCacheTestAccessor::has_full(path));
	CHECK_FALSE(ResourceCache::has(path));

	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("extends RefCounted\n\nfunc value(\n");
	}

	Ref<GDScript> failed = GDScriptCache::get_full_script(path, error, String(), true);
	CHECK(error == ERR_PARSE_ERROR);
	CHECK(failed == shallow);
	CHECK(ActiveReloadCacheTestAccessor::has_shallow(path));
	CHECK_FALSE(ActiveReloadCacheTestAccessor::has_full(path));
	CHECK_FALSE(ResourceCache::has(path));

	{
		Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE);
		REQUIRE(file.is_valid());
		file->store_string("extends RefCounted\n\nfunc value():\n\treturn 9\n");
	}

	Ref<GDScript> retried = GDScriptCache::get_full_script(path, error, String(), true);
	REQUIRE(error == OK);
	CHECK(retried == shallow);
	CHECK_FALSE(ActiveReloadCacheTestAccessor::has_shallow(path));
	CHECK(ActiveReloadCacheTestAccessor::has_full(path));
	CHECK(ResourceCache::has(path));
	Ref<RefCounted> owner = memnew(RefCounted);
	owner->set_script(retried);
	CHECK(owner->call("value") == Variant(9));

	GDScriptCache::remove_script(path);
}

} // namespace GDScriptTests
