#!/usr/bin/env python3
import importlib.util,json,pathlib,subprocess,sys,tempfile,hashlib
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).resolve().parent;ANGE=Path('/Users/macbookair/Documents/Work/ANGE');SDK=ANGE/'godot-cpp';R3=ROOT/'work/sdk-unloadable-active-context-20261028-r3'
spec=importlib.util.spec_from_file_location('prepare',OUT/'prepare.py');prep=importlib.util.module_from_spec(spec);spec.loader.exec_module(prep)
updates={str(ANGE/k):v[1] for k,v in prep.updates.items()}
with tempfile.TemporaryDirectory(prefix='k03-sdk-intake-') as tmp:
 s=Path(tmp)
 for p in ['include/godot_cpp/godot.hpp','include/godot_cpp/core/class_db.hpp','src/godot.cpp']:
  dest=s/p;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes((SDK/p).read_bytes())
 subprocess.run(['git','apply',str(R3/'SDK.patch')],cwd=s,check=True)
 cmd=['clang++','-std=c++17','-Werror=pointer-bool-conversion','-DHOT_RELOAD_ENABLED','-DTHREADS_ENABLED','-I'+str(s/'include'),'-I'+str(SDK/'include'),'-I'+str(SDK/'gen/include'),'-fsyntax-only',str(s/'src/godot.cpp'),str(SDK/'src/core/class_db.cpp')]
 with (OUT/'non-macos-branch.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 results={'non_macos_preprocessor_syntax':{'command':cmd,'rc':r.returncode,'scope':'host compiler with MACOS_ENABLED absent, not a foreign platform build'}}
 assert r.returncode==0
 # Generate current-target controlled source with all accessor intake changes applied in memory only.
 driver=ANGE/'tests/native/scene_ready_source/v2_receiver.py';sys.path.insert(0,str(driver.parent))
 module={'__file__':str(driver),'__name__':'private_receiver'}
 exec(compile(updates[str(driver)],str(driver),'exec'),module)
 original=pathlib.Path.read_text
 def read(path,*a,**kw):return updates[str(path)] if str(path) in updates else original(path,*a,**kw)
 pathlib.Path.read_text=read
 try:program,_=module['probe_source']()
 finally:pathlib.Path.read_text=original
 (s/'target.cpp').write_text(program)
 cmd=['clang++','-std=c++17','-DANGE_SCENE_READY_V2_TEST','-I'+str(SDK/'gen/include'),str(s/'target.cpp'),'-o',str(s/'target')]
 with (OUT/'target-receiver-build.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 results['current_target_receiver_build']={'command':cmd,'rc':r.returncode}
 (OUT/'CHECKS.json').write_text(json.dumps(results,indent=2)+'\n')
 print(results,flush=True)
 if r.returncode==0:
  with (OUT/'target-receiver.log').open('w') as f:r=subprocess.run([str(s/'target'),'0','0','0','1','0','0','0','0','73','941','1'],stdout=f,stderr=subprocess.STDOUT)
  results['current_target_base_control']={'rc':r.returncode};(OUT/'CHECKS.json').write_text(json.dumps(results,indent=2)+'\n')
  cases=[]
  for case in range(1,19):
   proc=subprocess.run([str(s/'target'),'0','0','0','1','0','0','0','0','73','941',str(case)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True)
   cases.append({'case':case,'rc':proc.returncode,'typed_abort':case!=14 or proc.stdout.startswith('unscheduled-pair-abort|')})
  results['current_v2_controls']=cases
  (OUT/'CHECKS.json').write_text(json.dumps(results,indent=2)+'\n')
  assert all(x['rc']==0 and x['typed_abort'] for x in cases),cases
  print('V2_CURRENT_CONTROLS_18_OK',flush=True)
 removed=sum(p.stat().st_size for p in s.rglob('*') if p.is_file())
(OUT/'CLEANUP.json').write_text(json.dumps({'temporary_bytes_deleted':removed,'processes':'all children reaped; no shared source edits','retained':'intake patch, source pins, runner/checks/logs'},indent=2)+'\n')
