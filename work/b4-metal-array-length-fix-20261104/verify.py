#!/usr/bin/env python3
"""Standalone C++/translator/source-slice checks; does not build or run Godot."""
from pathlib import Path
import subprocess,json,hashlib
ROOT=Path(__file__).resolve().parents[2];HERE=Path(__file__).resolve().parent
rows=[]
def run(command):
 p=subprocess.run(list(map(str,command)),cwd=ROOT,text=True,capture_output=True)
 rows.append({'command':list(map(str,command)),'returncode':p.returncode,'stdout':p.stdout,'stderr':p.stderr})
 (HERE/'BUILD_AND_RUN_RESULTS.json').write_text(json.dumps({'rows':rows,'engine_build':False,'gpu':False},indent=2)+'\n')
 if p.returncode:raise RuntimeError(p.stdout+p.stderr)
run(['python3',HERE/'extract_source_slices.py'])
run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-I.',HERE/'layout_test.cpp','-o',HERE/'layout_test'])
run(['clang++','-std=c++17','-O0','-I.','-Ithirdparty/spirv-cross','-Ithirdparty/spirv-headers/include/spirv/unified1',HERE/'translate_test.cpp',*[ROOT/'thirdparty/spirv-cross'/p for p in ('spirv_msl.cpp','spirv_glsl.cpp','spirv_cross.cpp','spirv_parser.cpp','spirv_cfg.cpp','spirv_cross_parsed_ir.cpp')],'-o',HERE/'translate_test'])
run(['python3',HERE/'run_checks.py'])
run(['python3',HERE/'cache_gate_test.py'])
print('Standalone checks passed; no Engine/compiler-runtime/GPU qualification.')
