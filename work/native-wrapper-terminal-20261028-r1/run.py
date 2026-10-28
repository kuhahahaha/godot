#!/usr/bin/env python3
"""Actual SDK/core wrapper-terminal control; no canonical ANGE owner claim."""
import hashlib,json,os,re,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
OUT=Path(__file__).resolve().parent
SDK=Path('/Users/macbookair/Documents/Work/ANGE/godot-cpp')
HOST=ROOT/'bin/godot.macos.editor.dev.arm64'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[HOST,OUT/'probe.cpp',OUT/'probe.gd',OUT/'probe.gdextension',OUT/'project.godot',Path(__file__),SDK/'src/godot.cpp',SDK/'src/core/class_db.cpp',SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a',SDK/'gen/include/gdextension_interface.h']
before={str(p):digest(p) for p in inputs}
(OUT/'.godot').mkdir(exist_ok=True)
(OUT/'.godot/extension_list.cfg').write_text('res://probe.gdextension\n')
results=[]
for label,extra in [('negative',['-DOMIT_TREE_SCHEDULE']),('positive',[])]:
 cmd=['clang++','-std=c++17','-dynamiclib',*extra,'-I'+str(SDK/'include'),'-I'+str(SDK/'gen/include'),str(OUT/'probe.cpp'),str(SDK/'src/godot.cpp'),str(SDK/'src/core/class_db.cpp'),str(SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a'),'-framework','CoreFoundation','-Wl,-dead_strip','-o',str(OUT/'probe.dylib')]
 with (OUT/(label+'-build.log')).open('w') as stream:build=subprocess.run(cmd,stdout=stream,stderr=subprocess.STDOUT)
 assert build.returncode==0,(label,'build',build.returncode)
 binary_sha=digest(OUT/'probe.dylib')
 run=[str(HOST),'--path',str(OUT),'--rendering-driver','metal','--rendering-method','mobile','--script','probe.gd']
 with (OUT/(label+'.log')).open('w') as stream:proc=subprocess.run(run,env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1'},stdout=stream,stderr=subprocess.STDOUT,timeout=30)
 text=(OUT/(label+'.log')).read_text()
 engine=re.search(r'WRAPPER_ENGINE_EARLY id=(\d+)',text);tree=re.search(r'WRAPPER_TREE_READY id=(\d+)',text)
 checks={'real_metal':'Metal ' in text and 'Apple M3 Pro' in text,'distinct_live_wrappers':bool(engine and tree and engine[1]!=tree[1]),'loop':text.count('WRAPPER_LOOP_ENTERED\n')==1}
 if label=='positive':
  checks.update(rc=proc.returncode==0,unload='WRAPPER_UNLOAD result=0' in text,dso_unload=text.count('WRAPPER_DSO_UNLOAD\n')==1,tree_exact_once=bool(tree) and text.count('WRAPPER_FREE_RETURN role=tree id='+tree[1]+'\n')==1,engine_exact_once=bool(engine) and text.count('WRAPPER_FREE_RETURN role=engine id='+engine[1]+'\n')==1,clean=not any(x in text for x in ['ERROR:','leaked at exit','CrashHandler','SIGSEGV']))
  if checks['tree_exact_once'] and checks['engine_exact_once']:checks['terminal_order']=text.index('WRAPPER_USER_RETURN level=2')<text.index('WRAPPER_FREE_RETURN role=tree')<text.index('WRAPPER_FREE_RETURN role=engine')<text.index('WRAPPER_UNLOAD result=0')
 else:checks.update(refused='WRAPPER_UNLOAD result=0' not in text and proc.returncode!=0,no_early_frees='WRAPPER_FREE_RETURN' not in text,exact_core_refusal=proc.returncode==78 and 'GDEXTENSION_CLOSE_FAILED error=030 route=5' in text,no_dso_unload='WRAPPER_DSO_UNLOAD' not in text)
 results.append({'label':label,'build_command':cmd,'build_rc':build.returncode,'dso_sha256':binary_sha,'command':run,'rc':proc.returncode,'checks':checks})
 print(label,proc.returncode,checks,flush=True)
 (OUT/'CHECKS.json').write_text(json.dumps({'runs':results},indent=2)+'\n')
 assert all(checks.values()),label
assert before=={str(p):digest(p) for p in inputs},'inputs changed'
(OUT/'SOURCE_MANIFEST.json').write_text(json.dumps(before,indent=2)+'\n')
