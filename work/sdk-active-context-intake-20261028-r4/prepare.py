#!/usr/bin/env python3
import difflib,json,hashlib,subprocess,tempfile
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];OUT=Path(__file__).resolve().parent
ANGE=Path('/Users/macbookair/Documents/Work/ANGE');R3=ROOT/'work/sdk-unloadable-active-context-20261028-r3'
paths=['src/register_types.cpp','tests/native/device_exit_scene_ready_test.cpp','tests/native/scene_ready_source/v2_receiver.py','tests/native/scene_ready_source/verify_discriminators.py'];updates={}
for path in paths:
 text=(ANGE/path).read_text();new=text
 if path.endswith('register_types.cpp'):new=new.replace('GDExtensionBinding::active_guarded_user','GDExtensionBinding::active_guarded_user()')
 elif path.endswith('device_exit_scene_ready_test.cpp'):new=new.replace('B::active_guarded_user','B::active_guarded_user()')
 elif path.endswith('v2_receiver.py'):
  marker="    template=Path(__file__).with_name('probe.cpp.in').read_text()\n";assert new.count(marker)==1
  new=new.replace(marker,marker+'''    # Adapt only the current target receiver. The pinned baseline template and oracle remain byte-exact.
    template=template.replace('static InitData* active_guarded_user;', 'static InitData*& active_guarded_user();')
    template=template.replace('GDExtensionBinding::InitData* GDExtensionBinding::active_guarded_user=&binding;',
        'GDExtensionBinding::InitData*& GDExtensionBinding::active_guarded_user(){static InitData* value=&binding;return value;}')
    template=template.replace('GDExtensionBinding::active_guarded_user=nullptr;', 'GDExtensionBinding::active_guarded_user()=nullptr;')
    # Diagnostics are an opaque, inert boundary in this control-flow receiver.
    template=template.replace('struct Result {bool success;bool has_value() const {return success;}};', 'struct DiagnosticCause {};struct Result {bool success;bool has_value() const {return success;} DiagnosticCause error() const {return {};}};')
    template=template.replace('// @PINNED_SOURCE_FUNCTION@', 'template<class Cause> void report_scene_initialization_refusal(const char*,const Cause&) {}\\n// @PINNED_SOURCE_FUNCTION@')
''')
 else:
  marker="    results=[]\n";assert new.count(marker)==1
  new=new.replace(marker,"    if args.target:\n        before,after=mutations['reset-entry-ledger']\n        mutations['reset-entry-ledger']=(before.replace('active_guarded_user;', 'active_guarded_user();'), after.replace('active_guarded_user;', 'active_guarded_user();'))\n"+marker)
 assert new!=text,path
 updates[path]=(text,new)
patch=''.join(''.join(difflib.unified_diff(a.splitlines(True),b.splitlines(True),fromfile='a/'+p,tofile='b/'+p)) for p,(a,b) in updates.items())
(OUT/'ANGE_INTAKE.patch').write_text(patch)
subprocess.run(['git','-C',str(ANGE),'apply','--check','--whitespace=error',str(OUT/'ANGE_INTAKE.patch')],check=True)
# Syntax checks for migrated Python receivers without creating shared pycache.
for p,(_,new) in updates.items():
 if p.endswith('.py'):compile(new,p,'exec')
(OUT/'SOURCE_PINS.json').write_text(json.dumps({str(ANGE/p):hashlib.sha256(a.encode()).hexdigest() for p,(a,b) in updates.items()},indent=2)+'\n')
print('ANGE_INTAKE_APPLY_CHECK_OK; migrated Python receiver syntax OK; historical baseline/template untouched')
