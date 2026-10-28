#!/usr/bin/env python3
import difflib,hashlib,json,os,subprocess,tempfile,shutil
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).resolve().parent
SDK=Path('/Users/macbookair/Documents/Work/ANGE/godot-cpp');BASE=ROOT/'work/native-wrapper-terminal-20261028-r1';HOST=ROOT/'bin/godot.macos.editor.dev.arm64'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
inputs=[SDK/'include/godot_cpp/godot.hpp',SDK/'src/godot.cpp',SDK/'src/core/class_db.cpp',SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a',BASE/'probe.cpp',BASE/'probe.gdextension',BASE/'project.godot',HOST,Path(__file__)]
inputs+=sorted((SDK/'gen/include').rglob('*.h'))+sorted((SDK/'gen/include').rglob('*.hpp'));pins={str(p):sha(p) for p in inputs}
h=(SDK/'include/godot_cpp/godot.hpp').read_text();c=(SDK/'src/godot.cpp').read_text();new_class=(SDK/'include/godot_cpp/core/class_db.hpp').read_text()
pins[str(SDK/'include/godot_cpp/core/class_db.hpp')]=sha(SDK/'include/godot_cpp/core/class_db.hpp')
with tempfile.TemporaryDirectory(prefix='k03-sdk-context-') as tmp:
 s=Path(tmp);(s/'include/godot_cpp').mkdir(parents=True);(s/'include/godot_cpp/godot.hpp').write_text(h);(s/'godot.cpp').write_text(c)
 (s/'include/godot_cpp/core').mkdir();(s/'include/godot_cpp/core/class_db.hpp').write_text(new_class)
 wrapped=(SDK/'include/godot_cpp/classes/wrapped.hpp').read_text();assert wrapped.count('#if defined(MACOS_ENABLED) && defined(HOT_RELOAD_ENABLED)')==2
 fixed=wrapped.replace('#if defined(MACOS_ENABLED) && defined(HOT_RELOAD_ENABLED)','#if defined(MACOS_ENABLED)')
 (s/'include/godot_cpp/classes').mkdir();(s/'include/godot_cpp/classes/wrapped.hpp').write_text(fixed)
 shutil.copy2(SDK/'src/classes/wrapped.cpp',s/'wrapped.cpp')
 old='        if (p_level == init_data->minimum_initialization_level) guarded_claimant = nullptr;'
 assert c.count(old)==1
 fixed_c=c.replace(old,'        // Claimant remains sticky until the actual binding image is unloaded.')
 (s/'godot.cpp').write_text(fixed_c)
 patch=''.join(difflib.unified_diff(wrapped.splitlines(True),fixed.splitlines(True),fromfile='a/include/godot_cpp/classes/wrapped.hpp',tofile='b/include/godot_cpp/classes/wrapped.hpp'))
 patch+=''.join(difflib.unified_diff(c.splitlines(True),fixed_c.splitlines(True),fromfile='a/src/godot.cpp',tofile='b/src/godot.cpp'))
 (OUT/'SDK.patch').write_text(patch)
 source=(BASE/'probe.cpp').read_text().replace('GDExtensionBinding::active_guarded_user->','GDExtensionBinding::active_guarded_user()->')
 source=source.replace('#include <godot_cpp/classes/engine.hpp>', '#include <godot_cpp/classes/engine.hpp>\n#include <godot_cpp/classes/node.hpp>\n#include <godot_cpp/core/class_db.hpp>')
 source=source.replace('namespace {', '''class SDKReloadNode : public godot::Node {
 GDCLASS(SDKReloadNode, godot::Node)
protected:
 static void _bind_methods() {godot::ClassDB::bind_method(godot::D_METHOD("value"), &SDKReloadNode::value);}
public:
 int value() const {return 41;}
 ~SDKReloadNode() {std::puts("SDK_NODE_DESTROY");std::fflush(stdout);}
};
namespace {''',1)
 source=source.replace('engine_id=Engine::get_singleton()->get_instance_id();', 'GDREGISTER_CLASS(SDKReloadNode);\n engine_id=Engine::get_singleton()->get_instance_id();')
 (s/'probe.cpp').write_text(source);(OUT/'probe.cpp').write_text(source)
 for f in ['probe.gdextension','project.godot']:shutil.copy2(BASE/f,s/f)
 (s/'.godot').mkdir();(s/'.godot/extension_list.cfg').write_text('res://probe.gdextension\n')
 (s/'probe.gd').write_text('''extends SceneTree
func _initialize():
 print("WRAPPER_LOOP_ENTERED")
 var first: Object = ClassDB.instantiate("SDKReloadNode")
 print("SDK_NODE_VALUE=", first.call("value"))
 first.free()
 var a = GDExtensionManager.unload_extension("res://probe.gdextension")
 print("FIRST_UNLOAD=", a)
 var b = GDExtensionManager.load_extension("res://probe.gdextension")
 print("FRESH_RELOAD=", b)
 if b != 0:
  quit(1)
  return
 var second: Object = ClassDB.instantiate("SDKReloadNode")
 print("SDK_NODE_VALUE=", second.call("value"))
 second.free()
 var c = GDExtensionManager.unload_extension("res://probe.gdextension")
 print("SECOND_UNLOAD=", c)
 quit(0 if a == 0 and b == 0 and c == 0 else 1)
''')
 common=['clang++','-std=c++17','-Werror=pointer-bool-conversion','-DMACOS_ENABLED','-DUNIX_ENABLED','-DTHREADS_ENABLED','-DDEBUG_ENABLED','-DNDEBUG','-I'+str(s/'include'),'-I'+str(SDK/'include'),'-I'+str(SDK/'gen/include')]
 cmd=common+['-dynamiclib',str(s/'probe.cpp'),str(s/'godot.cpp'),str(s/'wrapped.cpp'),str(SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a'),'-framework','CoreFoundation','-Wl,-dead_strip','-o',str(s/'probe.dylib')]
 with (OUT/'build.log').open('w') as f:r=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 assert r.returncode==0,'build'
 dso_sha=sha(s/'probe.dylib')
 run=[str(HOST),'--path',str(s),'--rendering-driver','metal','--rendering-method','mobile','--script','probe.gd']
 with (OUT/'native.log').open('w') as f:r=subprocess.run(run,env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=30)
 text=(OUT/'native.log').read_text();checks={'class_method_twice':text.count('SDK_NODE_VALUE=41')==2,'class_destroy_twice':text.count('SDK_NODE_DESTROY\n')==2,'exit':r.returncode==0,'metal':'Metal ' in text and 'Apple M3 Pro' in text,'fresh_reload':'FRESH_RELOAD=0' in text,'both_unloads':'FIRST_UNLOAD=0' in text and 'SECOND_UNLOAD=0' in text,'two_trees':text.count('WRAPPER_FREE_RETURN role=tree ')==2,'two_engines':text.count('WRAPPER_FREE_RETURN role=engine ')==2,'two_destructors':text.count('WRAPPER_DSO_UNLOAD\n')==2,'first_close_order':text.find('WRAPPER_DSO_UNLOAD')<text.find('FIRST_UNLOAD=0'),'last_close_order':text.rfind('WRAPPER_DSO_UNLOAD')<text.find('SECOND_UNLOAD=0'),'clean':not any(x in text for x in ['ERROR:','leaked at exit','CrashHandler'])}
 (OUT/'RESULTS.json').write_text(json.dumps({'build_command':cmd,'command':run,'dso_sha256':dso_sha,'rc':r.returncode,'checks':checks},indent=2)+'\n');print(checks,flush=True)
 assert all(checks.values()),'native'
 # Retain one real OS image reference to distinguish same-image from fresh-image reload.
 retained_source=source.replace('#include <cstdio>', '#include <cstdio>\n#include <dlfcn.h>')
 retained_source=retained_source.replace('namespace {', 'namespace {\nvoid image_anchor() {}\nvoid* retained_native_image=nullptr;',1)
 retained_source=retained_source.replace('GDREGISTER_CLASS(SDKReloadNode);', 'Dl_info info{};if(!dladdr(reinterpret_cast<void*>(&image_anchor),&info))std::abort();retained_native_image=dlopen(info.dli_fname,RTLD_NOW);if(!retained_native_image)std::abort();std::puts("SDK_NATIVE_IMAGE_RETAINED");\n GDREGISTER_CLASS(SDKReloadNode);')
 (s/'probe.cpp').write_text(retained_source)
 with (OUT/'retained-build.log').open('w') as f:b=subprocess.run(cmd,stdout=f,stderr=subprocess.STDOUT)
 assert b.returncode==0
 with (OUT/'retained-native.log').open('w') as f:n=subprocess.run(run,env={**os.environ,'ANGE_MACOS_BACKGROUND_RENDER':'1'},stdout=f,stderr=subprocess.STDOUT,timeout=30)
 log=(OUT/'retained-native.log').read_text()
 checks={'exit_rejected':n.returncode==1,'native_ref_once':log.count('SDK_NATIVE_IMAGE_RETAINED\n')==1,'first_logical_close_ok':'FIRST_UNLOAD=0' in log,'same_image_reload_refused':'FRESH_RELOAD=1' in log,'only_one_instance':log.count('SDK_NODE_VALUE=41')==1 and log.count('SDK_NODE_DESTROY\n')==1,'no_second_terminator':log.count('WRAPPER_USER_RETURN level=2')==1,'no_destructor_before_first_close':log.find('WRAPPER_DSO_UNLOAD')>log.find('FIRST_UNLOAD=0')}
 (OUT/'RETAINED.json').write_text(json.dumps({'command':run,'rc':n.returncode,'checks':checks,'scope':'real extra dlopen ref test-only, unsupported same-image init correctly rejected; no production OS-ref service'},indent=2)+'\n');print(checks,flush=True)
 assert all(checks.values()),'retained image'
 # Original scripted SDK frozen/reentry/return controls against the private sticky source.
 test=Path('/Users/macbookair/Documents/Work/ANGE/tests/native/device_exit_scene_ready_test.cpp')
 command=common+[str(test),str(s/'godot.cpp'),str(s/'wrapped.cpp'),str(SDK/'bin/libgodot-cpp.macos.template_debug.arm64.a'),'-framework','CoreFoundation','-Wl,-dead_strip','-o',str(s/'sdk_test')]
 with (OUT/'sdk-build.log').open('w') as f:b=subprocess.run(command,stdout=f,stderr=subprocess.STDOUT)
 assert b.returncode==0
 with (OUT/'sdk.log').open('w') as f:n=subprocess.run([str(s/'sdk_test')],stdout=f,stderr=subprocess.STDOUT)
 assert n.returncode==0
 (OUT/'SDK.json').write_text(json.dumps({'command':command,'rc':n.returncode,'scope':'original scripted SDK callback/reentry/return controls'},indent=2)+'\n')
 removed=sum(p.stat().st_size for p in s.rglob('*') if p.is_file())
assert all(sha(Path(p))==v for p,v in pins.items()),'inputs changed'
(OUT/'SOURCE_MANIFEST.json').write_text(json.dumps(pins,indent=2)+'\n');(OUT/'CLEANUP.json').write_text(json.dumps({'temporary_bytes_deleted':removed,'processes':'all child compilers/hosts/thread tests reaped','retained':'patches, logs, identities and runner; no shared SDK production edits'},indent=2)+'\n')
