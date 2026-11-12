#!/usr/bin/env python3
"""Exercise CPU particle scheduling source slices, not an engine or GPU runtime.

Extracts the real interval, activation, seed, age and finished statements. The
shell mocks containers/signals and omits transforms, force/material/curve work,
real RNG initialization, threading and rendering. Lifetime randomness is zero.
No candidate patch is synthesized or applied: current sources must contain it.
Generated C++ and executables live only in a temporary directory.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile


def replace_once(source: str, old: str, new: str) -> str:
    if source.count(old) != 1:
        raise ValueError(f"Expected one mutation anchor, found {source.count(old)}: {old!r}")
    return source.replace(old, new, 1)


def harness(s,dim):
    method=s.index(f'void CPUParticles{dim.upper()}::_particles_process')
    a=s.index('\tp_delta *= speed_scale;',method);b=s.index('\n\tTransform',a);trans=s[a:b]
    a=s.index('\tconst double emission_time = ',b) if '\tconst double emission_time = ' in s[b:] else s.index('\tdouble system_phase = ',b)
    b=s.index('\n\t\t\tp.active = true;',a)+len('\n\t\t\tp.active = true;');interval=s[a:b]
    seed_start=s.index('\t\t\tp.seed = ',b);seed_end=s.index('\n',seed_start);seed=s[seed_start:seed_end]
    # Actual reset and lifetime assignments, ignoring transform-only reset between them.
    age_start=s.index('\t\t\tp.time = 0;',seed_end);age_end=s.index('\n',s.index('\t\t\tp.lifetime = ',age_start));age=s[age_start:age_end]
    age_tail_start=s.index('\t\t} else if (!p.active) {',age_end)
    age_tail_end=s.index('\n\t\t\tp.custom[1]',age_tail_start)
    age_tail=s[age_tail_start:age_tail_end]
    # Actual should_be_active and finished suffix (motion/forces omitted).
    finish_start=s.index('\t\tshould_be_active = true;',age_tail_end);finish_end=s.index('\n}\n',finish_start)
    finish=s[finish_start:finish_end]
    ha=s.index('static uint32_t idhash(');hb=s.index('\n}\n',ha)+3
    expected_seed = '100+2*i' if dim=='2d' else '101+i'
    expected_last_seed = '114' if dim=='2d' else '108'
    code = '''#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>
namespace Math { using std::fmod; bool is_equal_approx(double a,double b) { return std::abs(a-b)<.00001; } }
struct SceneStringName { constexpr SceneStringName(int=0) {} constexpr operator int() const { return 0; } };
'''+s[ha:hb]+'''
struct Particle { bool active=false; double time=0,lifetime=.875,custom[4]{0,0,0,1}; unsigned seed=0; };
struct Particles : std::vector<Particle> { using std::vector<Particle>::vector; Particle *ptrw() {return data();} };
struct Emitter {
 Particles particles{8}; int births[8]{}; double birth_delta[8]{}; unsigned birth_seed[8]{};
 double speed_scale=1,time=0,lifetime=.875,randomness_ratio=0,explosiveness_ratio=0;
 uint32_t seed=100,cycle=0; bool emitting=true,one_shot=true,fractional_delta=true,active=true;
 int finished=0; bool tail_seen_with_public_emitting=false;
 void set_emitting(bool value) {emitting=value;} void notify_property_list_changed() {}
 void emit_signal(int) {finished++;}
 void step(double p_delta) {
'''+trans+interval+'''
'''+seed+age+'''
            births[i]++; birth_delta[i]=local_delta;birth_seed[i]=p.seed;
            if(i==7 && time==.125) tail_seen_with_public_emitting=emitting;
'''+age_tail+'''
        } // Remaining force, curve, appearance, and transform calculations omitted.
'''+finish+'''
 }
};
int failures=0,checks=0;
void check(bool value,const char *name) {checks++;if(!value){failures++;std::fprintf(stderr,"FAIL %s\\n",name);}}
int count(const Emitter&e){int n=0;for(int b:e.births)n+=b;return n;}
int main() {
 for(bool fractional:{false,true}) {
 Emitter e;e.fractional_delta=fractional;
 for(int n=0;n<4;n++)e.step(.25);
 check(count(e)==8 && e.births[7]==1,"old-cycle tail born");
 check(e.births[0]==1 && e.births[1]==1,"no next-cycle births");
 check(e.birth_seed[7]==EXPECTED_LAST_SEED,"birth initializer uses old cycle seed");
 check(e.birth_delta[7]==(fractional?.234375:.25),"fractional delta mode");
 check(!e.emitting && e.cycle==1 && !e.tail_seen_with_public_emitting,"public emission stopped before tail activation");
 // Actual extracted age/deactivation and finished suffix are used for drain.
 for(int n=0;n<12;n++)e.step(.125);
 check(!e.active && e.finished==1 && count(e)==8,"drain and one finished signal");
 }
 {Emitter e;e.lifetime=1;e.time=.5;e.step(.5);check(e.births[7]==1 && e.emitting && e.cycle==0,"exact endpoint keeps tail");e.step(.125);check(count(e)==4 && !e.emitting,"exact endpoint next step has no tail");}
 {Emitter e;e.lifetime=.25;e.step(.25);check(count(e)==8 && e.emitting,"first whole exact cycle emits");e.step(.25);check(count(e)==8 && !e.emitting,"next exact-cycle step no extra births");}
 for(int mode=0;mode<3;mode++){Emitter e;e.time=.75;if(mode==0)e.emitting=false;if(mode==1)e.speed_scale=0;e.step(mode==2?0:.25);check(count(e)==0,"already stopped zero delta speed zero");}
 {Emitter e;e.one_shot=false;for(int n=0;n<4;n++)e.step(.25);check(e.births[7]==1 && e.births[0]==2 && e.emitting,"continuous interval unchanged");}
 for(double randomness:{0.0,1.0})for(double explosive:{0.0,.5,1.0}){
 Emitter e;e.randomness_ratio=randomness;e.explosiveness_ratio=explosive;for(int n=0;n<4;n++)e.step(.25);
 check(count(e)==8,"randomness explosiveness full first-cycle count");
 for(int i=0;i<8;i++)check(e.births[i]==1 && e.birth_seed[i]==uint32_t(EXPECTED_SEED),"first-cycle initialization seed");
 // Independent expected i7 phase uses fixed public hash for cycle zero, not interval logic.
 double restart=(7.0/8+randomness*double(idhash(7)%65536)/65536/8)*(1-explosive)*.875;
 if(restart>=.75)check(e.birth_delta[7]==1-restart,"old-cycle restart hash selects correct fractional delta");
 }
 {Emitter e;e.time=.75;e.cycle=7;e.particles[0].active=true;e.particles[0].time=2;
 e.step(.25);check(e.births[0]==0 && !e.particles[0].active && e.births[7]==1,"age-forced restart does not gain tail permission");
 check(e.birth_seed[7]==uint32_t(EXPECTED_PREVIOUS_CYCLE_SEED),"nonzero previous cycle initializer seed");}
 {Emitter e;e.time=.75;e.emitting=false;e.particles[7].active=true;e.particles[7].time=.1;e.step(.25);
 check(e.births[7]==0,"already stopped active particles do not gain tail permission");}
 {Emitter e;e.time=.75;e.step(0);e.step(.25);check(e.births[7]==1,"zero delta does not consume tail");}
 {Emitter e;e.time=.75;e.speed_scale=0;e.step(.25);e.speed_scale=1;e.step(.25);check(e.births[7]==1,"speed-zero pause does not consume tail");}
 std::printf("CPU source interval/seed/age/finished checks=%d failures=%d\\n",checks,failures);return failures?1:0;
}
'''

    return code.replace('EXPECTED_LAST_SEED',expected_last_seed).replace('EXPECTED_SEED',expected_seed).replace('EXPECTED_PREVIOUS_CYCLE_SEED','121' if dim=='2d' else '164')


def mutations(source: str, dimension: str) -> dict[str, str]:
    initializer = ("p.seed = seed + uint32_t(i) + i + emission_cycle;" if dimension == "2d"
                   else "p.seed = seed + uint32_t(1) + i + emission_cycle * pcount;")
    changed_initializer = initializer.replace("emission_cycle", "cycle")
    # The >= control changes both the final-step predicate and wrap transition.
    if source.count("time > lifetime") != 2:
        raise ValueError("Expected exactly two strict lifetime transition anchors")
    return {
        "wrong-birth-seed": replace_once(source, initializer, changed_initializer),
        "wrong-restart-seed": replace_once(source, "uint32_t _seed = emission_cycle;", "uint32_t _seed = cycle;"),
        "short-fractional-delta": replace_once(source, "(final_one_shot_step ? advanced_time : time) - restart_time", "emission_time - restart_time"),
        "allow-next-cycle": replace_once(source, "final_one_shot_step ? lifetime : time", "time"),
        "block-tail": replace_once(source, "if (!emitting && !tail_birth)", "if (!emitting)"),
        "greater-equal": source.replace("time > lifetime", "time >= lifetime"),
        "age-gets-tail-permission": replace_once(source, "const bool tail_birth = final_one_shot_step && restart;", "const bool tail_birth = final_one_shot_step;"),
    }


def invoke(command: list[str], *, cwd: Path, timeout: float) -> dict:
    result = {"command": command, "exit": None, "stdout": "", "stderr": "", "timed_out": False}
    try:
        run = subprocess.run(command, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        result.update(exit=run.returncode, stdout=run.stdout, stderr=run.stderr)
    except subprocess.TimeoutExpired as exc:
        def decoded(value):
            return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
        result.update(stdout=decoded(exc.stdout), stderr=decoded(exc.stderr), timed_out=True)
    except OSError as exc:
        result["stderr"] = f"{type(exc).__name__}: {exc}"
    return result


def sha256(source: str) -> str:
    return hashlib.sha256(source.encode()).hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-root", type=Path, default=Path(__file__).resolve().parents[4])
    parser.add_argument("--baseline-ref", default="afafb0968c")
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    root = args.source_root.expanduser().resolve()
    output = args.output.expanduser().resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    receipt = {"scope": "source-slice-only; not engine/GPU acceptance", "source_root": str(root),
               "baseline_ref": args.baseline_ref, "baseline_commit": None, "compiler": [],
               "script_sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
               "sources": [], "results": [], "errors": []}
    try:
        compiler = shlex.split(os.environ.get("CXX", "c++"))
        if not compiler:
            raise ValueError("CXX must name a compiler")
        receipt["compiler"] = compiler
        revision = invoke(["git", "rev-parse", "--verify", args.baseline_ref + "^{commit}"], cwd=root, timeout=30)
        receipt["revision_command"] = revision
        if revision["exit"] != 0:
            raise ValueError("Cannot resolve baseline commit")
        commit = revision["stdout"].strip()
        receipt["baseline_commit"] = commit
        with tempfile.TemporaryDirectory(prefix="godot-cpu-tail-source-") as temporary:
            temporary_path = Path(temporary)
            for dimension in ("2d", "3d"):
                relative = f"scene/{dimension}/cpu_particles_{dimension}.cpp"
                command = ["git", "show", f"{commit}:{relative}"]
                baseline = invoke(command, cwd=root, timeout=30)
                if baseline["exit"] != 0:
                    receipt["errors"].append(f"{dimension}: cannot read baseline: {baseline['stderr']}")
                    continue
                before = baseline["stdout"]
                current = (root / relative).read_text()
                receipt["sources"].append({"dimension": dimension, "path": relative,
                    "baseline_command": command, "baseline_stderr": baseline["stderr"],
                    "baseline_sha256": sha256(before), "current_sha256": sha256(current)})
                variants = {"before-red": before, "current-green": current, **mutations(current, dimension)}
                for name, source in variants.items():
                    row = {"dimension": dimension, "variant": name, "source_path": relative,
                           "source_sha256": sha256(source), "expected_exit": 0 if name == "current-green" else 1,
                           "errors": [], "passed": False}
                    receipt["results"].append(row)
                    generated = harness(source, dimension)
                    row["slice_sha256"] = sha256(generated)
                    cpp = temporary_path / f"{dimension}-{name}.cpp"
                    executable = cpp.with_suffix(".bin")
                    cpp.write_text(generated)
                    row["compile"] = invoke(compiler + ["-std=c++17", "-O0", "-Wall", "-Wextra", str(cpp), "-o", str(executable)], cwd=root, timeout=60)
                    if row["compile"]["exit"] != 0:
                        row["errors"].append("Compilation did not succeed; behavior result is not RED evidence")
                        continue
                    row["behavior"] = invoke([str(executable)], cwd=root, timeout=15)
                    behavior = row["behavior"]
                    match = re.fullmatch(r"CPU source interval/seed/age/finished checks=(\d+) failures=(\d+)\n?", behavior["stdout"])
                    if not match:
                        row["errors"].append("Missing or malformed behavior summary")
                    else:
                        row.update(checks=int(match[1]), failures=int(match[2]))
                        if row["checks"] != 81:
                            row["errors"].append("Expected exactly 81 checks")
                        if (row["failures"] == 0) != (row["expected_exit"] == 0):
                            row["errors"].append("Failure count disagrees with expected RED/GREEN")
                    if behavior["exit"] != row["expected_exit"]:
                        row["errors"].append(f"Unexpected behavior exit: {behavior['exit']}")
                    row["passed"] = not row["errors"]
                    print(f"{dimension} {name}: {'PASS' if row['passed'] else 'FAIL'}; checks={row.get('checks')}; failures={row.get('failures')}; exit={behavior['exit']}")
    except (OSError, ValueError, IndexError) as exc:
        receipt["errors"].append(f"{type(exc).__name__}: {exc}")
    if len(receipt["results"]) != 18:
        receipt["errors"].append(f"Expected 18 variants, found {len(receipt['results'])}")
    receipt["passed"] = not receipt["errors"] and all(row["passed"] for row in receipt["results"])
    output.write_text(json.dumps(receipt, indent=2) + "\n")
    print(f"CPU particle source slices: {'PASS' if receipt['passed'] else 'FAIL'}; receipt={output}")
    return 0 if receipt["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
