#!/usr/bin/env python3
"""Actual cull helpers versus literal policy; never GPU winding qualification."""
from pathlib import Path
import hashlib,json,re,subprocess,tempfile
HERE=Path(__file__).resolve().parent;ROOT=HERE.parents[1]
shader=ROOT/'servers/rendering/renderer_rd/shaders/instance192_inc.glsl';text=shader.read_text()
functions=[]
for name in ('instance192_front_facing','instance192_discard_face'):
 match=re.search(r'bool '+name+r'\([^)]*\) \{.*?\n\}',text,re.S);assert match;functions.append(match[0])
literals=[(False,False,False,(False,False,True)),(False,True,True,(False,True,False)),(True,False,True,(False,True,False)),(True,True,False,(False,False,True))]
rows=[];runs=[]
for uber in (False,True):
 calls=[]
 for raw,mirror,logical,discards in literals:
  for mode,discard in enumerate(discards):
   flags=5 if mirror else 1;static=(mode+1)%3 if uber else mode;dynamic=mode if uber else (mode+1)%3
   rows.append({'uber':uber,'raster_front':raw,'record_mirror':mirror,'flags':flags,'logical_front':logical,'spec4':static,'dynamic_uc_cull':dynamic,'discard':discard})
   calls.append(f'instance192_cull_mode={static}u; dynamic_cull={dynamic}u; check(instance192_front_facing({str(raw).lower()},{flags}u),{str(logical).lower()}); check(instance192_discard_face({str(logical).lower()}),{str(discard).lower()});')
 prefix=('#define UBERSHADER\n' if uber else '')+'#include <cstdint>\n#include <iostream>\nusing uint=uint32_t; uint instance192_cull_mode=0,dynamic_cull=0;uint uc_cull_mode(){return dynamic_cull;}\n'
 source=prefix+'\n'.join(functions)+'\nint main(){unsigned checks=0,failures=0; auto check=[&](bool actual,bool expected){++checks;if(actual!=expected){++failures;std::cerr<<"CULL_LITERAL_MISMATCH actual="<<actual<<" expected="<<expected<<"\\n";}};\n'+'\n'.join(calls)+'\nstd::cout<<"12 literal rows / "<<checks<<" helper assertions, failures="<<failures<<" (CPU only)\\n";return failures?1:0;}\n'
 variants=[('current',source)]
 if uber:variants.append(('old_always_spec4',source.replace('uint material_cull = uc_cull_mode();','uint material_cull = instance192_cull_mode;')))
 for variant,code in variants:
  with tempfile.TemporaryDirectory(prefix='instance192-cull-') as directory:
   p=Path(directory)/'literal.cpp';p.write_text(code);binary=Path(directory)/'literal';command=['clang++','-std=c++17',str(p),'-o',str(binary)]
   build=subprocess.run(command,text=True,capture_output=True);assert build.returncode==0,build.stderr
   run=subprocess.run([str(binary)],text=True,capture_output=True)
   if variant=='current':assert run.returncode==0,run.stderr
   else:assert run.returncode==1 and 'CULL_LITERAL_MISMATCH' in run.stderr
   runs.append({'uber':uber,'variant':variant,'command':command,'build_returncode':build.returncode,'run_returncode':run.returncode,'stdout':run.stdout,'stderr':run.stderr,'expected_red':variant!='current'})
   print('uber' if uber else 'specialized',variant,run.returncode,run.stdout.strip())
(HERE/'cull-literal-results.json').write_text(json.dumps({'shader':str(shader),'shader_sha256':hashlib.sha256(shader.read_bytes()).hexdigest(),'literal_rows':rows,'runs':runs,'gpu_winding_calibration':False,'qualification':False},indent=2)+'\n')
