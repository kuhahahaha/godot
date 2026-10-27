from pathlib import Path

root = Path(__file__).resolve().parents[2]
main_h = (root / 'main/main.h').read_text()
main_cpp = (root / 'main/main.cpp').read_text()
jni = (root / 'platform/android/java_godot_lib_jni.cpp').read_text()
assert 'static void cleanup_setup_failure(Error p_error, const char *p_execpath = nullptr);' in main_h
assert 'void Main::cleanup_setup_failure(Error p_error, const char *p_execpath)' in main_cpp
assert 'cleanup_setup_failure(setup2_extension_error);' in main_cpp
assert 'if (setup2_extension_failure != Setup2ExtensionFailure::NONE)' in main_cpp
assert 'Main::cleanup();' in jni
