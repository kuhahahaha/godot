#!/usr/bin/env python3
"""Read-only source-order regression check; does not execute a host or shader."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
sys.dont_write_bytecode = True
import run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source-root', required=True, type=Path)
    args = parser.parse_args()
    files = {}
    checks = []

    def source(path):
        if path not in files:
            data = (args.source_root / path).read_bytes()
            files[path] = {'sha256': hashlib.sha256(data).hexdigest(), 'bytes': len(data), 'text': data.decode()}
        return files[path]['text']

    def anchor(path, needle, start=0):
        text = source(path)
        offset = text.index(needle, start)
        return {'path': path, 'line': text.count('\n', 0, offset) + 1, 'source': needle}, offset

    def ordered(name, path, needles, start_needle):
        text = source(path)
        start = text.index(start_needle)
        prior = start
        evidence = []
        for needle in needles:
            item, current = anchor(path, needle, prior)
            evidence.append(item)
            prior = current + len(needle)
        checks.append({'check': name, 'status': 'passed_source_order', 'anchors': evidence})

    ordered('script attachment precedes synchronous SceneReady dispatch', 'main/main.cpp',
            ['script_loop->set_script(script_res);', 'GDExtensionManager::get_singleton()->scene_materialized();'], 'int Main::start()')
    ordered('Object script attachment directly creates script instance', 'core/object/object.cpp',
            ['created = s->instance_create(this);'], 'void Object::set_script(')
    ordered('GDScript instance_create passes nonnegative zero argument count', 'modules/gdscript/gdscript.cpp',
            ['return _create_instance(nullptr, 0, p_this, unchecked_error);'], 'ScriptInstance *GDScript::instance_create(')
    ordered('nonnegative instance path calls selected _init synchronously before returning', 'modules/gdscript/gdscript.cpp',
            ['if (p_argcount < 0)', 'GDScriptFunction *applicable_initializer = _super_constructor(this);',
             'applicable_initializer->call(instance, p_args, p_argcount, r_error);', '//@TODO make thread safe', 'return instance;'], 'GDScriptInstance *GDScript::_create_instance(')
    ordered('compiler recognizes _init and installs its initializer function', 'modules/gdscript/gdscript_compiler.cpp',
            ['p_func->identifier->name == GDScriptLanguage::get_singleton()->strings._init',
             'if (is_initializer)', 'p_script->initializer = gd_function;'], 'bool is_initializer =')
    ordered('manager dispatches SceneReady synchronously', 'core/extension/gdextension_manager.cpp',
            ['item.extension->dispatch_scene_ready_if_materialized();'], 'Error GDExtensionManager::scene_materialized()')
    ordered('SceneReady wrapper directly enters original Registry', 'core/extension/gdextension_scene_ready.cpp',
            ['registry.dispatch_scene_ready(library_exit_ref);'], 'Error GDExtension::dispatch_scene_ready_if_materialized()')
    ordered('Registry directly invokes native callback before returning', 'core/extension/gdextension_guard_enrollment.cpp',
            ['callback_v2 ? callback_v2(userdata, ready, &plan) : callback(userdata, ready);'], 'Error Registry::dispatch_scene_ready(')
    ordered('macOS MainLoop initialization occurs only after Main::start returns', 'platform/macos/os_macos.mm',
            ['ret = Main::start();', 'main_loop->initialize();'], 'void OS_MacOS_NSApp::start_main()')
    ordered('SceneTree initialize enters MainLoop initialize', 'scene/main/scene_tree.cpp',
            ['MainLoop::initialize();'], 'void SceneTree::initialize()')
    ordered('MainLoop initialization invokes script _initialize', 'core/os/main_loop.cpp',
            ['GDVIRTUAL_CALL(_initialize);'], 'void MainLoop::initialize()')
    probe = Path(__file__).with_name('probe.gd')
    run.validate_probe_constructor(probe.read_text())
    result = {'schema': 1, 'status': 'passed_static_source_order_only', 'source_root': str(args.source_root.resolve()),
              'checks': checks, 'probe': {'path': str(probe), 'sha256': run.sha(probe)},
              'source_files': {path: {key: value for key, value in item.items() if key != 'text'} for path, item in files.items()},
              'host_executed': False, 'gpu_executed': False, 'engine_built': False,
              'limitation': 'Read-only source call/order check; GDScript parsing and actual new-host runtime remain pending.'}
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
