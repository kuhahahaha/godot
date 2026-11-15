"""Original core controlled-build generator checks (no runtime authority)."""
import hashlib
import importlib.util
import pathlib
import struct
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("engine_build_identity", ROOT / "core/engine_build_identity.py")
identity = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(identity)


class CoreBuildIdentityTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temp.name)
        self.obj = self.root / "engine.o"
        self.archive = self.root / "core.a"
        self.obj.write_bytes(b"actual-object")
        self.archive.write_bytes(b"actual-archive")

    def tearDown(self):
        self.temp.cleanup()

    def snapshot(self, **changes):
        values = dict(platform="macos", architecture="arm64", target_kind="Executable",
                      inputs=[("Object", self.obj), ("StaticArchive", self.archive)],
                      arguments=["@compiler", "-o", "@output", "@input:0", "@input:1"],
                      options={"tests": True, "target": "editor"},
                      tools=[("compiler", b"clang-real"), ("linker", b"ld-real")],
                      configuration=[("version", "actual-toolchain-version")],
                      recipes=[("generator", b"original-generator")], builtin_glslang=True)
        values.update(changes)
        return identity.make_snapshot(**values)

    def test_c01_schema_digest_exact_binary_framing(self):
        domain = b"ange.vfx.core-build-inputs.v1"
        payload = b"\x00\x00\x00\x01\xff"
        framed = struct.pack(">I", len(domain)) + domain + struct.pack(">I", len(payload)) + payload
        self.assertEqual(identity.schema_digest(domain.decode(), payload), hashlib.sha256(framed).digest())

    def test_core_compiler_separate_domains_and_closed_selection(self):
        snapshot = self.snapshot()
        core, compiler = identity.derive_stamps(snapshot)
        self.assertEqual(len(core), 32)
        self.assertEqual(len(compiler), 32)
        self.assertNotEqual(core, compiler)
        expected = identity.schema_digest("ange.vfx.runtime-compiler-build.v1", struct.pack(">I", 1) + core + struct.pack(">I", 1))
        self.assertEqual(compiler, expected)
        selection = identity.selection_record(snapshot, core, compiler)
        self.assertEqual(set(selection), {"schema", "core_stamp", "compiler_route", "compiler_stamp", "platform", "architecture", "target_kind"})

    def test_ordinary_source_content_change_invalidates(self):
        first = identity.derive_stamps(self.snapshot())
        self.obj.write_bytes(b"changed-actual-object")
        self.assertNotEqual(first, identity.derive_stamps(self.snapshot()))

    def test_order_and_duplicate_linkage_preserved(self):
        first = identity.derive_stamps(self.snapshot())
        for inputs in [[("StaticArchive", self.archive), ("Object", self.obj)], [("Object", self.obj), ("StaticArchive", self.archive), ("StaticArchive", self.archive)]]:
            self.assertNotEqual(first, identity.derive_stamps(self.snapshot(inputs=inputs)))

    def test_link_option_tool_recipe_config_delta_invalidates(self):
        first = identity.derive_stamps(self.snapshot())
        for changes in [dict(arguments=["@compiler", "-Wl,-dead_strip"]), dict(options={"tests": False}),
                        dict(tools=[("compiler", b"new-compiler")]), dict(recipes=[("generator", b"new-recipe")]),
                        dict(configuration=[("version", "new-toolchain")])]:
            self.assertNotEqual(first, identity.derive_stamps(self.snapshot(**changes)))

    def test_diagnostic_input_relocation_excluded(self):
        obj = self.root / "relocated.o"
        archive = self.root / "relocated.a"
        obj.write_bytes(self.obj.read_bytes())
        archive.write_bytes(self.archive.read_bytes())
        self.assertEqual(identity.derive_stamps(self.snapshot()), identity.derive_stamps(self.snapshot(inputs=[("Object", obj), ("StaticArchive", archive)])))

    def test_missing_input_nonbuiltin_unknown_options_fail_closed(self):
        for changes in [dict(inputs=[("Object", self.root / "missing.o")]), dict(builtin_glslang=False),
                        dict(options={"opaque": object()}), dict(target_kind="Unknown")]:
            with self.assertRaises((ValueError, FileNotFoundError)):
                self.snapshot(**changes)

    def test_original_graph_excludes_only_stamp_and_never_final(self):
        with self.assertRaises(ValueError):
            identity.validate_graph([self.obj, self.archive], self.obj, self.root / "godot")
        with self.assertRaises(ValueError):
            identity.validate_graph([self.obj, self.archive], self.root / "stamp.o", self.archive)
        identity.validate_graph([self.obj, self.archive], self.root / "stamp.o", self.root / "godot")

    def test_target_kind_is_actual_module(self):
        values = {identity.derive_stamps(self.snapshot(target_kind=kind)) for kind in ["Executable", "SharedLibrary", "StaticLibrary"]}
        self.assertEqual(len(values), 3)

    def test_actual_ld_plan_includes_lto_framework_and_implicit_library(self):
        sdk = self.root / "sdk"
        (sdk / "usr/lib").mkdir(parents=True)
        (sdk / "usr/lib/libSystem.tbd").write_bytes(b"actual-System-stub")
        framework = sdk / "System/Library/Frameworks/Metal.framework"
        framework.mkdir(parents=True)
        (framework / "Metal.tbd").write_bytes(b"actual-Metal-stub")
        lto = self.root / "libLTO.dylib"
        lto.write_bytes(b"actual-lto")
        command = ["ld", "-lto_library", str(lto), "-syslibroot", str(sdk), "-o", str(self.root / "godot"),
                   str(self.obj), str(self.archive), str(self.archive), "-framework", "Metal", "-lSystem",
                   "-rpath", "@executable_path/../Frameworks"]
        inputs, arguments = identity.resolve_link_inputs(command, [self.obj], self.root / "godot", sdk)
        self.assertEqual([path.name for _, path in inputs], ["libLTO.dylib", "engine.o", "core.a", "core.a", "Metal.tbd", "libSystem.tbd"])
        self.assertEqual([kind for kind, _ in inputs], ["SharedLibrary", "Object", "StaticArchive", "StaticArchive", "SharedLibrary", "SharedLibrary"])
        self.assertIn("@executable_path/../Frameworks", arguments)

    def test_original_command_omission_and_indirect_inputs_refused(self):
        with self.assertRaises(ValueError):
            identity.resolve_link_inputs(["ld", "-o", str(self.root / "godot")], [self.obj], self.root / "godot", self.root)
        for token in ["@response.txt", "-filelist", "-force_load", "-T"]:
            with self.assertRaises(ValueError):
                identity.resolve_link_inputs(["ld", token], [], self.root / "godot", self.root)

    def test_literal_argument_cannot_impersonate_input_ordinal(self):
        self.assertNotEqual(identity.encode_snapshot(self.snapshot(arguments=["@input:0"])),
                            identity.encode_snapshot(self.snapshot(arguments=[{"kind": "Input", "ordinal": 0}])))

    def test_actual_option_policy_excludes_diagnostics_but_keeps_runtime_paths(self):
        left = {"num_jobs": "2", "extra_suffix": "old", "object_prefix": "old", "MACOS_SDK_PATH": "/old/sdk", "system_certs_path": "/runtime/certs", "tests": True}
        right = dict(left, num_jobs="8", extra_suffix="new", object_prefix="new", MACOS_SDK_PATH="/relocated/sdk")
        a, _ = identity.collect_options(left, left)
        b, _ = identity.collect_options(right, right)
        self.assertEqual(a, b)
        self.assertEqual(a["system_certs_path"], "/runtime/certs")
        self.assertEqual(identity.derive_stamps(self.snapshot(options=a)), identity.derive_stamps(self.snapshot(options=b)))

    def test_build_profile_option_tracks_content_not_installation_path(self):
        first, second = self.root / "profile1.json", self.root / "profile2.json"
        first.write_bytes(b'{"disable_3d":true}')
        second.write_bytes(first.read_bytes())
        a, deps = identity.collect_options(["build_profile"], {"build_profile": str(first)})
        b, _ = identity.collect_options(["build_profile"], {"build_profile": str(second)})
        self.assertEqual(a, b)
        self.assertEqual(deps, [first.resolve()])
        second.write_bytes(b'{"disable_3d":false}')
        c, _ = identity.collect_options(["build_profile"], {"build_profile": str(second)})
        self.assertNotEqual(a, c)

    def test_driver_expansion_uses_original_action_environment(self):
        compiler = self.root / "driver"
        compiler.write_text('#!/usr/bin/python3\nimport os,sys,shlex\nprint(\'"/usr/bin/ld" "--env" \' + shlex.quote(os.environ["CONTROLLED_ENV_TEST"]) + " " + " ".join(shlex.quote(a) for a in sys.argv[1:-1]),file=sys.stderr)\n')
        compiler.chmod(0o755)
        result = identity.expand_link_driver([str(compiler), "-o", str(self.root / "godot"), str(self.obj)], [self.obj], {"CONTROLLED_ENV_TEST": "constructed"})
        self.assertIn("constructed", result)
        self.assertIn(str(self.obj), result)
        self.assertNotIn("/dev/null", result)

    def test_actual_c_compiler_and_all_version_queries_use_action_environment(self):
        paths = {}
        for role in ["CC", "CXX", "LINK", "ld"]:
            path = self.root / role
            path.write_text('#!/usr/bin/python3\nimport os\nprint(os.environ["CONTROLLED_ENV_TEST"])\n# ' + role + '\n')
            path.chmod(0o755)
            paths[role] = str(path)
        class Env(dict):
            def subst(self, value):
                return self[value[1:]]
        tools, configuration = identity._tool_inputs(Env(paths), paths["ld"], {"CONTROLLED_ENV_TEST": "constructed", "PATH": "/usr/bin"})
        self.assertEqual([role for role, _ in tools], ["c_compiler", "cpp_compiler", "link_driver", "linker"])
        self.assertTrue(all(value == "constructed" for _, value in configuration))
        self.assertNotEqual(tools[0][1], tools[1][1])

    def test_actual_linker_version_switch_matches_tool(self):
        class Env(dict):
            def subst(self, value):
                return self[value[1:]]

        for linker_name, expected_switch in [("ld", "-v"), ("libtool", "-V")]:
            paths = {}
            for role in ["CC", "CXX", "LINK", linker_name]:
                path = self.root / role
                path.write_text("#!/usr/bin/python3\nimport sys\nprint(' '.join(sys.argv[1:]))\n")
                path.chmod(0o755)
                paths[role] = str(path)
            _, configuration = identity._tool_inputs(
                Env({"CC": paths["CC"], "CXX": paths["CXX"], "LINK": paths["LINK"]}),
                paths[linker_name],
                {"PATH": "/usr/bin"},
            )
            linker_version = dict(configuration)["linker.version"]
            self.assertEqual(linker_version, expected_switch)

    def test_shared_install_name_and_rpath_are_semantic_operands(self):
        output = self.root / "libgodot.dylib"
        for name in ["@rpath/libgodot.dylib", str(output)]:
            command = ["ld", "-dylib", "-o", str(output), "-install_name", name, "-rpath", "@loader_path/lib.dylib", str(self.obj)]
            inputs, arguments = identity.resolve_link_inputs(command, [self.obj], output, self.root)
            self.assertEqual(inputs, [("Object", self.obj.resolve())])
            self.assertIn(name, arguments)
            self.assertIn("@loader_path/lib.dylib", arguments)
        inputs, arguments = identity.resolve_link_inputs(["ld", "-dylib", "-o", str(output), str(self.obj)], [self.obj], output, self.root)
        self.assertEqual(inputs, [("Object", self.obj.resolve())])
        self.assertIn({"kind": "DefaultInstallName", "value": str(output)}, arguments)

    def test_actual_linker_strip_flag_is_closed_flag(self):
        output = self.root / "godot"
        for flag in ["-S", "-x"]:
            inputs, arguments = identity.resolve_link_inputs(
                ["ld", flag, "-o", str(output), str(self.obj)],
                [self.obj], output, self.root,
            )
            self.assertEqual(inputs, [("Object", self.obj.resolve())])
            self.assertIn(flag, arguments)

    def test_unimplemented_search_and_hidden_file_options_fail_before_publication(self):
        for token in ["-search_dylibs_first", "-static", "-Z", "-weak-lFoo", "-order_file", "-lazy_library", "-unknown_future_option"]:
            with self.assertRaises(ValueError):
                identity.resolve_link_inputs(["ld", "-o", str(self.root / "godot"), str(self.obj), token], [self.obj], self.root / "godot", self.root)

    def test_source_projection_is_closed_and_acyclic(self):
        snapshot = self.snapshot()
        source = identity.render_source(identity.selection_record(snapshot, *identity.derive_stamps(snapshot)))
        self.assertIn('"core/config/engine_build_identity.h"', source)
        self.assertNotIn("OS::", source)
        self.assertNotIn("get_executable_path", source)
        self.assertIn(identity.derive_stamps(snapshot)[0].hex(), source)


if __name__ == "__main__":
    unittest.main()
