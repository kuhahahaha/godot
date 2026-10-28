#!/usr/bin/env python3
"""Private TLS causality experiment. The non-TLS mutant is NOT a production fix."""
import hashlib,json,os,shutil,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).resolve().parent
BASE=ROOT/'work/native-wrapper-terminal-20261028-r1'
SDK=Path('/Users/macbookair/Documents/Work/ANGE/godot-cpp');HOST=ROOT/'bin/godot.macos.editor.dev.arm64'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
pins=[HOST,BASE/'probe.cpp',BASE/'probe.gd',BASE/'probe.gdextension',BASE/'project.godot',SDK/'include/godot_cpp/godot.hpp',SDK/'src/godot.cpp',SDK/'src/core/class_db.cpp',SDK/'include/godot_cpp/classes/wrapped.hpp',SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a',Path(__file__)]
pins += sorted((SDK/'gen/include').rglob('*.h')) + sorted((SDK/'gen/include').rglob('*.hpp'))
pre={str(p):sha(p) for p in pins};rows=[]
with tempfile.TemporaryDirectory(prefix='k03-wrapper-tls-') as scratch:
 s=Path(scratch)
 for file in ['probe.gd','probe.gdextension','project.godot']:shutil.copy2(BASE/file,s/file)
 (s/'.godot').mkdir();(s/'.godot/extension_list.cfg').write_text('res://probe.gdextension\n')
 for mode in ['original_tls','non_tls_causality_only','restored_tls']:
  inc=s/'include/godot_cpp';inc.mkdir(parents=True,exist_ok=True)
  header=(SDK/'include/godot_cpp/godot.hpp').read_text();cpp=(SDK/'src/godot.cpp').read_text()
  if mode=='non_tls_causality_only':
   old='static thread_local InitData *active_guarded_user;';assert header.count(old)==1
   header=header.replace(old,'static InitData *active_guarded_user;')
   old='thread_local GDExtensionBinding::InitData *GDExtensionBinding::active_guarded_user = nullptr;';assert cpp.count(old)==1
   cpp=cpp.replace(old,'GDExtensionBinding::InitData *GDExtensionBinding::active_guarded_user = nullptr;')
  (inc/'godot.hpp').write_text(header);(s/'godot.cpp').write_text(cpp)
  cmd=['clang++','-std=c++17','-dynamiclib','-DMACOS_ENABLED','-DUNIX_ENABLED','-DHOT_RELOAD_ENABLED','-DTHREADS_ENABLED','-DDEBUG_ENABLED','-DNDEBUG','-I'+str(s/'include'),'-I'+str(SDK/'include'),'-I'+str(SDK/'gen/include'),str(BASE/'probe.cpp'),str(s/'godot.cpp'),str(SDK/'src/core/class_db.cpp'),str(SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a'),'-framework','CoreFoundation','-Wl,-dead_strip','-o',str(s/'probe.dylib')]
  with (OUT/(mode+'-build.log')).open('w') as f:b=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
  assert b.returncode==0,mode
  symbols=subprocess.check_output(['nm','-m',str(s/'probe.dylib')],text=True)
  (OUT/(mode+'-tls-symbols.txt')).write_text('\n'.join(line for line in symbols.splitlines() if 'thread' in line.lower() or 'active_guarded_user' in line or 'tlv' in line.lower())+'\n')
  command=[str(HOST),'--path',str(s),'--rendering-driver','metal','--rendering-method','mobile','--script','probe.gd']
  with (OUT/(mode+'.log')).open('w') as f:r=subprocess.run(command,env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=30)
  text=(OUT/(mode+'.log')).read_text();u=text.find('WRAPPER_UNLOAD result=0');d=text.find('WRAPPER_DSO_UNLOAD')
  checks={'rc':r.returncode==0,'metal':'Metal ' in text and 'Apple M3 Pro' in text,'manager_success':u>=0,'destructor':d>=0,'tree_once':text.count('WRAPPER_FREE_RETURN role=tree ')==1,'engine_once':text.count('WRAPPER_FREE_RETURN role=engine ')==1}
  rows.append({'mode':mode,'build_command':cmd,'build_rc':b.returncode,'dso_sha256':sha(s/'probe.dylib'),'header_sha256':sha(inc/'godot.hpp'),'cpp_sha256':sha(s/'godot.cpp'),'command':command,'rc':r.returncode,'destructor_before_manager_return':0<=d<u,'checks':checks})
  (OUT/'RESULTS.json').write_text(json.dumps({'scope':'controlled source comparison; non-TLS mutant is unsafe cross-thread and not proposed for production','runs':rows},indent=2)+'\n')
  print(mode,r.returncode,'destructor_before_manager_return',0<=d<u,flush=True)
  assert all(checks.values()),mode
 removed=sum(p.stat().st_size for p in s.rglob('*') if p.is_file())
assert [row['destructor_before_manager_return'] for row in rows]==[False,True,False], 'TLS causality contrast absent'
assert '__tlv_bootstrap' in (OUT/'original_tls-tls-symbols.txt').read_text()
assert '__tlv_bootstrap' not in (OUT/'non_tls_causality_only-tls-symbols.txt').read_text()
assert '__tlv_bootstrap' in (OUT/'restored_tls-tls-symbols.txt').read_text()
assert pre=={str(p):sha(p) for p in pins},'original inputs changed'
(OUT/'SOURCE_MANIFEST.json').write_text(json.dumps(pre,indent=2)+'\n')
(OUT/'CLEANUP.json').write_text(json.dumps({'temporary_bytes_deleted':removed,'processes':'all child compilers/hosts terminal and reaped','retained':'raw logs, symbol subsets, exact commands/source hashes and experiment runner only'},indent=2)+'\n')
