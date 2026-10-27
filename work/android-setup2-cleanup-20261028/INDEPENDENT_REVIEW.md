# Read-only review: Android setup2 failure cleanup

Review scope: `main/main.cpp`, `main/main.h`, `platform/android/java_godot_lib_jni.cpp`, and the focused source contract.

Findings:

- The Android caller still invokes `Main::cleanup()` from `_terminate()`; the new branch is inside `Main::cleanup()`, so no JNI-side duplicate cleanup was introduced.
- The branch is restricted to the existing `Setup2ExtensionFailure::SCENE` / `EDITOR` markers. It does not reinterpret generic setup2 return values or change the existing display-driver partial cleanup.
- The extracted helper retains the pre-existing setup error sequence: cancel pending SceneReady, run G1 preflight, clean the failed extension level before dependent servers, deinitialize guarded core, delete core-owned objects, finalize core, and release the main-thread marker.
- The successful `Main::cleanup()` path is unchanged apart from an early failure-only dispatch.
- No authority or qualification claim is created. Android runtime and cross-compilation remain unverified due the missing NDK.

Disposition: limited-scope accept for source/build/regression evidence; Android runtime verification remains open.
