"""Controlled identity of original core build inputs, never a runtime authority.

Schema1 canonical byte grammar uses C01 BE scalars, counted UTF8 strings and
raw32 digests. Paths in input records are diagnostic only. Actual command order,
input order/duplicates and semantic command strings remain significant.
"""

import hashlib
import json
import pathlib
import struct

INPUT_KINDS = {"Object": 1, "StaticArchive": 2, "SharedLibrary": 3}
TARGET_KINDS = {"Executable": 1, "SharedLibrary": 2, "StaticLibrary": 3}


def _u32(value):
    return struct.pack(">I", value)


def _u64(value):
    return struct.pack(">Q", value)


def _text(value):
    raw = value.encode("utf8")
    return _u32(len(raw)) + raw


def schema_digest(domain, payload):
    return hashlib.sha256(_text(domain) + _u32(len(payload)) + payload).digest()


def _option(value):
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        return value
    if isinstance(value, int):
        return str(value)
    if value is None:
        return "null"
    if isinstance(value, (list, tuple)):
        # Counted strings are serialized separately; never repr or delimiter join.
        return [_option(item) for item in value]
    raise ValueError("unsupported controlled build-option value")


def _option_bytes(value):
    if isinstance(value, list):
        return _u32(2) + _u32(len(value)) + b"".join(_option_bytes(item) for item in value)
    return _u32(1) + _text(value)


def _content(path):
    data = pathlib.Path(path).read_bytes()
    return {"bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()}


def make_snapshot(
    *, platform, architecture, target_kind, inputs, arguments, options, tools, configuration, recipes, builtin_glslang
):
    if not builtin_glslang:
        raise ValueError("nonbuiltin runtime compiler identity is unavailable")
    if target_kind not in TARGET_KINDS or not platform or not architecture:
        raise ValueError("unknown controlled core target")
    records = []
    for ordinal, (kind, path) in enumerate(inputs):
        if kind not in INPUT_KINDS:
            raise ValueError("unknown original link-input kind")
        records.append(dict(kind=kind, ordinal=ordinal, **_content(path)))
    if not records:
        raise ValueError("missing original link inputs")
    return {
        "schema": 1,
        "role": "Core",
        "platform": platform,
        "architecture": architecture,
        "target_kind": target_kind,
        "compiler_route": "BuiltinGlslangV1",
        "inputs": records,
        "arguments": list(arguments),
        "options": {key: _option(options[key]) for key in sorted(options)},
        "tools": [
            {"role": role, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} for role, data in tools
        ],
        "configuration": list(configuration),
        "recipes": [
            {"role": role, "bytes": len(data), "sha256": hashlib.sha256(data).hexdigest()} for role, data in recipes
        ],
    }


def encode_snapshot(snapshot):
    if snapshot["schema"] != 1 or snapshot["role"] != "Core" or snapshot["compiler_route"] != "BuiltinGlslangV1":
        raise ValueError("unsupported core input snapshot")
    out = _u32(1) + _u32(1) + _text(snapshot["platform"]) + _text(snapshot["architecture"])
    out += _u32(TARGET_KINDS[snapshot["target_kind"]]) + _u32(1)
    out += _u32(len(snapshot["inputs"]))
    for record in snapshot["inputs"]:
        out += (
            _u32(INPUT_KINDS[record["kind"]])
            + _u32(record["ordinal"])
            + _u64(record["bytes"])
            + bytes.fromhex(record["sha256"])
        )
    out += _u32(len(snapshot["arguments"])) + b"".join(_argument_bytes(arg) for arg in snapshot["arguments"])
    out += _u32(len(snapshot["options"]))
    for key in sorted(snapshot["options"]):
        out += _text(key) + _option_bytes(snapshot["options"][key])
    for name in ("tools", "recipes"):
        out += _u32(len(snapshot[name]))
        for record in snapshot[name]:
            out += _text(record["role"]) + _u64(record["bytes"]) + bytes.fromhex(record["sha256"])
    out += _u32(len(snapshot["configuration"]))
    for key, value in snapshot["configuration"]:
        out += _text(key) + _text(value)
    return out


def derive_stamps(snapshot):
    core = schema_digest("ange.vfx.core-build-inputs.v1", encode_snapshot(snapshot))
    compiler = schema_digest("ange.vfx.runtime-compiler-build.v1", _u32(1) + core + _u32(1))
    if not any(core) or not any(compiler):
        raise ValueError("derived identity matches the missing-value sentinel")
    return core, compiler


def selection_record(snapshot, core, compiler):
    return {
        "schema": 1,
        "core_stamp": core.hex(),
        "compiler_route": "BuiltinGlslangV1",
        "compiler_stamp": compiler.hex(),
        "platform": snapshot["platform"],
        "architecture": snapshot["architecture"],
        "target_kind": snapshot["target_kind"],
    }


def validate_graph(inputs, stamp_object, final_target):
    inputs = [pathlib.Path(path).resolve() for path in inputs]
    if pathlib.Path(stamp_object).resolve() in inputs or pathlib.Path(final_target).resolve() in inputs:
        raise ValueError("controlled core input graph contains its stamp or final target")


def render_source(selection):
    # Stable hand-written extern header; ordinary Engine.o never includes values.
    fields = [str(selection["schema"])] + [
        json.dumps(selection[key])
        for key in ["core_stamp", "compiler_route", "compiler_stamp", "platform", "architecture", "target_kind"]
    ]
    return (
        "// Generated from original ordinary core link inputs.\n"
        '#include "core/config/engine_build_identity.h"\n\n'
        "const EngineBuildIdentity &get_engine_build_identity() {\n"
        "    static const EngineBuildIdentity identity = { " + ", ".join(fields) + " };\n"
        "    return identity;\n}\n"
    )


def _resolve_library(name, directories):
    # Darwin searches each selected directory for dylib/tbd before static archive.
    for directory in directories:
        for suffix in (".dylib", ".tbd", ".a"):
            candidate = pathlib.Path(directory) / ("lib" + name + suffix)
            if candidate.is_file():
                return candidate.resolve()
    raise ValueError("unresolved original link library: " + name)


def resolve_link_inputs(command, ordinary_sources, final_target, sdk):
    """Resolve expanded original macOS link operands before adding stamp object."""
    import os

    sdk = pathlib.Path(sdk)
    unsupported_search = {"-search_dylibs_first", "-static", "-Z"}
    if unsupported_search.intersection(command):
        raise ValueError("unsupported original native library search mode")
    flag_only = {
        "-demangle",
        "-dynamic",
        "-dylib",
        "-no_deduplicate",
        "-dead_strip",
        "-pie",
        "-no_pie",
        "-export_dynamic",
        "-ObjC",
        "-all_load",
        "-headerpad_max_install_names",
        "-search_paths_first",
        "-S",
        "-x",
        "-s",
        "-O0",
        "-O1",
        "-O2",
        "-O3",
        "-no_uuid",
    }
    scalar_options = {
        "-arch": 1,
        "-platform_version": 3,
        "-stack_size": 1,
        "-compatibility_version": 1,
        "-current_version": 1,
    }
    directories = []
    frameworks = []
    for index, token in enumerate(command):
        if token == "-L":
            directories.append(command[index + 1])
        elif token.startswith("-L"):
            directories.append(token[2:])
        elif token == "-F":
            frameworks.append(command[index + 1])
        elif token.startswith("-F"):
            frameworks.append(token[2:])
    directories += [str(sdk / "usr/lib"), "/usr/lib", "/usr/local/lib"]
    frameworks += [str(sdk / "System/Library/Frameworks"), "/Library/Frameworks", "/System/Library/Frameworks"]
    inputs, arguments = [], []
    ordinary = [pathlib.Path(path).resolve() for path in ordinary_sources]
    represented = set()

    def add(path, kind=None):
        path = pathlib.Path(path).resolve()
        if kind is None:
            kind = "Object" if path.suffix == ".o" else "StaticArchive" if path.suffix == ".a" else "SharedLibrary"
        ordinal = len(inputs)
        inputs.append((kind, path))
        represented.add(path)
        return {"kind": "Input", "ordinal": ordinal}

    index = 0
    while index < len(command):
        token = command[index]
        if index == 0:
            arguments.append({"kind": "Tool", "role": "linker"})
        elif token == "-o":
            if pathlib.Path(command[index + 1]).resolve() != pathlib.Path(final_target).resolve():
                raise ValueError("original link output does not match final module")
            arguments += [token, {"kind": "Output"}]
            index += 1
        elif token in flag_only:
            arguments.append(token)
        elif token in scalar_options:
            count = scalar_options[token]
            if index + count >= len(command):
                raise ValueError("missing native scalar option operands")
            arguments += command[index : index + count + 1]
            index += count
        elif token == "-mllvm":
            if index + 1 == len(command) or command[index + 1] != "-enable-linkonceodr-outlining":
                raise ValueError("unsupported native LLVM option input closure")
            arguments += command[index : index + 2]
            index += 1
        elif token in ("-install_name", "-rpath", "-L", "-F"):
            if index + 1 == len(command):
                raise ValueError("missing semantic native link option operand")
            arguments += [token, command[index + 1]]
            index += 1
        elif token in ("-isysroot", "--sysroot", "-syslibroot"):
            if pathlib.Path(command[index + 1]).resolve() != sdk.resolve():
                raise ValueError("original link SDK selection mismatch")
            arguments += [token, {"kind": "SDK"}]
            index += 1
        elif token in ("-framework", "-weak_framework"):
            name = command[index + 1]
            selected = None
            for base in frameworks:
                for suffix in (".tbd", ""):
                    path = pathlib.Path(base) / (name + ".framework") / (name + suffix)
                    if path.is_file():
                        selected = path
                        break
                if selected:
                    break
            if selected is None:
                raise ValueError("unresolved original framework: " + name)
            ref = add(selected, "SharedLibrary")
            arguments += [token, {"kind": "Framework", "name": name, "ordinal": ref["ordinal"]}]
            index += 1
        elif token == "-lto_library":
            arguments.append(token)
        elif token in ("-lazy_library", "-load_hidden"):
            raise ValueError("unsupported native library-selection option")
        elif token.startswith("-l") and len(token) > 2:
            arguments.append(add(_resolve_library(token[2:], directories)))
        elif pathlib.Path(token).resolve() in ordinary or pathlib.Path(token).suffix in (".o", ".a", ".dylib", ".tbd"):
            arguments.append(add(token))
        elif (
            (token.startswith("@") and command[index - 1] not in ("-rpath", "-install_name"))
            or "-filelist" in token
            or "-force_load" in token
            or "-sectcreate" in token
            or token in ("-T", "-dylib_file")
        ):
            # No silent omissions for response/script/indirect inputs. Their
            # complete expansion is an explicit unsupported build branch.
            raise ValueError("unexpanded original indirect link input: " + token)
        elif not token.startswith("-") and os.path.isfile(token):
            raise ValueError("unclassified original file operand: " + token)
        elif token.startswith("-L") or token.startswith("-F"):
            arguments.append(token)
        else:
            raise ValueError("unclosed original native linker operand: " + token)
        index += 1
    if "-dylib" in command and "-install_name" not in command:
        # Darwin ld derives LC_ID_DYLIB from -o when no explicit install name
        # exists. That operand is then semantic, not merely an output pathname.
        arguments.append({"kind": "DefaultInstallName", "value": command[command.index("-o") + 1]})
    if any(path not in represented for path in ordinary):
        raise ValueError("ordinary object omitted from expanded original link command")
    return inputs, arguments


def _argument_bytes(value):
    if isinstance(value, str):
        return _u32(1) + _text(value)
    kind = value["kind"]
    if kind == "Input":
        return _u32(2) + _u32(value["ordinal"])
    if kind == "Output":
        return _u32(3)
    if kind == "Tool":
        return _u32(4) + _text(value["role"])
    if kind == "Framework":
        return _u32(5) + _text(value["name"]) + _u32(value["ordinal"])
    if kind == "SDK":
        return _u32(6)
    if kind == "DefaultInstallName":
        return _u32(7) + _text(value["value"])
    raise ValueError("unknown controlled link argument kind")


def execution_environment(env):
    from SCons.Action import get_default_ENV
    from SCons.Util import sanitize_shell_env

    if env.get("SHELL_ENV_GENERATORS"):
        raise ValueError("controlled identity does not support dynamic shell environment generators")
    return sanitize_shell_env(get_default_ENV(env))


def _tool_paths(env, actual_linker, process_env):
    import shlex
    import shutil
    import subprocess

    tools = []
    for role, variable in (("c_compiler", "CC"), ("cpp_compiler", "CXX"), ("link_driver", "LINK")):
        tokens = shlex.split(env.subst("$" + variable))
        if len(tokens) != 1:
            raise ValueError("unresolved compiler/linker launcher chain")
        executable = shutil.which(tokens[0], path=process_env.get("PATH"))
        if executable is None:
            raise ValueError("missing actual compiler/link driver")
        tools.append((role, pathlib.Path(executable)))
        if pathlib.Path(executable).resolve().parent == pathlib.Path("/usr/bin"):
            selected = subprocess.check_output(
                ["/usr/bin/xcrun", "--find", pathlib.Path(executable).name], text=True, env=process_env
            ).strip()
            tools.append((role + ".selected", pathlib.Path(selected)))
    tools.append(("linker", pathlib.Path(actual_linker)))
    return tools


def _tool_inputs(env, actual_linker, process_env):
    import subprocess

    tools, configuration = [], []
    for role, path in _tool_paths(env, actual_linker, process_env):
        tools.append((role, path.read_bytes()))
        # Apple libtool and ld expose different version switches. Keep the
        # probe tied to the actual selected tool so shared/executable link
        # paths do not send libtool's `-V` to ld.
        if role == "linker":
            switch = "-V" if path.name == "libtool" else "-v"
        else:
            switch = "--version"
        version = subprocess.check_output([str(path), switch], text=True, env=process_env)
        # InstalledDir is diagnostic; selected executable content participates.
        version = "\n".join(line for line in version.splitlines() if not line.startswith("InstalledDir:"))
        configuration.append((role + ".version", version))
    return tools, configuration


DIAGNOSTIC_OPTIONS = {
    "verbose",
    "progress",
    "compiledb",
    "compiledb_file",
    "compiledb_gen_only",
    "generate_bundle",
    "bundle_sign_identity",
    "cache_path",
    "cache_limit",
    "cache_compress",
    "cache_verbose",
    "use_cache",
    "separate_debug_symbols",
    "num_jobs",
    "extra_suffix",
    "object_prefix",
    "vsproj",
    "vsproj_name",
    "vsproj_gen_only",
    "ninja",
    "ninja_file",
    "ninja_auto_run",
    "import_env_vars",
}
SELECTED_PATH_OPTIONS = {
    "CC",
    "CXX",
    "LINK",
    "MACOS_SDK_PATH",
    "vulkan_sdk_path",
    "accesskit_sdk_path",
    "angle_libs",
    "profiler_path",
    "custom_modules",
    "SWIFT_FRONTEND",
    "c_compiler_launcher",
    "cpp_compiler_launcher",
}


def collect_options(keys, values):
    options, file_dependencies = {}, []
    for key in sorted(keys):
        if key in DIAGNOSTIC_OPTIONS or key not in values:
            continue
        value = values[key]
        if key == "build_profile" and value:
            path = pathlib.Path(value)
            content = _content(path)
            options[key] = ["SelectedFileContent", str(content["bytes"]), content["sha256"]]
            file_dependencies.append(path.resolve())
        elif key in SELECTED_PATH_OPTIONS:
            # Compiler/SDK/library selection is retained by actual tool content,
            # SDK settings and expanded ordinary native link operands. Object
            # bytes cover source/header selections. Raw installation paths do not.
            options[key] = "OriginalSelectedInputs" if value else "Unselected"
        else:
            options[key] = _option(value)
    return options, file_dependencies


def install_macos(env, program):
    """Extend the existing final target with one acyclic generated source/object."""
    from SCons.Script import Action

    final = program[0]
    ordinary = list(final.sources)
    kind = {"executable": "Executable", "shared_library": "SharedLibrary", "static_library": "StaticLibrary"}[
        env["library_type"]
    ]
    if kind == "StaticLibrary" and env.get("builtin_glslang") and env.get("module_glslang_enabled"):
        # The normal macOS target archives platform objects only. Build one
        # original aggregate archive from that platform archive plus the
        # ordered static archives already selected in env.LIBS. Static uses
        # target-specific generated nodes so it cannot overwrite the fixed
        # executable/shared stamp or direct-selection records.
        import subprocess

        flattened = env.Flatten(env.get("LIBS", []))
        archive_inputs = [program[0]]
        for item in flattened:
            path = str(item)
            if path.endswith(".a"):
                archive_inputs.append(item if hasattr(item, "abspath") else env.File(path))
        if not archive_inputs:
            raise ValueError("static aggregate has missing archive inputs")
        process_env = execution_environment(env)
        libtool = subprocess.check_output(["/usr/bin/xcrun", "--find", "libtool"], text=True, env=process_env).strip()
        final_path = program[0].abspath.replace("libgodot.platform", "libgodot")
        variant = env["LIBSUFFIX"].replace(".", "_")
        generated = env.File("#core/engine_build_identity.static" + variant + ".gen.cpp")
        stamp = env.Object(generated)
        input_paths = [x.abspath for x in archive_inputs]
        arguments = [{"kind": "Tool", "role": "libtool"}, "-static", "-D", "-o", {"kind": "Output"}]
        arguments += [{"kind": "Input", "ordinal": i} for i in range(len(input_paths))]
        options, option_files = collect_options(env["CONTROLLED_BUILD_OPTION_KEYS"], env)
        sdk = pathlib.Path(env.subst("$MACOS_SDK_PATH"))
        settings = sdk / "SDKSettings.json"
        recipes = [
            env.File("#core/engine_build_identity.py"),
            env.File("#core/SCsub"),
            env.File("#platform/macos/SCsub"),
            env.File("#platform/macos/detect.py"),
            env.File("#SConstruct"),
            env.File("#methods.py"),
        ]
        recipe_paths = [(node.path, node.abspath) for node in recipes]
        values = {
            "arguments": arguments,
            "options": options,
            "platform": env["platform"],
            "architecture": env["arch"],
            "target_kind": kind,
            "builtin_glslang": True,
        }
        tools, tool_configuration = _tool_inputs(env, libtool, process_env)
        record = env.File("#core/engine_build_identity.static" + variant + ".gen.json")
        direct = env.File("#core/engine_build_selection.static" + variant + ".gen.json")

        def generate(target, source, env):
            configuration = list(tool_configuration)
            configuration.append(("sdk.settings.sha256", _content(settings)["sha256"]))
            snapshot = make_snapshot(
                **values,
                inputs=[("StaticArchive", path) for path in input_paths],
                tools=tools,
                configuration=configuration,
                recipes=[(role, pathlib.Path(path).read_bytes()) for role, path in recipe_paths],
            )
            core, compiler = derive_stamps(snapshot)
            selection = selection_record(snapshot, core, compiler)
            pathlib.Path(target[0].abspath).write_text(render_source(selection), encoding="utf8")
            pathlib.Path(target[1].abspath).write_text(
                json.dumps(
                    {
                        "snapshot": snapshot,
                        "selection": selection,
                        "diagnostic_inputs": input_paths,
                        "diagnostic_tools": [
                            (role, str(path)) for role, path in _tool_paths(env, libtool, process_env)
                        ],
                        "diagnostic_recipes": recipe_paths,
                    },
                    indent=2,
                )
                + "\n",
                encoding="utf8",
            )
            pathlib.Path(target[2].abspath).write_text(json.dumps(selection, indent=2) + "\n", encoding="utf8")
            return 0

        deps = (
            [env.File(path) for path in input_paths]
            + recipes
            + [env.File(str(settings)), env.Value(json.dumps(values, sort_keys=True)), env.File(libtool)]
        )
        deps += [env.File(str(path)) for path in option_files]
        deps += [env.File(str(path)) for _, path in _tool_paths(env, libtool, process_env)]
        env.Command([generated, record, direct], deps, env.Run(generate))

        def aggregate_action(target, source, env):
            return subprocess.run(
                [libtool, "-static", "-D", "-o", target[0].abspath] + [node.abspath for node in source], env=process_env
            ).returncode

        aggregate = env.Command(final_path, archive_inputs + [stamp], env.Run(aggregate_action))
        env["_CONTROLLED_FINAL_PROGRAM"] = aggregate
        return record
    if not (env.get("builtin_glslang") and env.get("module_glslang_enabled")):
        # External compiler identity is still an explicit unsupported branch.
        reason = "nonbuiltin runtime compiler identity unavailable"
        generated = env.File("#core/engine_build_identity.gen.cpp")
        record = env.File("#core/engine_build_identity.gen.json")
        direct = env.File("#core/engine_build_selection.gen.json")

        def unavailable(target, source, env):
            selection = {
                "schema": 0,
                "core_stamp": "",
                "compiler_stamp": "",
                "compiler_route": "Unavailable",
                "platform": "macos",
                "architecture": env["arch"],
                "target_kind": kind,
            }
            pathlib.Path(target[0].abspath).write_text(render_source(selection), encoding="utf8")
            pathlib.Path(target[1].abspath).write_text(
                json.dumps({"status": "unavailable", "reason": reason}, indent=2) + "\n", encoding="utf8"
            )
            pathlib.Path(target[2].abspath).write_text(
                json.dumps({"schema": 0, "status": "unavailable", "reason": reason}) + "\n", encoding="utf8"
            )
            return 0

        env.Command([generated, record, direct], env.Value(reason + env["arch"]), env.Run(unavailable))
        builder = env.SharedObject if kind == "SharedLibrary" else env.Object
        stamp = builder(generated)
        final.add_source(stamp)
        final.get_executor().add_sources(stamp)
        return record
    command_variable = "$SHLINKCOM" if kind == "SharedLibrary" else "$LINKCOM"
    command = [str(token) for token in env.subst_list(command_variable, target=program, source=ordinary)[0]]
    sdk = pathlib.Path(env.subst("$MACOS_SDK_PATH"))
    process_env = execution_environment(env)
    command = expand_link_driver(command, [node.abspath for node in ordinary], process_env)
    inputs, arguments = resolve_link_inputs(command, [node.abspath for node in ordinary], final.abspath, sdk)
    generated = env.File("#core/engine_build_identity.gen.cpp")
    # The redirected object name is determined by the original object builder.
    builder = env.SharedObject if kind == "SharedLibrary" else env.Object
    stamp = builder(generated)
    validate_graph([path for _, path in inputs], stamp[0].abspath, final.abspath)
    options, option_files = collect_options(env["CONTROLLED_BUILD_OPTION_KEYS"], env)
    settings = sdk / "SDKSettings.json"
    recipes = [
        env.File("#core/engine_build_identity.py"),
        env.File("#core/SCsub"),
        env.File("#platform/macos/SCsub"),
        env.File("#platform/macos/detect.py"),
        env.File("#SConstruct"),
        env.File("#methods.py"),
    ]
    # Only immutable path strings enter action closures. Capturing a mutable
    # SCons Node would make action signatures depend on graph/cache state.
    recipe_paths = [(node.path, node.abspath) for node in recipes]
    # Value dependency captures configuration changes even when input bytes match.
    values = {
        "arguments": arguments,
        "options": options,
        "platform": env["platform"],
        "architecture": env["arch"],
        "target_kind": kind,
        "builtin_glslang": bool(env.get("builtin_glslang") and env.get("module_glslang_enabled")),
    }

    diagnostic_graph = {
        "original_sources": [node.abspath for node in ordinary],
        "stamp_object": stamp[0].abspath,
        "final_target": final.abspath,
        "native_link_command_before_stamp": command,
    }

    def generate(target, source, env):
        tools, configuration = _tool_inputs(env, command[0], process_env)
        configuration.append(("sdk.settings.sha256", _content(settings)["sha256"]))
        snapshot = make_snapshot(
            **values,
            inputs=inputs,
            tools=tools,
            configuration=configuration,
            recipes=[(role, pathlib.Path(path).read_bytes()) for role, path in recipe_paths],
        )
        core, compiler = derive_stamps(snapshot)
        selection = selection_record(snapshot, core, compiler)
        pathlib.Path(target[0].abspath).write_text(render_source(selection), encoding="utf8")
        pathlib.Path(target[1].abspath).write_text(
            json.dumps(
                {
                    "snapshot": snapshot,
                    "selection": selection,
                    "diagnostic_inputs": [str(path) for _, path in inputs],
                    "diagnostic_tools": [(role, str(path)) for role, path in _tool_paths(env, command[0], process_env)],
                    "diagnostic_recipes": recipe_paths,
                    "diagnostic_graph": diagnostic_graph,
                },
                indent=2,
            )
            + "\n",
            encoding="utf8",
        )
        pathlib.Path(target[2].abspath).write_text(json.dumps(selection, indent=2) + "\n", encoding="utf8")
        return 0

    record = env.File("#core/engine_build_identity.gen.json")
    direct = env.File("#core/engine_build_selection.gen.json")
    deps = (
        [env.File(str(path)) for _, path in inputs]
        + recipes
        + [env.File(str(settings)), env.Value(json.dumps(values, sort_keys=True))]
    )
    # Actual construction ENV and actual CC/CXX/driver/ld selections also
    # invalidate generation; never discover them in the Python host ENV.
    deps += [env.File(str(path)) for path in option_files]
    deps += [env.File(str(path)) for _, path in _tool_paths(env, command[0], process_env)]
    env.Command(
        [generated, record, direct], deps, Action(generate, "Generating actual core controlled-build identity ...")
    )
    final.add_source(stamp)
    final.get_executor().add_sources(stamp)
    return record


def attach_final_receipt(env, program, record):
    from SCons.Script import Action

    output = program[0].abspath + ".controlled_build.json"
    record_path = record.abspath

    def receipt(target, source, env):
        metadata = json.loads(pathlib.Path(record_path).read_text(encoding="utf8"))
        value = {
            "schema": 1,
            "selection": metadata.get("selection"),
            "status": metadata.get("status", "available"),
            "reason": metadata.get("reason"),
            "artifact": _content(target[0].abspath),
        }
        pathlib.Path(output).write_text(json.dumps(value, indent=2) + "\n", encoding="utf8")
        return 0

    env.AddPostAction(program, Action(receipt, "Recording final core artifact bytes ..."))
    env.SideEffect(output, program)
    env.Clean(program, output)


def expand_link_driver(command, ordinary_sources, process_env):
    """Ask the selected original clang driver for its actual ld invocation.

    Existing /dev/null operands stand in for not-yet-built direct objects only
    during -### (no execution); restore exact originals one-for-one afterward.
    This exposes implicit libc++/System/compiler-rt/LTO inputs to the snapshot.
    """
    import shlex
    import subprocess

    ordinary = {pathlib.Path(path).resolve() for path in ordinary_sources}
    substituted, originals = [command[0]], []
    semantic_options = {
        "-o",
        "-install_name",
        "-rpath",
        "-L",
        "-F",
        "-isysroot",
        "--sysroot",
        "-syslibroot",
        "-framework",
        "-weak_framework",
    }
    for index, token in enumerate(command[1:], 1):
        semantic = command[index - 1] in semantic_options
        if not semantic and (
            pathlib.Path(token).resolve() in ordinary
            or (not token.startswith("-") and pathlib.Path(token).suffix in (".o", ".a", ".dylib", ".tbd"))
        ):
            originals.append(token)
            substituted.append("/dev/null")
        else:
            substituted.append(token)
    result = subprocess.run([*substituted, "-###"], capture_output=True, text=True, env=process_env)
    if result.returncode:
        raise ValueError("selected original link driver expansion failed: " + result.stderr)
    candidates = []
    for line in result.stderr.splitlines():
        if not line.lstrip().startswith('"'):
            continue
        tokens = shlex.split(line)
        if tokens and pathlib.Path(tokens[0]).name in ("ld", "ld-classic", "ld64.lld"):
            candidates.append(tokens)
    if len(candidates) != 1:
        raise ValueError("missing or ambiguous original native linker invocation")
    native = candidates[0]
    if native.count("/dev/null") != len(originals):
        raise ValueError("link driver changed ordinary operand multiplicity")
    iterator = iter(originals)
    return [next(iterator) if token == "/dev/null" else token for token in native]
