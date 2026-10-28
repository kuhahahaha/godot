#!/usr/bin/env python3
import difflib,hashlib,json,os,subprocess,tempfile,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).resolve().parent
SDK=Path('/Users/macbookair/Documents/Work/ANGE/godot-cpp');BASE=ROOT/'work/native-wrapper-terminal-20261028-r1';HOST=ROOT/'bin/godot.macos.editor.dev.arm64'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[SDK/'include/godot_cpp/godot.hpp',SDK/'src/godot.cpp',SDK/'src/core/class_db.cpp',SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a',BASE/'probe.cpp',BASE/'probe.gdextension',BASE/'project.godot',HOST,Path(__file__)]
inputs+=sorted((SDK/'gen/include').rglob('*.h'))+sorted((SDK/'gen/include').rglob('*.hpp'));pins={str(p):sha(p) for p in inputs}
h=(SDK/'include/godot_cpp/godot.hpp').read_text();c=(SDK/'src/godot.cpp').read_text()
old='static thread_local InitData *active_guarded_user;';assert h.count(old)==1
h=h.replace(old,'static InitData *&active_guarded_user();')
old='thread_local GDExtensionBinding::InitData *GDExtensionBinding::active_guarded_user = nullptr;';assert c.count(old)==1
c=c.replace(old,'''GDExtensionBinding::InitData *&GDExtensionBinding::active_guarded_user() {
#if defined(MACOS_ENABLED)
    // Match Wrapped's unload-compatible per-thread storage on macOS. Native
    // TLS pins the binding image and prevents a fresh guarded reload.
    struct ActiveContextStore {
        std::mutex mutex;
        std::map<std::thread::id, InitData *> contexts;
        InitData *&get() {
            std::lock_guard<std::mutex> lock(mutex);
            return contexts[std::this_thread::get_id()];
        }
    };
    static ActiveContextStore store;
    return store.get();
#else
    static thread_local InitData *context = nullptr;
    return context;
#endif
}''')
c=c.replace('#include <godot_cpp/godot.hpp>','#include <godot_cpp/godot.hpp>\n#include <map>\n#include <mutex>')
# Replace all original uses, excluding the new definition.
import re
c=re.sub(r'\bactive_guarded_user\b(?!\s*\()', 'active_guarded_user()',c)
patch=''
for path,new in [('include/godot_cpp/godot.hpp',h),('src/godot.cpp',c)]:
 old=(SDK/path).read_text();patch+=''.join(difflib.unified_diff(old.splitlines(True),new.splitlines(True),fromfile='a/'+path,tofile='b/'+path))
class_path='include/godot_cpp/core/class_db.hpp'
old_class=(SDK/class_path).read_text();new_class=old_class.replace('GDExtensionBinding::active_guarded_user','GDExtensionBinding::active_guarded_user()')
patch+=''.join(difflib.unified_diff(old_class.splitlines(True),new_class.splitlines(True),fromfile='a/'+class_path,tofile='b/'+class_path))
(OUT/'SDK.patch').write_text(patch)
pins[str(SDK/class_path)]=sha(SDK/class_path)
consumer=Path('/Users/macbookair/Documents/Work/ANGE/src/register_types.cpp');original=consumer.read_text();updated=original.replace('GDExtensionBinding::active_guarded_user','GDExtensionBinding::active_guarded_user()')
(OUT/'ANGE_CONSUMER.patch').write_text(''.join(difflib.unified_diff(original.splitlines(True),updated.splitlines(True),fromfile='a/src/register_types.cpp',tofile='b/src/register_types.cpp')))
pins[str(consumer)]=sha(consumer)
with tempfile.TemporaryDirectory(prefix='k03-sdk-context-') as tmp:
 s=Path(tmp);(s/'include/godot_cpp').mkdir(parents=True);(s/'include/godot_cpp/godot.hpp').write_text(h);(s/'godot.cpp').write_text(c)
 (s/'include/godot_cpp/core').mkdir();(s/'include/godot_cpp/core/class_db.hpp').write_text(new_class)
 (s/'probe.cpp').write_text((BASE/'probe.cpp').read_text().replace('GDExtensionBinding::active_guarded_user->','GDExtensionBinding::active_guarded_user()->'))
 for f in ['probe.gdextension','project.godot']:shutil.copy2(BASE/f,s/f)
 (s/'.godot').mkdir();(s/'.godot/extension_list.cfg').write_text('res://probe.gdextension\n')
 (s/'probe.gd').write_text('''extends SceneTree
func _initialize():
 print("WRAPPER_LOOP_ENTERED")
 var a = GDExtensionManager.unload_extension("res://probe.gdextension")
 print("FIRST_UNLOAD=", a)
 var b = GDExtensionManager.load_extension("res://probe.gdextension")
 print("FRESH_RELOAD=", b)
 var c = GDExtensionManager.unload_extension("res://probe.gdextension")
 print("SECOND_UNLOAD=", c)
 quit(0 if a == 0 and b == 0 and c == 0 else 1)
''')
 common=['clang++','-std=c++17','-Werror=pointer-bool-conversion','-DMACOS_ENABLED','-DUNIX_ENABLED','-DHOT_RELOAD_ENABLED','-DTHREADS_ENABLED','-DDEBUG_ENABLED','-DNDEBUG','-I'+str(s/'include'),'-I'+str(SDK/'include'),'-I'+str(SDK/'gen/include')]
 cmd=common+['-dynamiclib',str(s/'probe.cpp'),str(s/'godot.cpp'),str(SDK/'src/core/class_db.cpp'),str(SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a'),'-framework','CoreFoundation','-Wl,-dead_strip','-o',str(s/'probe.dylib')]
 with (OUT/'build.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,'build'
 dso_sha=sha(s/'probe.dylib')
 run=[str(HOST),'--path',str(s),'--rendering-driver','metal','--rendering-method','mobile','--script','probe.gd']
 with (OUT/'native.log').open('w') as f:r=subprocess.run(run,env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=30)
 text=(OUT/'native.log').read_text();checks={'exit':r.returncode==0,'metal':'Metal ' in text and 'Apple M3 Pro' in text,'fresh_reload':'FRESH_RELOAD=0' in text,'both_unloads':'FIRST_UNLOAD=0' in text and 'SECOND_UNLOAD=0' in text,'two_trees':text.count('WRAPPER_FREE_RETURN role=tree ')==2,'two_engines':text.count('WRAPPER_FREE_RETURN role=engine ')==2,'two_destructors':text.count('WRAPPER_DSO_UNLOAD\n')==2,'first_close_order':text.find('WRAPPER_DSO_UNLOAD')<text.find('FIRST_UNLOAD=0'),'last_close_order':text.rfind('WRAPPER_DSO_UNLOAD')<text.find('SECOND_UNLOAD=0'),'clean':not any(x in text for x in ['ERROR:','leaked at exit','CrashHandler'])}
 (OUT/'RESULTS.json').write_text(json.dumps({'build_command':cmd,'command':run,'dso_sha256':dso_sha,'rc':r.returncode,'checks':checks},indent=2)+'\n');print(checks,flush=True)
 assert all(checks.values()),'native'
 # Test the actual accessor implementation under two simultaneously live contexts.
 (s/'threads.cpp').write_text('''#include <godot_cpp/godot.hpp>
#include <thread>
#include <atomic>
#include <cstdlib>
using B=godot::GDExtensionBinding;
int main(){B::InitData a,b;std::atomic<int> step{0};auto &main=B::active_guarded_user();if(main)std::abort();main=&a;
std::thread worker([&]{auto &local=B::active_guarded_user();if(local)std::abort();local=&b;step.store(1);while(step.load()!=2)std::this_thread::yield();if(local!=&b)std::abort();local=nullptr;});
while(step.load()!=1)std::this_thread::yield();if(main!=&a)std::abort();step.store(2);worker.join();if(main!=&a)std::abort();main=nullptr;if(B::active_guarded_user())std::abort();}
''')
 tc=common+[str(s/'threads.cpp'),str(s/'godot.cpp'),str(SDK/'src/core/class_db.cpp'),str(SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a'),'-framework','CoreFoundation','-Wl,-dead_strip','-o',str(s/'threads')]
 with (OUT/'threads-build.log').open('w') as f:r=subprocess.run(tc,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,'thread build'
 r=subprocess.run([str(s/'threads')]);assert r.returncode==0,'thread isolation';print('THREAD_ISOLATION_OK',flush=True)
 (OUT/'THREADS.json').write_text(json.dumps({'command':tc,'rc':r.returncode,'scope':'actual accessor simultaneous main/worker pointer isolation and reset; no core authority issued'},indent=2)+'\n')
 # Existing original SDK callback/reentry/return tests, adapted only for the accessor spelling.
 test=Path('/Users/macbookair/Documents/Work/ANGE/tests/native/device_exit_scene_ready_test.cpp')
 pins[str(test)]=sha(test)
 (s/'sdk_test.cpp').write_text(test.read_text().replace('B::active_guarded_user','B::active_guarded_user()'))
 tc=common+[str(s/'sdk_test.cpp'),str(s/'godot.cpp'),str(SDK/'src/core/class_db.cpp'),str(SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a'),'-framework','CoreFoundation','-Wl,-dead_strip','-o',str(s/'sdk_test')]
 with (OUT/'sdk-build.log').open('w') as f:r=subprocess.run(tc,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,'sdk build'
 with (OUT/'sdk.log').open('w') as f:r=subprocess.run([str(s/'sdk_test')],stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,'SDK original controls'
 print('ORIGINAL_SDK_CONTROLS_OK',flush=True)
 (OUT/'SDK_CONTROLS.json').write_text(json.dumps({'build_command':tc,'rc':r.returncode,'test_source':str(test),'adaptation':'active context variable spelling to accessor only'},indent=2)+'\n')
 # Preserve the original negative cleanup-enrollment gate on the candidate.
 (s/'probe.gd').write_text((BASE/'probe.gd').read_text())
 red=cmd.copy();red.insert(1,'-DOMIT_TREE_SCHEDULE')
 with (OUT/'negative-build.log').open('w') as f:r=subprocess.run(red,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,'negative build'
 with (OUT/'negative.log').open('w') as f:r=subprocess.run(run,env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=30)
 log=(OUT/'negative.log').read_text();assert r.returncode==78 and 'GDEXTENSION_CLOSE_FAILED error=030 route=5' in log and 'WRAPPER_FREE_RETURN' not in log and 'WRAPPER_DSO_UNLOAD' not in log,'negative gate'
 (OUT/'NEGATIVE.json').write_text(json.dumps({'command':run,'rc':r.returncode,'omission':'Tree original schedule','core_refusal':'error030 route5','no_frees':True,'no_dso_destructor':True},indent=2)+'\n')
 print('ORIGINAL_OMISSION_GATE_OK',flush=True)
 removed=sum(p.stat().st_size for p in s.rglob('*') if p.is_file())
assert all(sha(Path(p))==v for p,v in pins.items()),'inputs changed'
(OUT/'SOURCE_MANIFEST.json').write_text(json.dumps(pins,indent=2)+'\n');(OUT/'CLEANUP.json').write_text(json.dumps({'temporary_bytes_deleted':removed,'processes':'all child compilers/hosts/thread tests reaped','retained':'patches, logs, identities and runner; no shared SDK production edits'},indent=2)+'\n')
